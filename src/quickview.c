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

#include <curses.h> /* mvwaddstr() werase() wattrset() */

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fclose() fdopen() feof() */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() strlen() strncat() */

#include "cfg/config.h"
#include "compat/os.h"
#include "engine/mode.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "modes/view.h"
#include "ui/ui.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
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
#include "macros.h"
#include "status.h"
#include "types.h"

/* Line at which quickview content should be displayed. */
#define LINE 1
/* Column at which quickview content should be displayed. */
#define COL 1

/* Size of buffer holding preview line (in characters). */
#define PREVIEW_LINE_BUF_LEN 4096

static void view_file(FILE *fp, int wrapped);
static int shift_line(char line[], size_t len, size_t offset);
static size_t add_to_line(FILE *fp, size_t max, char line[], size_t len);
static char * get_viewer_command(const char viewer[]);

void
toggle_quick_view(void)
{
	if(curr_stats.view)
	{
		curr_stats.view = 0;

		if(ui_view_is_visible(other_view))
		{
			draw_dir_list(other_view);
			refresh_view_win(other_view);
		}
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

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(vle_mode_is(VIEW_MODE))
	{
		return;
	}

	if(curr_stats.number_of_windows == 1)
	{
		return;
	}

	if(draw_abandoned_view_mode())
	{
		return;
	}

	ui_view_erase(other_view);

	entry = &view->dir_entry[view->list_pos];
	get_full_path_of(entry, sizeof(path), path);

	switch(view->dir_entry[view->list_pos].type)
	{
		case FT_CHAR_DEV:
			mvwaddstr(other_view->win, LINE, COL, "File is a Character Device");
			break;
		case FT_BLOCK_DEV:
			mvwaddstr(other_view->win, LINE, COL, "File is a Block Device");
			break;
#ifndef _WIN32
		case FT_SOCK:
			mvwaddstr(other_view->win, LINE, COL, "File is a Socket");
			break;
#endif
		case FT_FIFO:
			mvwaddstr(other_view->win, LINE, COL, "File is a Named Pipe");
			break;
		case FT_LINK:
			if(get_link_target_abs(path, entry->origin, path, sizeof(path)) != 0)
			{
				mvwaddstr(other_view->win, LINE, COL, "Cannot resolve Link");
				break;
			}
			if(!ends_with_slash(path) && is_dir(path))
			{
				strncat(path, "/", sizeof(path) - strlen(path) - 1);
			}
			/* break intensionally omitted */
		case FT_UNK:
		default:
			{
				const char *viewer;
				FILE *fp;

				char *const typed_fname = get_typed_fname(path);
				viewer = ft_get_viewer(typed_fname);
				free(typed_fname);

				if(viewer == NULL && is_dir(path))
				{
					mvwaddstr(other_view->win, LINE, COL, "File is a Directory");
					break;
				}
				if(is_null_or_empty(viewer))
				{
					fp = os_fopen(path, "rb");
				}
				else
				{
					fp = use_info_prog(viewer);
				}

				if(fp == NULL)
				{
					mvwaddstr(other_view->win, LINE, COL, "Cannot open file");
					break;
				}

				ui_view_clear(other_view);
				wattrset(other_view->win, 0);
				view_file(fp, cfg.wrap_quick_view);

				fclose(fp);
				break;
			}
	}
	refresh_view_win(other_view);

	ui_view_title_update(other_view);
}

/* Displays contents read from the fp in the other pane starting from the second
 * line and second column.  The wrapped parameter determines whether lines
 * should be wrapped. */
static void
view_file(FILE *fp, int wrapped)
{
	const size_t max_width = other_view->window_width - 1;
	const size_t max_y = other_view->window_rows - 1;

	const col_scheme_t *cs = ui_view_get_cs(other_view);
	char line[PREVIEW_LINE_BUF_LEN];
	int line_continued = 0;
	int y = LINE;
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
	size_t n_len = get_normal_utf8_string_length(line) - esc_str_overhead(line);
	size_t curr_len = strlen(line);
	while(n_len < max && line[curr_len - 1] != '\n' && !feof(fp))
	{
		if(get_line(fp, line + curr_len, len - curr_len) == NULL)
		{
			break;
		}
		n_len = get_normal_utf8_string_length(line) - esc_str_overhead(line);
		curr_len = strlen(line);
	}
	return curr_len;
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
		char *const escaped = escape_filename(get_current_file_name(curr_view), 0);
		result = format_str("%s %s", viewer, escaped);
		free(escaped);
	}
	else
	{
		result = expand_macros(viewer, NULL, NULL, 1);
	}
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
