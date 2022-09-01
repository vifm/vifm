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
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* memmove() */

#include "../cfg/config.h"
#include "../engine/autocmds.h"
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
#include "fileview.h"
#include "ui.h"

/**
 * Tabs are stored in a two-level structure where global tabs own pane tabs.
 *
 * The rest of the code works almost exclusively with global `lwin` and `rwin`
 * variables.  The exception is occasional looping through hidden views (the
 * ones in inactive tabs).
 *
 * All tabs have an id which is unique during a running session.  IDs are unique
 * among all tabs ignoring its type (so even a global and a pane tab can never
 * have the same id).
 */

/* Pane-specific tab (contains information about only one view). */
typedef struct
{
	view_t view;            /* Buffer holding state of the view when it's
	                           hidden. */
	preview_t preview;      /* Information about state of the quickview. */
	char *name;             /* Name of the tab.  Might be NULL. */
	unsigned int id;        /* Unique during the session id of the tab. */
	unsigned int init_mark; /* Which initialization this tab has seen. */
}
pane_tab_t;

/* Collection of pane tabs. */
typedef struct
{
	pane_tab_t **tabs;       /* List of tabs.  Allocated individually on heap to
	                            not copy them and for their fields to have fixed
	                            memory addresses. */
	DA_INSTANCE_FIELD(tabs); /* Declarations to enable use of DA_* on tabs. */
	int current;             /* Index of the current tab. */
}
pane_tabs_t;

/* Global tab (contains both views as well as layout). */
typedef struct
{
	pane_tabs_t left;  /* Collection of tabs for the left pane. */
	pane_tabs_t right; /* Collection of tabs for the right pane. */

	int active_pane;        /* 0 -- left, 1 -- right. */
	int only_mode;          /* Whether in single-pane mode. */
	SPLIT split;            /* State of window split. */
	int splitter_pos;       /* Splitter position. */
	double splitter_ratio;  /* Relative position of the splitter. */
	preview_t preview;      /* Information about state of the quickview. */
	char *name;             /* Name of the tab.  Might be NULL. */
	unsigned int id;        /* Unique during the session id of the tab. */
	unsigned int init_mark; /* Which initialization this tab has seen. */
}
global_tab_t;

static int tabs_new_global(const char name[], const char path[], int at,
		int clean);
static pane_tab_t * tabs_new_pane(pane_tabs_t *ptabs, view_t *side,
		const char name[], const char path[], int at, int clean);
static void clone_view(view_t *dst, view_t *side, const char path[], int clean);
static void clone_viewport(view_t *dst, const view_t *src);
static void tabs_goto_pane(int idx);
static void tabs_goto_global(int idx);
static void capture_global_state(global_tab_t *gtab);
static void assign_preview(preview_t *dst, const preview_t *src);
static void stash_view(view_t *dst, const view_t *src);
static void restore_view(view_t *dst, const view_t *src);
static void free_global_tab(global_tab_t *gtab);
static void free_pane_tabs(pane_tabs_t *ptabs);
static void free_pane_tab(pane_tab_t *ptab);
static void free_preview(preview_t *preview);
static pane_tabs_t * get_pane_tabs(const view_t *side);
static int get_pane_tab(view_t *side, int idx, tab_info_t *tab_info);
static int get_global_tab(view_t *side, int idx, tab_info_t *tab_info,
		int return_active);
static int count_pane_visitors(const pane_tabs_t *ptabs, const char path[],
		const view_t *view);
static void normalize_pane_tabs(const pane_tabs_t *ptabs, const view_t *side);
static void apply_layout(global_tab_t *gtab, const tab_layout_t *layout);

/* Number of time this unit was (re-)initialized. */
static unsigned int init_counter;
/* List of global tabs. */
static global_tab_t *gtabs;
/* Declarations to enable use of DA_* on gtabs. */
static DA_INSTANCE(gtabs);
/* Index of current global tab. */
static int current_gtab;
/* Id number to use on creation of a new tab (global or pane). */
unsigned int next_tab_id = 1;

void
tabs_init(void)
{
	assert(init_counter == 0 && "Mustn't initialize more than once.");
	const int result = tabs_new_global(NULL, NULL, 0, 0);
	assert(result == 0 && "Failed to initialize first tab.");
	(void)result;

	init_counter = 1;
}

void
tabs_reinit(void)
{
	tabs_only(&lwin);
	tabs_only(&rwin);

	++init_counter;
}

int
tabs_new(const char name[], const char path[])
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		int idx = ptabs->current + 1;
		pane_tab_t *ptab = tabs_new_pane(ptabs, curr_view, name, path, idx, 0);
		if(ptab == NULL)
		{
			return 1;
		}

		ptab->preview.on = curr_stats.preview.on;
		tabs_goto_pane(idx);
		return 0;
	}

	if(tabs_new_global(name, path, current_gtab + 1, 0) == 0)
	{
		tabs_goto_global(current_gtab + 1);
		return 0;
	}
	return 1;
}

/* Creates new global tab at position with the specified name, which might be
 * NULL.  Path specifies location of active pane and can be NULL.  Non-zero
 * clean parameter requests clean cloning.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
tabs_new_global(const char name[], const char path[], int at, int clean)
{
	assert(at >= 0 && "Global tab position is too small.");
	assert(at <= (int)DA_SIZE(gtabs) && "Global tab position is too big.");

	global_tab_t new_tab = {};

	if(DA_EXTEND(gtabs) == NULL)
	{
		return 1;
	}

	const char *leftPath = (curr_view == &lwin ? path : NULL);
	const char *rightPath = (curr_view == &rwin ? path : NULL);
	if(tabs_new_pane(&new_tab.left, &lwin, NULL, leftPath, 0, clean) == NULL ||
			tabs_new_pane(&new_tab.right, &rwin, NULL, rightPath, 0, clean) == NULL)
	{
		free_global_tab(&new_tab);
		return 1;
	}
	update_string(&new_tab.name, name);
	capture_global_state(&new_tab);
	new_tab.id = next_tab_id++;

	DA_COMMIT(gtabs);

	memmove(gtabs + at + 1, gtabs + at,
			sizeof(*gtabs)*(DA_SIZE(gtabs) - (at + 1)));
	gtabs[at] = new_tab;
	return 0;
}

/* Creates new tab at position with the specified name, which might be NULL.
 * Path specifies location of active pane and can be NULL.  Non-zero clean
 * parameter requests clean cloning.  Returns newly created tab on success or
 * NULL on error. */
static pane_tab_t *
tabs_new_pane(pane_tabs_t *ptabs, view_t *side, const char name[],
		const char path[], int at, int clean)
{
	assert(at >= 0 && "Pane tab position is too small.");
	assert(at <= (int)DA_SIZE(ptabs->tabs) && "Pane tab position is too big.");

	if(DA_EXTEND(ptabs->tabs) == NULL)
	{
		return NULL;
	}

	pane_tab_t *new_tab = calloc(1, sizeof(*new_tab));
	if(new_tab == NULL)
	{
		return NULL;
	}

	DA_COMMIT(ptabs->tabs);

	/* When we're called from tabs_init(), we just need to create internal
	 * structures without cloning data (or it will leak). */
	if(DA_SIZE(gtabs) != 0U)
	{
		clone_view(&new_tab->view, side, path, clean);
		update_string(&new_tab->name, name);

		memmove(ptabs->tabs + at + 1, ptabs->tabs + at,
				sizeof(*ptabs->tabs)*(DA_SIZE(ptabs->tabs) - (at + 1)));
	}

	new_tab->id = next_tab_id++;
	ptabs->tabs[at] = new_tab;
	return new_tab;
}

/* Clones one view into another.  Path specifies new location and can be NULL.
 * The destination view is assumed to not own any resources.  Clean cloning
 * produces a copy without populating file list and with empty history. */
static void
clone_view(view_t *dst, view_t *side, const char path[], int clean)
{
	copy_str(dst->curr_dir, sizeof(dst->curr_dir),
			path == NULL ? flist_get_dir(side) : path);

	flist_init_view(dst);
	/* This is for replace_dir_entries() below due to check in fentry_free(),
	 * should adjust the check instead? */
	dst->dir_entry[0].origin = side->curr_dir;

	clone_local_options(side, dst, 1);
	reset_local_options(dst);

	matcher_free(dst->manual_filter);
	dst->manual_filter = matcher_clone(side->manual_filter);
	filter_assign(&dst->auto_filter, &side->auto_filter);
	dst->prev_invert = side->prev_invert;
	dst->invert = side->invert;

	/* Clone current entry even though we populate file list later to give
	 * reloading reference point for cursor. */
	replace_dir_entries(dst, &dst->dir_entry, &dst->list_rows,
			get_current_entry(side), 1);
	dst->list_pos = 0;
	clone_viewport(dst, side);

	flist_hist_resize(dst, cfg.history_len);

	if(!clean)
	{
		flist_hist_clone(dst, side);
		if(path != NULL && !flist_custom_active(side))
		{
			/* Record location we're leaving. */
			flist_hist_setup(dst, side->curr_dir, get_current_file_name(side),
					side->list_pos - side->top_line, -1);
		}

		(void)populate_dir_list(dst, path == NULL);

		/* Record new location. */
		flist_hist_save(dst);
	}
}

/* Clones viewport configuration. */
static void
clone_viewport(view_t *dst, const view_t *src)
{
	dst->curr_line = src->curr_line;
	dst->top_line = src->top_line;
	dst->window_rows = src->window_rows;
	dst->window_cols = src->window_cols;
	dst->window_cells = src->window_cells;
}

void
tabs_rename(view_t *side, const char name[])
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(side);
		update_string(&ptabs->tabs[ptabs->current]->name, name);;
	}
	else
	{
		global_tab_t *const gtab = &gtabs[current_gtab];
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

	ui_qv_cleanup_if_needed();

	const int prev = ptabs->current;

	if(curr_stats.load_stage >= 3 && !curr_stats.restart_in_progress)
	{
		/* Mark the tab we started at as visited. */
		ptabs->tabs[ptabs->current]->init_mark = init_counter;
	}

	stash_view(&ptabs->tabs[ptabs->current]->view, curr_view);
	assign_preview(&ptabs->tabs[ptabs->current]->preview, &curr_stats.preview);
	restore_view(curr_view, &ptabs->tabs[idx]->view);
	assign_preview(&curr_stats.preview, &ptabs->tabs[idx]->preview);
	ptabs->current = idx;

	stats_set_quickview(curr_stats.preview.on);
	ui_view_schedule_redraw(curr_view);

	load_view_options(curr_view);

	if(ptabs->tabs[ptabs->current]->init_mark != init_counter &&
			(curr_stats.load_stage >= 3 || curr_stats.load_stage < 0))
	{
		if(ptabs->tabs[ptabs->current]->init_mark == 0)
		{
			clone_viewport(curr_view, &ptabs->tabs[prev]->view);
			populate_dir_list(curr_view, 0);
			fview_dir_updated(curr_view);
		}
		vle_aucmd_execute("DirEnter", flist_get_dir(curr_view), curr_view);
		ptabs->tabs[ptabs->current]->init_mark = init_counter;
	}

	(void)vifm_chdir(flist_get_dir(curr_view));
}

/* Switches to global tab specified by its index if the index is valid. */
static void
tabs_goto_global(int idx)
{
	if(current_gtab == idx)
	{
		return;
	}

	if(idx < 0 || idx >= (int)DA_SIZE(gtabs))
	{
		return;
	}

	ui_qv_cleanup_if_needed();
	modview_hide_graphics();

	global_tab_t *old_gtab = &gtabs[current_gtab];
	global_tab_t *new_gtab = &gtabs[idx];

	if(curr_stats.load_stage >= 3 && !curr_stats.restart_in_progress)
	{
		/* Mark the tab we started at as visited. */
		old_gtab->init_mark = init_counter;
	}

	stash_view(&old_gtab->left.tabs[old_gtab->left.current]->view, &lwin);
	stash_view(&old_gtab->right.tabs[old_gtab->right.current]->view, &rwin);
	capture_global_state(old_gtab);
	assign_preview(&old_gtab->preview, &curr_stats.preview);

	restore_view(&lwin, &new_gtab->left.tabs[new_gtab->left.current]->view);
	restore_view(&rwin, &new_gtab->right.tabs[new_gtab->right.current]->view);
	if(new_gtab->active_pane != (curr_view == &rwin))
	{
		swap_view_roles();
	}
	curr_stats.number_of_windows = (new_gtab->only_mode ? 1 : 2);
	curr_stats.split = new_gtab->split;
	if(new_gtab->splitter_ratio == -1 || new_gtab->splitter_pos < 0)
	{
		stats_set_splitter_pos(new_gtab->splitter_pos);
	}
	else
	{
		stats_set_splitter_ratio(new_gtab->splitter_ratio);
	}
	assign_preview(&curr_stats.preview, &new_gtab->preview);

	current_gtab = idx;

	stats_set_quickview(curr_stats.preview.on);
	ui_view_schedule_redraw(&lwin);
	ui_view_schedule_redraw(&rwin);

	load_view_options(curr_view);

	if(new_gtab->init_mark != init_counter &&
			(curr_stats.load_stage >= 3 || curr_stats.load_stage < 0))
	{
		if(curr_stats.load_stage >= 3)
		{
			ui_resize_all();
		}
		if(new_gtab->init_mark == 0)
		{
			populate_dir_list(&lwin, 0);
			populate_dir_list(&rwin, 0);
			fview_dir_updated(other_view);
			fview_dir_updated(curr_view);
		}
		vle_aucmd_execute("DirEnter", flist_get_dir(&lwin), &lwin);
		vle_aucmd_execute("DirEnter", flist_get_dir(&rwin), &rwin);
		new_gtab->init_mark = init_counter;
	}

	(void)vifm_chdir(flist_get_dir(curr_view));
}

/* Records global state into a global tab structure. */
static void
capture_global_state(global_tab_t *gtab)
{
	tab_layout_t layout;
	tabs_layout_fill(&layout);
	apply_layout(gtab, &layout);
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

/* Turns visible view into a hidden one. */
static void
stash_view(view_t *dst, const view_t *src)
{
	*dst = *src;

	dst->win = NULL;
	dst->title = NULL;

	flist_update_origins(dst);
}

/* Turns hidden view into a visible one. */
static void
restore_view(view_t *dst, const view_t *src)
{
	WINDOW *win = dst->win;
	WINDOW *title = dst->title;

	*dst = *src;

	dst->win = win;
	dst->title = title;

	flist_update_origins(dst);
}

int
tabs_quit_on_close(void)
{
	return (tabs_count(curr_view) == 1);
}

void
tabs_close(void)
{
	/* XXX: FUSE filesystems aren't exited this way, but this might be OK because
	 *      usually we exit from them on explicit ".." by a user. */

	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		pane_tab_t **const ptab_ptr = &ptabs->tabs[ptabs->current];
		const int n = (int)DA_SIZE(ptabs->tabs);
		if(n != 1)
		{
			tabs_goto_pane(ptabs->current + (ptabs->current == n - 1 ? -1 : +1));
			if(ptabs->current > ptab_ptr - ptabs->tabs)
			{
				--ptabs->current;
			}
			free_pane_tab(*ptab_ptr);
			DA_REMOVE(ptabs->tabs, ptab_ptr);
		}
	}
	else
	{
		global_tab_t *const gtab = &gtabs[current_gtab];
		const int n = (int)DA_SIZE(gtabs);
		if(n != 1)
		{
			int idx = (current_gtab == n - 1 ? current_gtab - 1 : current_gtab + 1);
			tabs_goto_global(idx);
			if(current_gtab > gtab - gtabs)
			{
				--current_gtab;
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
		free_pane_tab(ptabs->tabs[i]);
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
	free(ptab);
}

/* Frees resources owned by a preview state structure. */
static void
free_preview(preview_t *preview)
{
	update_string(&preview->cleanup_cmd, NULL);
	modview_info_free(preview->explore);
}

void
tabs_next(int n)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		const int count = (int)DA_SIZE(ptabs->tabs);
		tabs_goto_pane((ptabs->current + n)%count);
	}
	else
	{
		tabs_goto_global((current_gtab + 1)%(int)DA_SIZE(gtabs));
	}
}

void
tabs_previous(int n)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(curr_view);
		const int count = (int)DA_SIZE(ptabs->tabs);
		tabs_goto_pane((ptabs->current + count - n)%count);
	}
	else
	{
		const int count = DA_SIZE(gtabs);
		tabs_goto_global((current_gtab + count - n)%count);
	}
}

int
tabs_get(view_t *side, int idx, tab_info_t *tab_info)
{
	if(cfg.pane_tabs)
	{
		return get_pane_tab(side, idx, tab_info);
	}

	return get_global_tab(side, idx, tab_info, 1);
}

/* Fills *tab_info for the pane tab specified by its index and side.  Returns
 * non-zero on success, otherwise zero is returned. */
static int
get_pane_tab(view_t *side, int idx, tab_info_t *tab_info)
{
	pane_tabs_t *const ptabs = get_pane_tabs(side);
	const int n = (int)DA_SIZE(ptabs->tabs);
	if(idx < 0 || idx >= n)
	{
		return 0;
	}

	tabs_layout_fill(&tab_info->layout);

	if(idx == ptabs->current)
	{
		tab_info->view = side;
	}
	else
	{
		tab_info->view = &ptabs->tabs[idx]->view;
		tab_info->layout.preview = ptabs->tabs[idx]->preview.on;
	}

	tab_info->name = ptabs->tabs[idx]->name;
	tab_info->id = ptabs->tabs[idx]->id;
	tab_info->last = (idx == n - 1);
	return 1;
}

/* Fills *tab_info for the global tab specified by its index and side when
 * return_active is zero, otherwise active one is used.  Returns non-zero on
 * success, otherwise zero is returned. */
static int
get_global_tab(view_t *side, int idx, tab_info_t *tab_info, int return_active)
{
	const int n = (int)DA_SIZE(gtabs);
	if(idx < 0 || idx >= n)
	{
		return 0;
	}

	global_tab_t *gtab = &gtabs[idx];
	tab_info->name = gtab->name;
	tab_info->id = gtab->id;
	tab_info->last = (idx == n - 1);

	if(idx == current_gtab)
	{
		tab_info->view = side;
		tabs_layout_fill(&tab_info->layout);
		return 1;
	}
	tab_info->view = (return_active && !gtab->active_pane)
	              || (!return_active && side == &lwin)
	               ? &gtab->left.tabs[gtab->left.current]->view
	               : &gtab->right.tabs[gtab->right.current]->view;
	tab_info->layout.active_pane = gtab->active_pane;
	tab_info->layout.only_mode = gtab->only_mode;
	tab_info->layout.split = gtab->split;
	tab_info->layout.splitter_pos = gtab->splitter_pos;
	tab_info->layout.splitter_ratio = gtab->splitter_ratio;
	tab_info->layout.preview = gtab->preview.on;
	return 1;
}

int
tabs_current(const view_t *side)
{
	return (cfg.pane_tabs ? get_pane_tabs(side)->current : current_gtab);
}

int
tabs_count(const view_t *side)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(side);
		return (int)DA_SIZE(ptabs->tabs);
	}
	return (int)DA_SIZE(gtabs);
}

void
tabs_only(view_t *side)
{
	if(cfg.pane_tabs)
	{
		pane_tabs_t *const ptabs = get_pane_tabs(side);
		while(DA_SIZE(ptabs->tabs) != 1U)
		{
			pane_tab_t **const ptab_ptr = &ptabs->tabs[ptabs->current == 0 ? 1 : 0];
			ptabs->current -= (ptabs->current == 0 ? 0 : 1);
			free_pane_tab(*ptab_ptr);
			DA_REMOVE(ptabs->tabs, ptab_ptr);
		}
	}
	else
	{
		while(DA_SIZE(gtabs) != 1U)
		{
			global_tab_t *const gtab = &gtabs[current_gtab == 0 ? 1 : 0];
			current_gtab -= (current_gtab == 0 ? 0 : 1);
			free_global_tab(gtab);
			DA_REMOVE(gtabs, gtab);
		}
	}
}

/* Retrieves pane tab that corresponds to the specified view (must be &lwin or
 * &rwin).  Returns the pane tab. */
static pane_tabs_t *
get_pane_tabs(const view_t *side)
{
	global_tab_t *const gtab = &gtabs[current_gtab];
	return (side == &lwin ? &gtab->left : &gtab->right);
}

void
tabs_move(view_t *side, int where_to)
{
	int future = MAX(0, MIN(tabs_count(side) - 1, where_to));
	const int current = tabs_current(side);
	const int from = (current <= future ? current + 1 : future);
	const int to = (current <= future ? current : future + 1);

	/* Second check is for the case when the value was already truncated by MIN()
	 * above. */
	if(current < future && where_to < tabs_count(side))
	{
		--future;
	}

	if(cfg.pane_tabs)
	{
		pane_tab_t **const ptabs = get_pane_tabs(side)->tabs;
		pane_tab_t *ptab = ptabs[current];
		memmove(ptabs + to, ptabs + from, sizeof(*ptabs)*abs(future - current));
		ptabs[future] = ptab;
		get_pane_tabs(side)->current = future;
	}
	else
	{
		const global_tab_t gtab = gtabs[current];
		memmove(gtabs + to, gtabs + from, sizeof(*gtabs)*abs(future - current));
		gtabs[future] = gtab;
		current_gtab = future;
	}
}

int
tabs_visitor_count(const char path[])
{
	int count = 0;
	int i;
	for(i = 0; i < (int)DA_SIZE(gtabs); ++i)
	{
		global_tab_t *const gtab = &gtabs[i];
		view_t *const lview = (i == current_gtab ? &lwin : NULL);
		view_t *const rview = (i == current_gtab ? &rwin : NULL);
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
		const pane_tab_t *const ptab = ptabs->tabs[i];
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
		const pane_tabs_t tmp = gtabs[current_gtab].left;
		gtabs[current_gtab].left = gtabs[current_gtab].right;
		gtabs[current_gtab].right = tmp;

		normalize_pane_tabs(&gtabs[current_gtab].left, &lwin);
		normalize_pane_tabs(&gtabs[current_gtab].right, &rwin);
	}
}

/* Fixes data fields of views after they got moved from one pane to another.
 * The side parameter indicates destination pane. */
static void
normalize_pane_tabs(const pane_tabs_t *ptabs, const view_t *side)
{
	view_t tmp = *side;
	int i;
	for(i = 0; i < (int)DA_SIZE(ptabs->tabs); ++i)
	{
		if(i != ptabs->current)
		{
			view_t *const v = &ptabs->tabs[i]->view;
			ui_swap_view_data(v, &tmp);
			*v = tmp;
		}
	}
}

int
tabs_enum(view_t *side, int idx, tab_info_t *tab_info)
{
	return cfg.pane_tabs ? get_pane_tab(side, idx, tab_info)
	                     : get_global_tab(side, idx, tab_info, 0);
}

int
tabs_enum_all(int idx, tab_info_t *tab_info)
{
	if(tabs_enum(&lwin, idx, tab_info))
	{
		return 1;
	}

	int offset = cfg.pane_tabs ? DA_SIZE(get_pane_tabs(&lwin)->tabs)
	                           : DA_SIZE(gtabs);
	return tabs_enum(&rwin, idx - offset, tab_info);
}

void
tabs_layout_fill(tab_layout_t *layout)
{
	layout->active_pane = (curr_view == &rwin);
	layout->only_mode = (curr_stats.number_of_windows == 1);
	layout->split = curr_stats.split;
	layout->splitter_pos = curr_stats.splitter_pos;
	layout->splitter_ratio = curr_stats.splitter_ratio;
	layout->preview = curr_stats.preview.on;
}

int
tabs_setup_gtab(const char name[], const tab_layout_t *layout, view_t **left,
		view_t **right)
{
	int idx = DA_SIZE(gtabs);
	if(tabs_new_global(name, NULL, idx, 1) != 0)
	{
		return 1;
	}

	global_tab_t *gtab = &gtabs[idx];
	apply_layout(gtab, layout);

	*left = &gtab->left.tabs[0]->view;
	*right = &gtab->right.tabs[0]->view;
	return 0;
}

/* Applies layout data to a global tab. */
static void
apply_layout(global_tab_t *gtab, const tab_layout_t *layout)
{
	gtab->active_pane = layout->active_pane;
	gtab->only_mode = layout->only_mode;
	gtab->split = layout->split;
	gtab->splitter_pos = layout->splitter_pos;
	gtab->splitter_ratio = layout->splitter_ratio;
	gtab->preview.on = layout->preview;
}

view_t *
tabs_setup_ptab(view_t *view, const char name[], int preview)
{
	pane_tabs_t *ptabs = get_pane_tabs(view);
	int idx = DA_SIZE(ptabs->tabs);
	pane_tab_t *ptab = tabs_new_pane(ptabs, view, name, NULL, idx, 1);
	if(ptab == NULL)
	{
		return NULL;
	}

	ptab->preview.on = preview;
	return &ptab->view;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
