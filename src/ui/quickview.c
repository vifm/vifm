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

#include <curses.h> /* mvwaddstr() wattrset() */
#include <unistd.h> /* usleep() */

#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE SEEK_SET fclose() fdopen() feof() fseek()
                      tmpfile() */
#include <stdlib.h> /* free() qsort() */
#include <string.h> /* memmove() strcat() strlen() strncat() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../engine/mode.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/modes.h"
#include "../modes/view.h"
#include "../utils/file_streams.h"
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
#include "ui.h"

/* Size of buffer holding preview line (in characters). */
#define PREVIEW_LINE_BUF_LEN 4096

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

static void view_file(const char path[]);
static FILE * view_dir(const char path[], int max_lines);
static int print_dir_tree(tree_print_state_t *s, const char path[], int last);
static int enter_dir(tree_print_state_t *s, const char path[], int last);
static int visit_file(tree_print_state_t *s, const char path[], int last);
static void leave_dir(tree_print_state_t *s);
static void indent_prefix(tree_print_state_t *s);
static void unindent_prefix(tree_print_state_t *s);
static void set_prefix_char(tree_print_state_t *s, char c);
static void print_tree_entry(tree_print_state_t *s, const char path[]);
static void print_entry_prefix(tree_print_state_t *s);
static char ** list_sorted_files(const char path[], int *len);
static int path_sorter(const void *first, const void *second);
TSTATIC void view_stream(FILE *fp, int wrapped);
static int shift_line(char line[], size_t len, size_t offset);
static size_t add_to_line(FILE *fp, size_t max, char line[], size_t len);
static void write_message(const char msg[]);
static void cleanup_for_text(void);
static char * get_viewer_command(const char viewer[]);
static char * get_typed_fname(const char path[]);

void
toggle_quick_view(void)
{
	if(curr_stats.view)
	{
		curr_stats.view = 0;

		if(ui_view_is_visible(other_view))
		{
			/* Force cleaning possible leftovers of graphics, otherwise curses
			 * internal structures don't know that those parts need to be redrawn on
			 * the screen. */
			if(curr_stats.preview_cleanup != NULL || curr_stats.graphics_preview)
			{
				qv_cleanup(other_view, curr_stats.preview_cleanup);
			}

			draw_dir_list(other_view);
			refresh_view_win(other_view);
		}

		update_string(&curr_stats.preview_cleanup, NULL);
		curr_stats.graphics_preview = 0;
	}
	else
	{
		curr_stats.view = 1;
		quick_view_file(curr_view);
	}
}

void
quick_view_file(FileView *view)
{
	char path[PATH_MAX];
	const dir_entry_t *entry;

	if(curr_stats.load_stage < 2 || curr_stats.number_of_windows == 1 ||
	   vle_mode_is(VIEW_MODE) || draw_abandoned_view_mode())
	{
		return;
	}

	ui_view_erase(other_view);

	entry = &view->dir_entry[view->list_pos];
	get_full_path_of(entry, sizeof(path), path);

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
	refresh_view_win(other_view);

	ui_view_title_update(other_view);
}

/* Displays contents of file or output of its viewer in the other pane
 * starting from the second line and second column. */
static void
view_file(const char path[])
{
	int graphics = 0;
	const char *viewer;
	const char *clean_cmd;
	FILE *fp;

	viewer = qv_get_viewer(path);

	if(viewer == NULL && is_dir(path))
	{
		fp = view_dir(path, ui_qv_height(other_view));
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
		graphics = is_graphics_viewer(viewer);
		/* If graphics will be displayed, clear the window and wait a bit to let
		 * terminal emulator do actual refresh (at least some of them need this). */
		if(graphics)
		{
			qv_cleanup(other_view, curr_stats.preview_cleanup);
			usleep(50000);
		}
		fp = use_info_prog(viewer);
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
	if(!graphics)
	{
		cleanup_for_text();
	}
	curr_stats.graphics_preview = graphics;

	clean_cmd = (viewer != NULL) ? ma_get_clean_cmd(viewer) : NULL;
	update_string(&curr_stats.preview_cleanup, clean_cmd);

	wattrset(other_view->win, 0);
	view_stream(fp, cfg.wrap_quick_view);
	fclose(fp);

	ui_cancellation_disable();
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
			fprintf(fp, "\n%d director%s, %d file%s",
					s.ndirs, (s.ndirs == 1) ? "y" : "ies",
					s.nfiles, (s.nfiles == 1) ? "" : "s");
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
	for(i = 0; i < len && !reached_limit; ++i)
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

		/* If is_dir_empty() returns non-zero than we know that it's directory and
		 * no additional checks are needed. */
		if(!is_dir_empty(full_path))
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
	print_tree_entry(s, path);

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
	print_tree_entry(s, path);

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
print_tree_entry(tree_print_state_t *s, const char path[])
{
	print_entry_prefix(s);
	fputs(get_last_path_component(path), s->fp);
	if(is_dir(path) && !ends_with_slash(path))
	{
		fputc('/', s->fp);
	}
	fputc('\n', s->fp);
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

/* Enumerates content of the path in sorted order.  Returns list of names of
 * lengths *len, which can be NULL on empty list, error is indicated by negative
 * *len. */
static char **
list_sorted_files(const char path[], int *len)
{
	DIR *dir;
	struct dirent *d;
	char **list = NULL;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		*len = -1;
		return NULL;
	}

	*len = 0;
	while((d = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			*len = add_to_string_array(&list, *len, 1, d->d_name);
		}
	}
	os_closedir(dir);

	if(*len != 0)
	{
		qsort(list, *len, sizeof(*list), &path_sorter);
	}

	return list;
}

/* Wraps stroscmp() for use with qsort(). */
static int
path_sorter(const void *first, const void *second)
{
	const char *const *const a = first;
	const char *const *const b = second;
	return stroscmp(*a, *b);
}

/* Displays contents read from the fp in the other pane starting from the second
 * line and second column.  The wrapped parameter determines whether lines
 * should be wrapped. */
TSTATIC void
view_stream(FILE *fp, int wrapped)
{
	const size_t left = ui_qv_left(other_view);
	const size_t top = ui_qv_top(other_view);
	const size_t max_width = ui_qv_width(other_view);
	const size_t max_height = ui_qv_height(other_view);

	const col_scheme_t *cs = ui_view_get_cs(other_view);
	char line[PREVIEW_LINE_BUF_LEN];
	int line_continued = 0;
	size_t y = top;
	const char *res;
	esc_state state;

	esc_state_init(&state, &cs->color[WIN_COLOR]);

	skip_bom(fp);
	res = get_line(fp, line, sizeof(line));

	while(res != NULL && y < top + max_height)
	{
		int offset;
		int printed;
		const size_t len = add_to_line(fp, max_width, line, sizeof(line));
		if(!wrapped && line[len - 1] != '\n')
		{
			skip_until_eol(fp);
		}

		offset = esc_print_line(line, other_view->win, left, y, max_width, 0,
				&state, &printed);
		y += !wrapped || (!line_continued || printed);
		line_continued = line[len - 1] != '\n';

		if(y < top + max_height && (!wrapped || shift_line(line, len, offset)))
		{
			res = get_line(fp, line, sizeof(line));
		}
	}
}

/* Shifts characters in the line of length len, so that characters at the offset
 * position are moved to the beginning of the line.  Returns non-zero if new
 * buffer should be treated as empty. */
static int
shift_line(char line[], size_t len, size_t offset)
{
	const size_t shift_width = len - offset;
	if(shift_width != 0)
	{
		memmove(line, line + offset, shift_width + 1);
		return (shift_width == 1 && line[0] == '\n');
	}
	return 1;
}

/* Tries to add more characters from the fp file, but not exceed length of the
 * line buffer (the len parameter) and maximum number of printable character
 * positions (the max parameter).  Returns new length of the line buffer. */
static size_t
add_to_line(FILE *fp, size_t max, char line[], size_t len)
{
	size_t n_len = utf8_nstrlen(line) - esc_str_overhead(line);
	size_t curr_len = strlen(line);
	while(n_len < max && line[curr_len - 1] != '\n' && !feof(fp))
	{
		if(get_line(fp, line + curr_len, len - curr_len) == NULL)
		{
			break;
		}
		n_len = utf8_nstrlen(line) - esc_str_overhead(line);
		curr_len = strlen(line);
	}
	return curr_len;
}

/* Writes single line message of error or information kind instead of real
 * preview. */
static void
write_message(const char msg[])
{
	cleanup_for_text();
	wattrset(other_view->win, 0);
	mvwaddstr(other_view->win, ui_qv_top(other_view), ui_qv_left(other_view),
			msg);
}

/* Ensures that view is ready to display regular text. */
static void
cleanup_for_text(void)
{
	if(curr_stats.preview_cleanup != NULL || curr_stats.graphics_preview)
	{
		qv_cleanup(other_view, curr_stats.preview_cleanup);
	}
	update_string(&curr_stats.preview_cleanup, NULL);
	curr_stats.graphics_preview = 0;
}

void
preview_close(void)
{
	if(curr_stats.view)
	{
		toggle_quick_view();
	}
	if(lwin.explore_mode)
	{
		view_explore_mode_quit(&lwin);
	}
	if(rwin.explore_mode)
	{
		view_explore_mode_quit(&rwin);
	}
}

FILE *
use_info_prog(const char viewer[])
{
	FILE *fp;
	char *cmd;

	cmd = get_viewer_command(viewer);
	fp = read_cmd_output(cmd);
	free(cmd);

	return fp;
}

/* Returns a pointer to newly allocated memory, which should be released by the
 * caller. */
static char *
get_viewer_command(const char viewer[])
{
	char *result;
	if(strchr(viewer, '%') == NULL)
	{
		char *escaped;
		FileView *view = curr_stats.preview_hint;
		if(view == NULL)
		{
			view = curr_view;
		}

		escaped = shell_like_escape(get_current_file_name(view), 0);
		result = format_str("%s %s", viewer, escaped);
		free(escaped);
	}
	else
	{
		result = expand_macros(viewer, NULL, NULL, 1);
	}
	return result;
}

void
qv_cleanup(FileView *view, const char cmd[])
{
	FileView *const curr = curr_view;
	FILE *fp;

	if(cmd == NULL)
	{
		ui_view_wipe(view);
		return;
	}

	curr_view = view;
	curr_stats.clear_preview = 1;
	fp = use_info_prog(cmd);
	curr_stats.clear_preview = 0;
	curr_view = curr;

	while(fgetc(fp) != EOF);
	fclose(fp);

	ui_view_wipe(view);
}

const char *
qv_get_viewer(const char path[])
{
	char *const typed_fname = get_typed_fname(path);
	const char *const viewer = ft_get_viewer(typed_fname);
	free(typed_fname);
	return viewer;
}

/* Gets typed filename (not path, just name).  Allocates memory, that should be
 * freed by the caller. */
static char *
get_typed_fname(const char path[])
{
	const char *const last_part = get_last_path_component(path);
	return is_dir(path) ? format_str("%s/", last_part) : strdup(last_part);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
