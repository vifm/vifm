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

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fclose() fdopen() feof() */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() strlen() strncat() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/mode.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "modes/view.h"
#include "ui/ui.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "colors.h"
#include "escape.h"
#include "filelist.h"
#include "filetype.h"
#include "fileview.h"
#include "macros.h"
#include "status.h"
#include "types.h"

/* Line at which quickview content should be displayed. */
#define LINE 1
/* Column at which quickview content should be displayed. */
#define COL 1

/* Size of buffer holding preview line (in characters). */
#define PREVIEW_LINE_BUF_LEN 4096

static void view_file(const char path[]);
static void view_stream(FILE *fp, int wrapped);
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

	viewer = gv_get_viewer(path);

	if(viewer == NULL && is_dir(path))
	{
		write_message("File is a Directory");
		return;
	}

	if(is_null_or_empty(viewer))
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
}

/* Displays contents read from the fp in the other pane starting from the second
 * line and second column.  The wrapped parameter determines whether lines
 * should be wrapped. */
static void
view_stream(FILE *fp, int wrapped)
{
	const size_t max_width = other_view->window_width - 1;
	const size_t max_y = other_view->window_rows - 1;

	const col_scheme_t *cs = ui_view_get_cs(other_view);
	char line[PREVIEW_LINE_BUF_LEN];
	int line_continued = 0;
	size_t y = LINE;
	const char *res = get_line(fp, line, sizeof(line));
	esc_state state;

	esc_state_init(&state, &cs->color[WIN_COLOR]);

	while(res != NULL && y <= max_y)
	{
		int offset;
		int printed;
		const size_t len = add_to_line(fp, max_width, line, sizeof(line));
		if(!wrapped && line[len - 1] != '\n')
		{
			skip_until_eol(fp);
		}

		offset = esc_print_line(line, other_view->win, COL, y, max_width, 0, &state,
				&printed);
		y += !wrapped || (!line_continued || printed);
		line_continued = line[len - 1] != '\n';

		if(!wrapped || shift_line(line, len, offset))
		{
			res = get_line(fp, line, sizeof(line));
		}
	}
}

/* Shifts characters in the line of length len, so that characters at the offset
 * position are moved to the beginning of the line.  Returns non-zero if new
 * buffer should be threated as empty. */
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
	mvwaddstr(other_view->win, LINE, COL, msg);
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
gv_get_viewer(const char path[])
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
