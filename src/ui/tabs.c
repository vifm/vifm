/* vifm
 * Copyright (C) 2018 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "tabs.h"

#include <assert.h> /* assert() */
#include <string.h> /* memmove() */

#include "../cfg/config.h"
#include "../modes/view.h"
#include "../utils/darray.h"
#include "../utils/filter.h"
#include "../utils/matcher.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../opt_handlers.h"
#include "../status.h"
#include "ui.h"

/* Pane-specific tab (contains information about only one view). */
typedef struct
{
	view_t view;       /* Buffer holding state of the view when it's hidden. */
	preview_t preview; /* Information about state of the quickview. */
	char *name;        /* Name of the tab.  Might be NULL. */
}
pane_tab_t;

/* Collection of pane tabs. */
typedef struct
{
	pane_tab_t *tabs;        /* List of tabs. */
	DA_INSTANCE_FIELD(tabs); /* Declarations to enable use of DA_* on tabs. */
	int current;             /* Index of the current tab. */
}
pane_tabs_t;

/* Global tab (contains both views as well as layout). */
typedef struct
{
	pane_tabs_t left;  /* Collection of tabs for the left pane. */
	pane_tabs_t right; /* Collection of tabs for the right pane. */

	int active_pane;   /* 0 -- left, 1 -- right. */
	int only_mode;     /* Whether in single-pane mode. */
	SPLIT split;       /* State of window split. */
	int splitter_pos;  /* Splitter position. */
	preview_t preview; /* Information about state of the quickview. */
	char *name;        /* Name of the tab.  Might be NULL. */
}
global_tab_t;

static int tabs_new_global(const char name[]);
static pane_tab_t * tabs_new_pane(pane_tabs_t *ptabs, view_t *view,
		const char name[]);
static void clone_view(view_t *dst, view_t *src);
static void tabs_goto_pane(int idx);
static void tabs_goto_global(int idx);
static void capture_global_state(global_tab_t *gtab);
static void assign_preview(preview_t *dst, const preview_t *src);
static void free_global_tab(global_tab_t *gtab);
static void free_pane_tabs(pane_tabs_t *ptabs);
static void free_pane_tab(pane_tab_t *ptab);
static void free_preview(preview_t *preview);
static pane_tabs_t * get_pane_tabs(const view_t *view);
static int count_pane_visitors(const pane_tabs_t *ptabs, const char path[],
		const view_t *view);
static void normalize_pane_tabs(const pane_tabs_t *ptabs, view_t *view);

/* List of global tabs. */
static global_tab_t *gtabs;
/* Declarations to enable use of DA_* on gtabs. */
static DA_INSTANCE(gtabs);
/* Index of current global tab. */
static int current_tab;

void
tabs_init(void)
{
	const int result = tabs_new_global(NULL);
	assert(result == 0 && "Failed to initialize first tab.");
	(void)result;
}

int
tabs_new(const char name[])
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		pane_tab_t *const ptab = tabs_new_pane(ptabs, curr_view, name);
		if(ptab == NULL)
		{
			return 1;
		}

		ptab->preview.on = curr_stats.preview.on;
		tabs_goto(ptab - ptabs->tabs);
		return 0;
	}

	return tabs_new_global(name);
}

/* Creates new global tab with the specified name, which might be NULL.  Returns
 * zero on success, otherwise non-zero is returned. */
static int
tabs_new_global(const char name[])
{
	global_tab_t new_tab = {};

	if(DA_EXTEND(gtabs) == NULL)
	{
		return 1;
	}

	if(tabs_new_pane(&new_tab.left, &lwin, NULL) == NULL ||
			tabs_new_pane(&new_tab.right, &rwin, NULL) == NULL)
	{
		free_global_tab(&new_tab);
		return 1;
	}
	update_string(&new_tab.name, name);
	capture_global_state(&new_tab);

	DA_COMMIT(gtabs);

	/* We're called from tabs_init(). */
	if(DA_SIZE(gtabs) == 1U)
	{
		gtabs[0U] = new_tab;
		return 0;
	}

	memmove(gtabs + current_tab + 2, gtabs + current_tab + 1,
			sizeof(*gtabs)*(DA_SIZE(gtabs) - (current_tab + 2)));
	gtabs[current_tab + 1] = new_tab;
	tabs_goto(current_tab + 1);
	return 0;
}

/* Creates new tab with the specified name, which might be NULL.  Returns newly
 * created tab on success or NULL on error. */
static pane_tab_t *
tabs_new_pane(pane_tabs_t *ptabs, view_t *view, const char name[])
{
	pane_tab_t new_tab = {};

	if(DA_EXTEND(ptabs->tabs) == NULL)
	{
		return NULL;
	}

	DA_COMMIT(ptabs->tabs);

	/* We're called from tabs_init() and just need to create internal structures
	 * without cloning data (or it will leak). */
	if(DA_SIZE(gtabs) == 0U)
	{
		ptabs->tabs[0] = new_tab;
		return &ptabs->tabs[0];
	}

	clone_view(&new_tab.view, view);
	update_string(&new_tab.name, name);

	if(DA_SIZE(ptabs->tabs) == 1U)
	{
		ptabs->tabs[0] = new_tab;
		return &ptabs->tabs[0];
	}

	memmove(ptabs->tabs + ptabs->current + 2, ptabs->tabs + ptabs->current + 1,
			sizeof(*ptabs->tabs)*(DA_SIZE(ptabs->tabs) - (ptabs->current + 2)));
	ptabs->tabs[ptabs->current + 1] = new_tab;

	return &ptabs->tabs[ptabs->current + 1];
}

/* Clones one view into another.  The destination view is assumed to not own any
 * resources. */
static void
clone_view(view_t *dst, view_t *src)
{
	strcpy(dst->curr_dir, flist_get_dir(src));
	dst->timestamps_mutex = src->timestamps_mutex;
	dst->win = src->win;
	dst->title = src->title;

	flist_init_view(dst);
	dst->dir_entry[0].origin = src->curr_dir;

	clone_local_options(src, dst, 1);
	matcher_free(dst->manual_filter);
	dst->manual_filter = matcher_clone(src->manual_filter);
	filter_assign(&dst->auto_filter, &src->auto_filter);
	dst->prev_invert = src->prev_invert;
	dst->invert = src->invert;

	/* Clone current entry even though we populate file list later to give
	 * reloading reference point for cursor. */
	replace_dir_entries(dst, &dst->dir_entry, &dst->list_rows,
			get_current_entry(src), 1);
	dst->list_pos = 0;
	/* Clone viewport configuration. */
	dst->curr_line = src->curr_line;
	dst->top_line = src->top_line;
	dst->window_rows = src->window_rows;
	dst->window_cols = src->window_cols;
	dst->window_cells = src->window_cells;

	flist_hist_resize(dst, cfg.history_len);
	flist_hist_clone(dst, src);

	(void)populate_dir_list(dst, 1);
	flist_update_origins(dst, &dst->curr_dir[0], &src->curr_dir[0]);
}

void
tabs_rename(view_t *view, const char name[])
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(view);
		update_string(&ptabs->tabs[ptabs->current].name, name);;
	}
	else
	{
		global_tab_t *const gtab = &gtabs[current_tab];
		update_string(&gtab->name, name);
	}
}

void
tabs_goto(int idx)
{
	if(cfg.pane_tabs)
	{
		tabs_goto_pane(idx);
	}
	else
	{
		tabs_goto_global(idx);
	}
}

/* Switches to pane tab specified by its index if the index is valid. */
static void
tabs_goto_pane(int idx)
{
	pane_tabs_t *const ptabs = get_pane_tabs(curr_view);

	if(ptabs->current == idx)
	{
		return;
	}

	if(idx < 0 || idx >= (int)DA_SIZE(ptabs->tabs))
	{
		return;
	}

	ptabs->tabs[ptabs->current].view = *curr_view;
	assign_preview(&ptabs->tabs[ptabs->current].preview, &curr_stats.preview);
	*curr_view = ptabs->tabs[idx].view;
	assign_preview(&curr_stats.preview, &ptabs->tabs[idx].preview);
	ptabs->current = idx;

	ui_view_schedule_redraw(curr_view);

	load_view_options(curr_view);

	(void)vifm_chdir(flist_get_dir(curr_view));
}

/* Switches to global tab specified by its index if the index is valid. */
static void
tabs_goto_global(int idx)
{
	if(current_tab == idx)
	{
		return;
	}

	if(idx < 0 || idx >= (int)DA_SIZE(gtabs))
	{
		return;
	}

	gtabs[current_tab].left.tabs[gtabs[current_tab].left.current].view = lwin;
	gtabs[current_tab].right.tabs[gtabs[current_tab].right.current].view = rwin;
	capture_global_state(&gtabs[current_tab]);

	lwin = gtabs[idx].left.tabs[gtabs[idx].left.current].view;
	rwin = gtabs[idx].right.tabs[gtabs[idx].right.current].view;
	if(gtabs[idx].active_pane != (curr_view == &rwin))
	{
		swap_view_roles();
	}
	curr_stats.number_of_windows = (gtabs[idx].only_mode ? 1 : 2);
	curr_stats.split = gtabs[idx].split;
	curr_stats.splitter_pos = gtabs[idx].splitter_pos;
	assign_preview(&curr_stats.preview, &gtabs[idx].preview);

	current_tab = idx;

	ui_view_schedule_redraw(&lwin);
	ui_view_schedule_redraw(&rwin);

	load_view_options(curr_view);

	(void)vifm_chdir(flist_get_dir(curr_view));
}

/* Records global state into a global tab structure. */
static void
capture_global_state(global_tab_t *gtab)
{
	gtab->active_pane = (curr_view == &rwin);
	gtab->only_mode = (curr_stats.number_of_windows == 1);
	gtab->split = curr_stats.split;
	gtab->splitter_pos = curr_stats.splitter_pos;
	assign_preview(&gtab->preview, &curr_stats.preview);
}

/* Assigns one instance of preview_t to another managing dynamic resources on
 * the way. */
static void
assign_preview(preview_t *dst, const preview_t *src)
{
	*dst = *src;

	/* Memory allocation can fail here, but this will just mess up terminal a
	 * bit, which isn't really a problem given that we're probably out of
	 * memory. */
	update_string(&dst->cleanup_cmd, src->cleanup_cmd);
}

int
tabs_quit_on_close(void)
{
	return (tabs_count(curr_view) == 1);
}

void
tabs_close(void)
{
	// XXX: FUSE filesystems aren't exited this way, but this might be OK because
	//      usually we exit from them on explicit ".." by a user.

	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		pane_tab_t *const ptab = &ptabs->tabs[ptabs->current];
		const int n = (int)DA_SIZE(ptabs->tabs);
		if(n != 1)
		{
			tabs_goto(ptabs->current + (ptabs->current == n - 1 ? -1 : +1));
			if(ptabs->current > ptab - ptabs->tabs)
			{
				--ptabs->current;
			}
			free_pane_tab(ptab);
			DA_REMOVE(ptabs->tabs, ptab);
		}
	}
	else
	{
		global_tab_t *const gtab = &gtabs[current_tab];
		const int n = (int)DA_SIZE(gtabs);
		if(n != 1)
		{
			tabs_goto(current_tab == n - 1 ? current_tab - 1 : current_tab + 1);
			if(current_tab > gtab - gtabs)
			{
				--current_tab;
			}
			free_global_tab(gtab);
			DA_REMOVE(gtabs, gtab);
		}
	}
}

/* Frees resources owned by a global tab. */
static void
free_global_tab(global_tab_t *gtab)
{
	free_pane_tabs(&gtab->left);
	free_pane_tabs(&gtab->right);
	free_preview(&gtab->preview);
	free(gtab->name);
}

/* Frees resources owned by a collection of pane tabs. */
static void
free_pane_tabs(pane_tabs_t *ptabs)
{
	size_t i;
	for(i = 0U; i < DA_SIZE(ptabs->tabs); ++i)
	{
		free_pane_tab(&ptabs->tabs[i]);
	}
	DA_REMOVE_ALL(ptabs->tabs);
}

/* Frees resources owned by a pane tab. */
static void
free_pane_tab(pane_tab_t *ptab)
{
	flist_free_view(&ptab->view);
	free_preview(&ptab->preview);
	free(ptab->name);
}

/* Frees resources owned by a preview state structure. */
static void
free_preview(preview_t *preview)
{
	update_string(&preview->cleanup_cmd, NULL);
	view_info_free(preview->explore);
}

void
tabs_next(int n)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		const int count = (int)DA_SIZE(ptabs->tabs);
		tabs_goto((ptabs->current + n)%count);
	}
	else
	{
		tabs_goto((current_tab + 1)%(int)DA_SIZE(gtabs));
	}
}

void
tabs_previous(int n)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		const int count = (int)DA_SIZE(ptabs->tabs);
		tabs_goto((ptabs->current + count - n)%count);
	}
	else
	{
		const int count = DA_SIZE(gtabs);
		tabs_goto((current_tab + count - n)%count);
	}
}

int
tabs_get(view_t *view, int idx, tab_info_t *tab_info)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(view);
		const int n = (int)DA_SIZE(ptabs->tabs);
		if(idx < 0 || idx >= n)
		{
			return 0;
		}
		tab_info->view = (idx == ptabs->current ? view : &ptabs->tabs[idx].view);
		tab_info->name = ptabs->tabs[idx].name;
		tab_info->last = (idx == n - 1);
		return 1;
	}
	else
	{
		global_tab_t *gtab;
		const int n = (int)DA_SIZE(gtabs);
		if(idx < 0 || idx >= n)
		{
			return 0;
		}

		gtab = &gtabs[idx];
		tab_info->name = gtab->name;
		tab_info->last = (idx == n - 1);

		if(idx == current_tab)
		{
			tab_info->view = view;
			return 1;
		}
		tab_info->view = gtab->active_pane
		               ? &gtab->right.tabs[gtab->right.current].view
		               : &gtab->left.tabs[gtab->left.current].view;
		return 1;
	}
}

int
tabs_count(const view_t *view)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(view);
		return (int)DA_SIZE(ptabs->tabs);
	}
	return (int)DA_SIZE(gtabs);
}

void
tabs_only(view_t *view)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		while(DA_SIZE(ptabs->tabs) != 1U)
		{
			pane_tab_t *const ptab = &ptabs->tabs[ptabs->current == 0 ? 1 : 0];
			ptabs->current -= (ptabs->current == 0 ? 0 : 1);
			free_pane_tab(ptab);
			DA_REMOVE(ptabs->tabs, ptab);
		}
	}
	else
	{
		while(DA_SIZE(gtabs) != 1U)
		{
			global_tab_t *const gtab = &gtabs[current_tab == 0 ? 1 : 0];
			current_tab -= (current_tab == 0 ? 0 : 1);
			free_global_tab(gtab);
			DA_REMOVE(gtabs, gtab);
		}
	}
}

/* Retrieves pane tab that corresponds to the specified view (must be &lwin or
 * &rwin).  Returns the pane tab. */
static pane_tabs_t *
get_pane_tabs(const view_t *view)
{
	global_tab_t *const gtab = &gtabs[current_tab];
	return (view == &lwin ? &gtab->left : &gtab->right);
}

int
tabs_visitor_count(const char path[])
{
	int count = 0;
	int i;
	for(i = 0; i < (int)DA_SIZE(gtabs); ++i)
	{
		global_tab_t *const gtab = &gtabs[i];
		view_t *const lview = (i == current_tab ? &lwin : NULL);
		view_t *const rview = (i == current_tab ? &rwin : NULL);
		count += count_pane_visitors(&gtab->left, path, lview);
		count += count_pane_visitors(&gtab->right, path, rview);
	}
	return count;
}

/* Counts number of pane tabs from the collection that are inside specified
 * path.  When view parameter isn't NULL, it's a possible replacement view to
 * use instead of the stored one.  Returns the count. */
static int
count_pane_visitors(const pane_tabs_t *ptabs, const char path[],
		const view_t *view)
{
	int count = 0;
	int i;
	for(i = 0; i < (int)DA_SIZE(ptabs->tabs); ++i)
	{
		const pane_tab_t *const ptab = &ptabs->tabs[i];
		const view_t *const v = (view != NULL && i == ptabs->current)
		                      ? view
		                      : &ptab->view;
		if(path_starts_with(v->curr_dir, path))
		{
			++count;
		}
	}
	return count;
}

void
tabs_switch_panes(void)
{
	if(cfg.pane_tabs)
	{
		const pane_tabs_t tmp = gtabs[current_tab].left;
		gtabs[current_tab].left = gtabs[current_tab].right;
		gtabs[current_tab].right = tmp;

		normalize_pane_tabs(&gtabs[current_tab].left, &lwin);
		normalize_pane_tabs(&gtabs[current_tab].right, &rwin);
	}
}

/* Fixes data fields of views after they got moved from one pane to another. The
 * view parameter indicates destination pane. */
static void
normalize_pane_tabs(const pane_tabs_t *ptabs, view_t *view)
{
	view_t tmp = *view;
	view_t *const other = (view == &rwin ? &lwin : &rwin);
	int i;
	for(i = 0; i < (int)DA_SIZE(ptabs->tabs); ++i)
	{
		if(i != ptabs->current)
		{
			view_t *const v = &ptabs->tabs[i].view;
			ui_swap_view_data(v, &tmp);
			*v = tmp;
			flist_update_origins(v, &other->curr_dir[0], &view->curr_dir[0]);
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
