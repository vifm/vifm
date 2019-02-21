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
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/modes.h"
#include "../modes/view.h"
#include "../utils/file_streams.h"
#include "../utils/filemon.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../filetype.h"
#include "../macros.h"
#include "../status.h"
#include "../types.h"
#include "cancellation.h"
#include "color_manager.h"
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
	filemon_t filemon; /* Timestamp for the file. */
	strlist_t lines;   /* Top MAX_PREVIEW_LINES of preview contents. */
	int x, y, w, h;    /* Last seen preview area parameters (important for
	                      graphical preview). */
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
	char prefix[4096]; /* Prefix character for each tree level. */
}
tree_print_state_t;

static void view_entry(const dir_entry_t *entry);
static void view_file(const char path[]);
static int is_cache_valid(const char path[], int graphical);
static void fill_cache(FILE *fp, const char path[]);
TSTATIC strlist_t read_lines(FILE *fp, int max_lines);
static FILE * view_dir(const char path[], int max_lines);
static int print_dir_tree(tree_print_state_t *s, const char path[], int last);
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
static void draw_lines(const strlist_t *lines, int wrapped);
static void write_message(const char msg[]);
static void cleanup_for_text(void);
static char * expand_viewer_command(const char viewer[]);

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
		stats_redraw_schedule();
		return;
	}

	stats_set_quickview(0);

	if(ui_view_is_visible(other_view))
	{
		/* Force cleaning possible leftovers of graphics, otherwise curses internal
		 * structures don't know that those parts need to be redrawn on the
		 * screen. */
		if(curr_stats.preview.cleanup_cmd != NULL || curr_stats.preview.graphical)
		{
			qv_cleanup(other_view, curr_stats.preview.cleanup_cmd);
		}

		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}

	update_string(&curr_stats.preview.cleanup_cmd, NULL);
	curr_stats.preview.graphical = 0;
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

	if(view_detached_draw())
	{
		/* View mode handled the drawing. */
		return;
	}

	ui_view_erase(other_view);

	curr = get_current_entry(view);
	if(!fentry_is_fake(curr))
	{
		view_entry(curr);
	}

	refresh_view_win(other_view);
	ui_view_title_update(other_view);
}

/* Draws preview of the entry in the other view. */
static void
view_entry(const dir_entry_t *entry)
{
	char path[PATH_MAX + 1];
	qv_get_path_to_explore(entry, path, sizeof(path));

	switch(entry->type)
	{
		case FT_CHAR_DEV:
			write_message("File is a Character Device");
			break;
		case FT_BLOCK_DEV:
			write_message("File is a Block Device");
			break;
#ifndef _WIN32
		case FT_SOCK:
			write_message("File is a Socket");
			break;
#endif
		case FT_FIFO:
			write_message("File is a Named Pipe");
			break;
		case FT_LINK:
			if(get_link_target_abs(path, entry->origin, path, sizeof(path)) != 0)
			{
				write_message("Cannot resolve Link");
				break;
			}
			if(!ends_with_slash(path) && is_dir(path))
			{
				strncat(path, "/", sizeof(path) - strlen(path) - 1);
			}
			/* break is omitted intentionally. */
		case FT_UNK:
		default:
			view_file(path);
			break;
	}
}

/* Displays contents of file or output of its viewer in the other pane
 * starting from the second line and second column. */
static void
view_file(const char path[])
{
	const char *viewer = qv_get_viewer(path);
	int graphical = (!is_null_or_empty(viewer) && is_graphical_viewer(viewer));

	if(is_cache_valid(path, graphical))
	{
		ui_drop_attr(other_view->win);
		draw_lines(&qv_cache.lines, cfg.wrap_quick_view);
		return;
	}

	FILE *fp;
	if(viewer == NULL && is_dir(path))
	{
		ui_cancellation_reset();
		ui_cancellation_enable();
		fp = view_dir(path, ui_qv_height(other_view));
		ui_cancellation_disable();

		if(fp == NULL)
		{
			write_message("Failed to view directory");
			return;
		}
	}
	else if(is_null_or_empty(viewer))
	{
		fp = os_fopen(path, "rb");
		if(fp == NULL)
		{
			write_message("Cannot open file");
			return;
		}
	}
	else
	{
		/* If graphics will be displayed, clear the window and wait a bit to let
		 * terminal emulator do actual refresh (at least some of them need this). */
		if(graphical)
		{
			qv_cleanup(other_view, curr_stats.preview.cleanup_cmd);
			usleep(50000);
		}
		fp = qv_execute_viewer(viewer);
		if(fp == NULL)
		{
			write_message("Cannot read viewer output");
			return;
		}
	}

	ui_cancellation_reset();
	ui_cancellation_enable();

	/* We want to wipe the view if it was displaying graphics, but won't anymore.
	 * Do this only if we didn't already cleared the window. */
	if(!graphical)
	{
		cleanup_for_text();
	}
	curr_stats.preview.graphical = graphical;

	const char *clear_cmd = (viewer != NULL) ? ma_get_clear_cmd(viewer) : NULL;
	update_string(&curr_stats.preview.cleanup_cmd, clear_cmd);

	fill_cache(fp, path);

	fclose(fp);

	ui_cancellation_disable();

	ui_drop_attr(other_view->win);
	draw_lines(&qv_cache.lines, cfg.wrap_quick_view);
}

/* Checks whether data in qv_cache is up to date with the file on disk.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_cache_valid(const char path[], int graphical)
{
	filemon_t filemon;
	if(filemon_from_file(path, &filemon) == 0 && qv_cache.path != NULL &&
			paths_are_equal(qv_cache.path, path) &&
			filemon_equal(&qv_cache.filemon, &filemon))
	{
		if(!graphical)
		{
			return 1;
		}

		return graphical
		    && qv_cache.h == ui_qv_height(other_view)
		    && qv_cache.w == ui_qv_width(other_view)
		    && qv_cache.x == ui_qv_x(other_view)
		    && qv_cache.y == ui_qv_y(other_view);
	}

	return 0;
}

/* Fills qv_cache data with file's contents. */
static void
fill_cache(FILE *fp, const char path[])
{
	/* File monitor must always be initialized, because it's used below. */
	filemon_t filemon = {};
	(void)filemon_from_file(path, &filemon);
	filemon_assign(&qv_cache.filemon, &filemon);

	replace_string(&qv_cache.path, path);

	free_string_array(qv_cache.lines.items, qv_cache.lines.nitems);
	qv_cache.lines = read_lines(fp, MAX_PREVIEW_LINES);

	qv_cache.h = ui_qv_height(other_view);
	qv_cache.w = ui_qv_width(other_view);
	qv_cache.x = ui_qv_x(other_view);
	qv_cache.y = ui_qv_y(other_view);
}

/* Reads at most max_lines from the stream ignoring BOM.  Returns the lines
 * read. */
TSTATIC strlist_t
read_lines(FILE *fp, int max_lines)
{
	strlist_t lines = {};
	skip_bom(fp);

	char *next_line;
	while(lines.nitems < max_lines && (next_line = read_line(fp, NULL)) != NULL)
	{
		const int old_len = lines.nitems;
		lines.nitems = put_into_string_array(&lines.items, lines.nitems, next_line);
		if(lines.nitems == old_len)
		{
			free(next_line);
			break;
		}
	}

	return lines;
}

FILE *
qv_view_dir(const char path[])
{
	return view_dir(path, INT_MAX);
}

/* Previews directory, actual preview is to be read from returned stream.
 * Returns the stream or NULL on error. */
static FILE *
view_dir(const char path[], int max_lines)
{
	FILE *fp = os_tmpfile();

	if(fp != NULL)
	{
		tree_print_state_t s = {
			.fp = fp,
			.max = max_lines,
		};

		if(print_dir_tree(&s, path, 0) == 0 && s.n != 0)
		{
			/* Print summary only if we visited the whole subtree. */
			fprintf(fp, "%s\n%d director%s, %d file%s",
					ui_cancellation_requested() ? "(cancelled)\n" : "",
					s.ndirs, (s.ndirs == 1) ? "y" : "ies",
					s.nfiles, (s.nfiles == 1) ? "" : "s");
		}
		else if(ui_cancellation_requested())
		{
			fputs("(cancelled)", fp);
		}

		if(s.n == 0)
		{
			fclose(fp);
			fp = NULL;
		}
		else
		{
			fseek(fp, 0, SEEK_SET);
		}
	}
	return fp;
}

/* Produces tree preview of the path.  Returns non-zero to request stopping of
 * the traversal, otherwise zero is returned. */
static int
print_dir_tree(tree_print_state_t *s, const char path[], int last)
{
	int i;
	int reached_limit;

	int len;
	char **lst = list_sorted_files(path, &len);

	if(len < 0)
	{
		free_string_array(lst, len);
		return 1;
	}

	if(enter_dir(s, path, last) != 0)
	{
		free_string_array(lst, len);
		return 1;
	}

	reached_limit = 0;
	for(i = 0; i < len && !reached_limit && !ui_cancellation_requested(); ++i)
	{
		char link_target[PATH_MAX + 1];
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

		if(get_link_target(full_path, link_target, sizeof(link_target)) == 0)
		{
			if(visit_link(s, full_path, last_entry, link_target) != 0)
			{
				reached_limit = 1;
			}
		}
		/* If is_dir_empty() returns non-zero than we know that it's directory and
		 * no additional checks are needed. */
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
		else if(visit_file(s, full_path, last_entry) != 0)
		{
			reached_limit = 1;
		}

		free(full_path);
	}
	free_string_array(lst, len);

	leave_dir(s);

	return reached_limit;
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

	return ++s->n >= s->max;
}

/* Handles visiting file on directory tree traversal.  Returns non-zero to
 * request stopping of the traversal, otherwise zero is returned. */
static int
visit_file(tree_print_state_t *s, const char path[], int last)
{
	set_prefix_char(s, last ? '`' : '|');
	print_tree_entry(s, path, 1);

	return ++s->n >= s->max;
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

	return ++s->n >= s->max;
}

/* Handles leaving directory on directory tree traversal. */
static void
leave_dir(tree_print_state_t *s)
{
	unindent_prefix(s);
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
draw_lines(const strlist_t *lines, int wrapped)
{
	const size_t left = ui_qv_left(other_view);
	const size_t top = ui_qv_top(other_view);
	const size_t max_width = ui_qv_width(other_view);
	const size_t max_height = ui_qv_height(other_view);

	const col_scheme_t *cs = ui_view_get_cs(other_view);
	int line_continued = 0;
	size_t y = top;

	esc_state state;
	esc_state_init(&state, &cs->color[WIN_COLOR], COLORS);

	int next_line = 0;
	const char *input_line = (next_line == lines->nitems)
	                       ? NULL
	                       : lines->items[next_line++];
	while(input_line != NULL && y < top + max_height)
	{
		int printed;
		input_line += esc_print_line(input_line, other_view->win, left, y,
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
write_message(const char msg[])
{
	cleanup_for_text();
	ui_drop_attr(other_view->win);
	mvwaddstr(other_view->win, ui_qv_top(other_view), ui_qv_left(other_view),
			msg);
}

/* Ensures that view is ready to display regular text. */
static void
cleanup_for_text(void)
{
	if(curr_stats.preview.cleanup_cmd != NULL || curr_stats.preview.graphical)
	{
		qv_cleanup(other_view, curr_stats.preview.cleanup_cmd);
	}
	update_string(&curr_stats.preview.cleanup_cmd, NULL);
	curr_stats.preview.graphical = 0;
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
		view_quit_explore_mode(&lwin);
	}
	if(rwin.explore_mode)
	{
		view_quit_explore_mode(&rwin);
	}
}

FILE *
qv_execute_viewer(const char viewer[])
{
	FILE *fp;
	char *expanded;

	expanded = expand_viewer_command(viewer);
	fp = read_cmd_output(expanded, 0);
	free(expanded);

	return fp;
}

/* Returns a pointer to newly allocated memory, which should be released by the
 * caller. */
static char *
expand_viewer_command(const char viewer[])
{
	char *result;
	if(strchr(viewer, '%') == NULL)
	{
		char *escaped = shell_like_escape(get_current_file_name(curr_view), 0);
		result = format_str("%s %s", viewer, escaped);
		free(escaped);
	}
	else
	{
		result = ma_expand(viewer, NULL, NULL, 1);
	}
	return result;
}

void
qv_cleanup(view_t *view, const char cmd[])
{
	view_t *const curr = curr_view;
	FILE *fp;

	if(cmd == NULL)
	{
		ui_view_wipe(view);
		return;
	}

	curr_view = view;
	curr_stats.preview.clearing = 1;
	fp = qv_execute_viewer(cmd);
	curr_stats.preview.clearing = 0;
	curr_view = curr;

	while(fgetc(fp) != EOF);
	fclose(fp);

	ui_view_wipe(view);
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
	qv_cache.h = 0;
	qv_cache.w = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
