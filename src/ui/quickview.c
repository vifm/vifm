/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#include "quickview.h"

#include <curses.h> /* mvwaddstr() */
#include <unistd.h> /* usleep() */

#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE SEEK_SET fclose() fdopen() feof() fseek()
                      tmpfile() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcat() strlen() strncat() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../engine/mode.h"
#include "../lua/vlua.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/modes.h"
#include "../modes/view.h"
#include "../utils/filemon.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../filetype.h"
#include "../macros.h"
#include "../status.h"
#include "../types.h"
#include "../vcache.h"
#include "cancellation.h"
#include "color_scheme.h"
#include "colors.h"
#include "escape.h"
#include "fileview.h"
#include "statusbar.h"
#include "ui.h"

/* Maximum number of lines used for preview. */
enum { MAX_PREVIEW_LINES = 256 };

/* Cached information about a single file's preview. */
typedef struct
{
	char *path;        /* Full path to the file. */
	char *viewer;      /* Viewer of the file. */
	filemon_t filemon; /* Timestamp for the file. */
	preview_area_t pa; /* Where preview is being drawn. */
	strlist_t lines;   /* Lines of graphics (not pass through. */
	int beg_x;         /* Original x coordinate of host window. */
	int beg_y;         /* Original y coordinate of host window. */
	ViewerKind kind;   /* Kind of preview. */
	int max_lines;     /* Maximum number of lines requested last time. */
	int graphics_lost; /* Whether graphics was invalidated on the screen. */
}
quickview_cache_t;

/* State of directory tree print functions. */
typedef struct
{
	FILE *fp;          /* Output preview stream. */
	int n;             /* Current line number (zero based). */
	int ndirs;         /* Number of seen directories. */
	int nfiles;        /* Number of seen files. */
	int max;           /* Maximum line number. */
	int full_stats;    /* Collect statistics for the whole tree. */
	int depth;         /* Current depth of the traversal. */
	char prefix[4096]; /* Prefix character for each tree level. */
}
tree_print_state_t;

static const char * view_entry(const dir_entry_t *entry,
		const preview_area_t *parea, quickview_cache_t *cache);
static const char * view_file(const char path[], const preview_area_t *parea,
		quickview_cache_t *cache);
static int is_cache_valid(const quickview_cache_t *cache, const char path[],
		const char viewer[], const preview_area_t *parea);
static void update_cache(quickview_cache_t *cache, const char path[],
		const char viewer[], ViewerKind kind, const preview_area_t *parea,
		int max_lines);
static strlist_t get_lines(const quickview_cache_t *cache);
static void print_tree_stats(tree_print_state_t *s);
static int print_dir_tree(tree_print_state_t *s, const char path[], int last);
static void collect_subtree_stats(tree_print_state_t *s, const char path[]);
static int enter_dir(tree_print_state_t *s, const char path[], int last);
static int visit_file(tree_print_state_t *s, const char path[], int last);
static int visit_link(tree_print_state_t *s, const char path[], int last,
		const char target[]);
static void leave_dir(tree_print_state_t *s);
static void indent_prefix(tree_print_state_t *s);
static void unindent_prefix(tree_print_state_t *s);
static void set_prefix_char(tree_print_state_t *s, char c);
static void print_tree_entry(tree_print_state_t *s, const char path[],
		int end_line);
static void print_entry_prefix(tree_print_state_t *s);
static void draw_lines(const strlist_t *lines, int wrapped,
		const preview_area_t *parea, ViewerKind kind);
static void write_message(const char msg[], const preview_area_t *parea);
static void cleanup_for_text(const preview_area_t *parea);
static void wipe_area(const preview_area_t *parea);
static void fill_area(const preview_area_t *parea);

/* Cached preview data for a single file entry. */
static quickview_cache_t qv_cache;

int
qv_ensure_is_shown(void)
{
	if(!curr_stats.preview.on && !qv_can_show())
	{
		return 1;
	}
	stats_set_quickview(1);
	return 0;
}

int
qv_can_show(void)
{
	if(curr_stats.number_of_windows == 1)
	{
		ui_sb_err("Cannot view files in one window mode");
		return 0;
	}
	if(other_view->explore_mode)
	{
		ui_sb_err("Other view is already used for file viewing");
		return 0;
	}
	return 1;
}

void
qv_toggle(void)
{
	if(!curr_stats.preview.on)
	{
		stats_set_quickview(1);
		stats_redraw_later();
		return;
	}

	stats_set_quickview(0);

	if(ui_view_is_visible(other_view))
	{
		/* Force cleaning possible leftovers of graphics, otherwise curses internal
		 * structures don't know that those parts need to be redrawn on the
		 * screen. */
		if(curr_stats.preview.cleanup_cmd != NULL ||
				curr_stats.preview.kind != VK_TEXTUAL)
		{
			qv_cleanup(other_view, curr_stats.preview.cleanup_cmd);
		}

		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}

	update_string(&curr_stats.preview.cleanup_cmd, NULL);
	curr_stats.preview.kind = VK_TEXTUAL;
	qv_ui_updated();
}

void
qv_draw(view_t *view)
{
	const dir_entry_t *curr;

	if(curr_stats.load_stage < 2 || curr_stats.number_of_windows == 1 ||
	   vle_mode_is(VIEW_MODE))
	{
		return;
	}

	/* Viewers might use relative path, so try to make sure that we're at correct
	 * location. */
	(void)vifm_chdir(flist_get_dir(view));

	if(modview_detached_draw())
	{
		/* View mode handled the drawing. */
		return;
	}

	ui_view_erase(other_view, 1);

	curr = get_current_entry(view);
	if(!fentry_is_fake(curr))
	{
		const preview_area_t parea = {
			.source = view,
			.view = other_view,
			.def_col = cfg.cs.color[WIN_COLOR],
			.x = ui_qv_left(other_view),
			.y = ui_qv_top(other_view),
			.w = ui_qv_width(other_view),
			.h = ui_qv_height(other_view),
		};
		(void)view_entry(curr, &parea, &qv_cache);
	}

	refresh_view_win(other_view);
	ui_view_title_update(other_view);
}

const char *
qv_draw_on(const dir_entry_t *entry, const preview_area_t *parea)
{
	static quickview_cache_t lwin_cache, rwin_cache;

	ui_set_attr(parea->view->win, &parea->def_col, -1);
	fill_area(parea);

	quickview_cache_t *cache = (parea->view == &lwin ? &lwin_cache : &rwin_cache);

	const char *clear_cmd = view_entry(entry, parea, cache);

	parea->view->displays_graphics = (cache->kind != VK_TEXTUAL);

	/* Unconditionally invalidate graphics cache, since we don't keep track of its
	 * validity in any way. */
	cache->graphics_lost = 1;

	return clear_cmd;
}

/* Draws preview of the entry in the other view.  Returns preview clear command
 * or NULL. */
static const char *
view_entry(const dir_entry_t *entry, const preview_area_t *parea,
		quickview_cache_t *cache)
{
	char path[PATH_MAX + 1];
	qv_get_path_to_explore(entry, path, sizeof(path));

	FileType type = entry->type;

	struct stat st;
	if(os_stat(path, &st) == 0)
	{
		type = get_type_from_mode(st.st_mode);
	}

	switch(type)
	{
		case FT_CHAR_DEV:
			write_message("File is a Character Device", parea);
			break;
		case FT_BLOCK_DEV:
			write_message("File is a Block Device", parea);
			break;
#ifndef _WIN32
		case FT_SOCK:
			write_message("File is a Socket", parea);
			break;
#endif
		case FT_FIFO:
			write_message("File is a Named Pipe", parea);
			break;
		case FT_LINK:
			if(get_link_target_abs(path, entry->origin, path, sizeof(path)) != 0)
			{
				write_message("Cannot resolve Link", parea);
				break;
			}

			if(!path_exists(path, DEREF))
			{
				char *msg = format_str("Cannot access link's target: %s", path);
				write_message(msg, parea);
				free(msg);
				break;
			}

			if(!ends_with_slash(path) && is_dir(path))
			{
				strncat(path, "/", sizeof(path) - strlen(path) - 1);
			}
			/* break is omitted intentionally. */
		case FT_UNK:
		default:
			return view_file(path, parea, cache);
	}

	return NULL;
}

/* Displays contents of file or output of its viewer in the other pane
 * starting from the second line and second column.  Returns preview clear
 * command or NULL. */
static const char *
view_file(const char path[], const preview_area_t *parea,
		quickview_cache_t *cache)
{
	const char *viewer = qv_get_viewer(path);
	const char *clear_cmd = (viewer != NULL ? ma_get_clear_cmd(viewer) : NULL);

	/* Don't draw graphics while dialog is shown as they aren't combined nicely
	 * on the screen. */
	const ViewerKind kind = ft_viewer_kind(viewer);
	if(kind != VK_TEXTUAL && modes_is_dialog_like())
	{
		return NULL;
	}

	if(is_cache_valid(cache, path, viewer, parea))
	{
		/* Update area as we might draw preview at a different location. */
		cache->pa = *parea;

		draw_lines(&cache->lines, cfg.wrap_quick_view, &cache->pa, cache->kind);
		return clear_cmd;
	}

	int max_lines = is_dir(path) ? ui_qv_height(parea->view)
	                             : MAX_PREVIEW_LINES;

	/* If graphics will be displayed, clear the window and wait a bit to let
	 * terminal emulator do actual refresh (at least some of them need this). */
	if(kind != VK_TEXTUAL)
	{
		qv_cleanup_area(parea, curr_stats.preview.cleanup_cmd);
		usleep(cfg.graphics_delay);
	}
	else
	{
		/* We want to wipe the view if it was displaying graphics, but won't
		 * anymore.  Do this only if we didn't already cleared the window. */
		cleanup_for_text(parea);
	}

	curr_stats.preview.kind = kind;

	update_string(&curr_stats.preview.cleanup_cmd, clear_cmd);

	update_cache(cache, path, viewer, kind, parea, max_lines);
	strlist_t lines = get_lines(cache);
	draw_lines(&lines, cfg.wrap_quick_view, &cache->pa, cache->kind);

	if(cache->kind != VK_TEXTUAL)
	{
		free_string_array(cache->lines.items, cache->lines.nitems);
		cache->lines.items = copy_string_array(lines.items, lines.nitems);
		cache->lines.nitems = lines.nitems;
	}

	return clear_cmd;
}

/* Checks whether data in the cache is up to date with the file on disk.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_cache_valid(const quickview_cache_t *cache, const char path[],
		const char viewer[], const preview_area_t *parea)
{
	if(cache->kind == VK_TEXTUAL)
	{
		/* This level of cache deals only with graphics and ignores text. */
		return 0;
	}

	int same_viewer = (cache->viewer == NULL && viewer == NULL)
	               || (cache->viewer != NULL && viewer != NULL &&
	                   strcmp(cache->viewer, viewer) == 0);

	filemon_t filemon;
	if(same_viewer &&
			filemon_from_file(path, FMT_MODIFIED, &filemon) == 0 &&
			cache->path != NULL &&
			paths_are_equal(cache->path, path) &&
			filemon_equal(&cache->filemon, &filemon))
	{
		return !cache->graphics_lost
		    && cache->pa.h == parea->h
		    && cache->pa.w == parea->w
		    && cache->beg_x + cache->pa.x == getbegx(parea->view->win) + parea->x
		    && cache->beg_y + cache->pa.y == getbegy(parea->view->win) + parea->y;
	}

	return 0;
}

/* Updates cache data. */
static void
update_cache(quickview_cache_t *cache, const char path[], const char viewer[],
		ViewerKind kind, const preview_area_t *parea, int max_lines)
{
	(void)filemon_from_file(path, FMT_MODIFIED, &cache->filemon);

	replace_string(&cache->path, path);
	update_string(&cache->viewer, viewer);

	cache->pa = *parea;
	cache->beg_x = getbegx(parea->view->win);
	cache->beg_y = getbegy(parea->view->win);
	cache->kind = kind;
	cache->max_lines = max_lines;
	cache->graphics_lost = 0;
}

/* Retrieves lines to be printed.  Returns the lines. */
static strlist_t
get_lines(const quickview_cache_t *cache)
{
	view_t *curr = curr_view;
	curr_view = cache->pa.source;
	curr_stats.preview_hint = &cache->pa;

	const char *error;

	MacroFlags flags = MF_NONE;
	char *expanded = (cache->viewer == NULL)
	               ? NULL
	               : qv_expand_viewer(cache->pa.source, cache->viewer, &flags);

	strlist_t lines = vcache_lookup(cache->path, expanded, flags, cache->kind,
			cache->max_lines, VC_ASYNC, &error);
	free(expanded);

	if(error != NULL)
	{
		lines.nitems = add_to_string_array(&lines.items, lines.nitems, error);
	}

	curr_stats.preview_hint = NULL;
	curr_view = curr;

	return lines;
}

FILE *
qv_view_dir(const char path[], int max_lines)
{
	enum { NSPACES = 64 };

	FILE *fp = os_tmpfile();
	if(fp == NULL)
	{
		return NULL;
	}

	tree_print_state_t s = {
		.fp = fp,
		/* Increase by one to cause cached data to be recognized as incomplete
		 * when max_lines isn't enough. */
		.max = (max_lines == INT_MAX ? max_lines : max_lines + 1),
		.full_stats = cfg.top_tree_stats,
		.n = (cfg.top_tree_stats ? 2 : 0),
	};

	/* Spare blank line on the top of the view to put the (files, directories)
	 * count in case "toptreestats" option is set. */
	fprintf(fp, "%*s\n", NSPACES, "");

	const int whole_tree = (print_dir_tree(&s, path, 0) == 0 && s.n != 0);
	if(!whole_tree && ui_cancellation_requested())
	{
		fputs("(cancelled)", fp);
	}

	if(s.n == 0)
	{
		fclose(fp);
		return NULL;
	}

	if(cfg.top_tree_stats)
	{
		/* Print count on the top, spare line. */
		fseek(fp, 0, SEEK_SET);
		print_tree_stats(&s);
		fseek(fp, 0, SEEK_SET);
	}
	else
	{
		/* Print count at the bottom and "hide" the spare line away. */
		fputs("\n", fp);
		print_tree_stats(&s);
		fseek(fp, NSPACES + 1, SEEK_SET);
	}

	return fp;
}

/* Prints one-line tree statistics. */
static void
print_tree_stats(tree_print_state_t *s)
{
	fprintf(s->fp, "%s%d director%s, %d file%s\n",
			ui_cancellation_requested() ? "(cancelled)\n" : "",
			s->ndirs, (s->ndirs == 1) ? "y" : "ies",
			s->nfiles, (s->nfiles == 1) ? "" : "s");
}

/* Produces tree preview of the path.  Returns non-zero to request stopping of
 * the traversal, otherwise zero is returned. */
static int
print_dir_tree(tree_print_state_t *s, const char path[], int last)
{
	int len;
	char **lst = list_sorted_files(path, &len);
	if(len < 0)
	{
		return 1;
	}

	if(enter_dir(s, path, last) != 0)
	{
		free_string_array(lst, len);
		collect_subtree_stats(s, path);
		return 1;
	}

	/* No need to check cfg.max_tree_depth for 0, after enter_dir s->depth is
	 * greater than 0. */
	if(s->depth == cfg.max_tree_depth)
	{
		free_string_array(lst, len);
		leave_dir(s);
		return 0;
	}

	int i;
	int reached_limit = 0;
	for(i = 0; i < len && !reached_limit && !ui_cancellation_requested(); ++i)
	{
		const int last_entry = (i == len - 1);
		char *const full_path = format_str("%s/%s", path, lst[i]);

		if(is_dir(full_path))
		{
			++s->ndirs;
		}
		else
		{
			++s->nfiles;
		}

		char link_target[PATH_MAX + 1];
		if(get_link_target(full_path, link_target, sizeof(link_target)) == 0)
		{
			if(visit_link(s, full_path, last_entry, link_target) != 0)
			{
				reached_limit = 1;
			}
		}
		/* If is_dir_empty() returns non-zero then we know that it's a directory
		 * and no additional checks are needed. */
		else if(!is_dir_empty(full_path))
		{
			if(last_entry)
			{
				set_prefix_char(s, '`');
			}
			if(print_dir_tree(s, full_path, last_entry) != 0)
			{
				reached_limit = 1;
			}
		}
		else
		{
			if(visit_file(s, full_path, last_entry) != 0)
			{
				reached_limit = 1;
			}
		}

		free(full_path);
	}

	if(reached_limit && s->full_stats)
	{
		for(; i < len && !ui_cancellation_requested(); ++i)
		{
			char *const full_path = format_str("%s/%s", path, lst[i]);
			if(is_symlink(full_path))
			{
				++s->nfiles;
			}
			else if(is_dir(full_path))
			{
				++s->ndirs;
				collect_subtree_stats(s, full_path);
			}
			else
			{
				++s->nfiles;
			}
			free(full_path);
		}
	}

	free_string_array(lst, len);
	leave_dir(s);

	return reached_limit;
}

/* Collects stats for items of a directory in a much faster way than traversal
 * for printing. */
static void
collect_subtree_stats(tree_print_state_t *s, const char path[])
{
	DIR *dir = os_opendir(path);
	if(dir == NULL)
	{
		return;
	}

	struct dirent *d;
	while((d = os_readdir(dir)) != NULL)
	{
		if(is_builtin_dir(d->d_name))
		{
			continue;
		}

		char *const full_path = format_str("%s/%s", path, d->d_name);
		if(entry_is_dir(full_path, d))
		{
			++s->ndirs;
			collect_subtree_stats(s, full_path);
		}
		else if(is_dirent_targets_dir(full_path, d))
		{
			++s->ndirs;
		}
		else
		{
			++s->nfiles;
		}
		free(full_path);
	}
	os_closedir(dir);
}

/* Handles entering directory on directory tree traversal.  Returns non-zero to
 * request stopping of the traversal, otherwise zero is returned. */
static int
enter_dir(tree_print_state_t *s, const char path[], int last)
{
	print_tree_entry(s, path, 1);

	if(last)
	{
		set_prefix_char(s, ' ');
	}

	indent_prefix(s);
	set_prefix_char(s, '|');

	++s->depth;
	return (++s->n >= s->max);
}

/* Handles visiting file on directory tree traversal.  Returns non-zero to
 * request stopping of the traversal, otherwise zero is returned. */
static int
visit_file(tree_print_state_t *s, const char path[], int last)
{
	set_prefix_char(s, last ? '`' : '|');
	print_tree_entry(s, path, 1);

	return (++s->n >= s->max);
}

/* Handles visiting symbolic link on directory tree traversal.  Returns non-zero
 * to request stopping of the traversal, otherwise zero is returned. */
static int
visit_link(tree_print_state_t *s, const char path[], int last,
		const char target[])
{
	set_prefix_char(s, last ? '`' : '|');
	print_tree_entry(s, path, 0);
	fputs(" -> ", s->fp);
	fputs(target, s->fp);
	fputc('\n', s->fp);

	return (++s->n >= s->max);
}

/* Handles leaving directory on directory tree traversal. */
static void
leave_dir(tree_print_state_t *s)
{
	unindent_prefix(s);
	--s->depth;
}

/* Adds one indentation level for the following elements of tree. */
static void
indent_prefix(tree_print_state_t *s)
{
	if(strlen(s->prefix) + 1U + 1U < sizeof(s->prefix))
	{
		strcat(s->prefix, " ");
	}
}

/* Removes one indentation level for the following elements of tree. */
static void
unindent_prefix(tree_print_state_t *s)
{
	set_prefix_char(s, '\0');
}

/* Sets prefix for current tree level. */
static void
set_prefix_char(tree_print_state_t *s, char c)
{
	const size_t len = strlen(s->prefix);
	if(len >= 1U)
	{
		s->prefix[len - 1U] = c;
	}
}

/* Prints single entry of directory tree. */
static void
print_tree_entry(tree_print_state_t *s, const char path[], int end_line)
{
	print_entry_prefix(s);
	fputs(get_last_path_component(path), s->fp);
	if(is_dir(path) && !ends_with_slash(path))
	{
		fputc('/', s->fp);
	}
	if(end_line)
	{
		fputc('\n', s->fp);
	}
}

/* Prints part of the string to the left of entry name. */
static void
print_entry_prefix(tree_print_state_t *s)
{
	const char *p = s->prefix;

	/* Expand " |`" into "    |   `-- ". */
	while(p[0] != '\0')
	{
		fputc(p[0], s->fp);
		fputs(p[1] == '\0' ? "-- " : "   ", s->fp);
		++p;
	}
}

/* Displays lines in the other pane.  The wrapped parameter determines whether
 * lines should be wrapped. */
static void
draw_lines(const strlist_t *lines, int wrapped, const preview_area_t *parea,
		ViewerKind kind)
{
	if(kind == VK_PASS_THROUGH)
	{
		ui_pass_through(lines, parea->view->win, parea->x, parea->y);
		return;
	}

	const size_t left = parea->x;
	const size_t top = parea->y;
	const size_t max_width = parea->w;
	const size_t max_height = parea->h;

	int line_continued = 0;
	size_t y = top;

	esc_state state;
	esc_state_init(&state, &parea->def_col, COLORS);

	int next_line = 0;
	const char *input_line = (next_line == lines->nitems)
	                       ? NULL
	                       : lines->items[next_line++];
	while(input_line != NULL && y < top + max_height)
	{
		int printed;
		input_line += esc_print_line(input_line, parea->view->win, left, y,
				max_width, 0, !wrapped, &state, &printed);

		y += !wrapped || (!line_continued || printed);
		line_continued = (*input_line != '\0');

		if(y < top + max_height && (!wrapped || !line_continued))
		{
			input_line = (next_line == lines->nitems) ? NULL
			                                          : lines->items[next_line++];
		}
	}
}

/* Writes single line message of error or information kind instead of real
 * preview. */
static void
write_message(const char msg[], const preview_area_t *parea)
{
	cleanup_for_text(parea);

	char *items[] = { (char *)msg };
	const strlist_t lines = { .items = items, .nitems = 1 };
	draw_lines(&lines, 1, parea, VK_TEXTUAL);
}

/* Ensures that area is ready to display regular text. */
static void
cleanup_for_text(const preview_area_t *parea)
{
	if(curr_stats.preview.cleanup_cmd != NULL ||
			curr_stats.preview.kind != VK_TEXTUAL)
	{
		qv_cleanup_area(parea, curr_stats.preview.cleanup_cmd);
	}
	update_string(&curr_stats.preview.cleanup_cmd, NULL);
	curr_stats.preview.kind = VK_TEXTUAL;
}

void
qv_hide(void)
{
	if(curr_stats.preview.on)
	{
		qv_toggle();
	}
	if(lwin.explore_mode)
	{
		modview_quit_exploring(&lwin);
	}
	if(rwin.explore_mode)
	{
		modview_quit_exploring(&rwin);
	}
}

char *
qv_expand_viewer(view_t *view, const char viewer[], MacroFlags *flags)
{
	ma_flags_set(flags, MF_NONE);

	char *result;
	if(strchr(viewer, '%') == NULL)
	{
		char *escaped = shell_like_escape(get_current_file_name(view), 0);
		result = format_str("%s %s", viewer, escaped);
		free(escaped);
	}
	else
	{
		result = ma_expand(viewer, NULL, flags, MER_SHELL);
	}
	return result;
}

void
qv_cleanup(view_t *view, const char cmd[])
{
	const preview_area_t parea = {
		.source = view,
		.view = view,
		.def_col = ui_view_get_cs(view)->color[WIN_COLOR],
		.x = 0,
		.y = 0,
		.w = view->window_cols,
		.h = view->window_rows,
	};
	qv_cleanup_area(&parea, cmd);
}

void
qv_cleanup_area(const preview_area_t *parea, const char cmd[])
{
	if(cmd == NULL)
	{
		wipe_area(parea);
		return;
	}

	view_t *const curr = curr_view;
	curr_view = parea->view;

	curr_stats.preview.clearing = 1;
	curr_stats.preview_hint = parea;

	char *expanded = qv_expand_viewer(parea->view, cmd, NULL);
	if(vlua_handler_cmd(curr_stats.vlua, expanded))
	{
		char path[PATH_MAX + 1];
		dir_entry_t *entry = get_current_entry(parea->source);
		qv_get_path_to_explore(entry, path, sizeof(path));

		strlist_t lines = vlua_view_file(curr_stats.vlua, expanded, path, parea);
		free_string_array(lines.items, lines.nitems);
	}
	else
	{
		FILE *fp = read_cmd_output(expanded, /*preserve_stdin=*/0);
		while(fgetc(fp) != EOF);
		fclose(fp);
	}
	free(expanded);

	curr_stats.preview_hint = NULL;
	curr_stats.preview.clearing = 0;

	curr_view = curr;

	wipe_area(parea);
}

/* Ensures that area of a view is updated on the screen (e.g. to clear anything
 * put there by other programs as well). */
static void
wipe_area(const preview_area_t *parea)
{
	if(cfg.hard_graphics_clear)
	{
		wclear(parea->view->win);
		return;
	}

	/* User doesn't need to see fake filling so draw it with the color of
	 * background. */
	col_attr_t col = parea->def_col;
	col.attr = 0;
	col.gui_attr = 0;
	ui_set_attr(parea->view->win, &col, -1);

	fill_area(parea);
	/* The check is for tests, which work otherwise. */
	if(parea->view->win != NULL)
	{
		redrawwin(parea->view->win);
	}
	ui_refresh_win(parea->view->win);

	ui_set_attr(parea->view->win, &parea->def_col, -1);
}

/* Fills preview area with spaces just to clear background. */
static void
fill_area(const preview_area_t *parea)
{
	char filler[parea->w + 1];
	memset(filler, ' ', sizeof(filler) - 1U);
	filler[sizeof(filler) - 1U] = '\0';

	int line;
	for(line = parea->y; line < parea->y + parea->h; ++line)
	{
		mvwaddstr(parea->view->win, line, parea->x, filler);
	}
}

const char *
qv_get_viewer(const char path[])
{
	if(is_null_or_empty(curr_view->preview_prg))
	{
		char *const typed_fname = is_dir(path)
		                        ? format_str("%s/", path)
		                        : strdup(path);
		const char *const viewer = ft_get_viewer(typed_fname);
		free(typed_fname);
		return viewer;
	}
	else
	{
		/* 'previewprg' option has priority over :fileviewer command. */
		return curr_view->preview_prg;
	}
}

void
qv_get_path_to_explore(const dir_entry_t *entry, char buf[], size_t buf_len)
{
	if(entry->type == FT_DIR && is_parent_dir(entry->name))
	{
		char *const typed_fname = format_str("%s%s../", entry->origin,
				ends_with_slash(entry->origin) ? "" : "/");

		/* In the absence of handler for ".." entry, transform it into path to its
		 * origin. */
		if(ft_get_viewer(typed_fname) == NULL)
		{
			free(typed_fname);
			copy_str(buf, buf_len, entry->origin);
			return;
		}
		free(typed_fname);
	}

	get_full_path_of(entry, buf_len, buf);
}

void
qv_ui_updated(void)
{
	/* Invalidate graphical cache. */
	qv_cache.graphics_lost = 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
