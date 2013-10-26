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

#include <stddef.h> /* NULL size_t */
#include <string.h> /* memmove() strlen() strncat() */

#include "cfg/config.h"
#include "modes/modes.h"
#include "modes/view.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utf8.h"
#include "color_manager.h"
#include "escape.h"
#include "filelist.h"
#include "filetype.h"
#include "status.h"
#include "ui.h"

/* Line at which quickview content should be displayed. */
#define LINE 1
/* Column at which quickview content should be displayed. */
#define COL 1

/* Size of buffer holding preview line (in characters). */
#define PREVIEW_LINE_BUF_LEN 4096

static void view_file(FILE *fp, int wrapped);
static int shift_line(char line[], size_t len, size_t offset);
static size_t add_to_line(FILE *fp, size_t max, char line[], size_t len);

void
toggle_quick_view(void)
{
	if(curr_stats.view)
	{
		curr_stats.view = 0;

		draw_dir_list(other_view);
		refresh_view_win(other_view);
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
	char buf[PATH_MAX];

	if(curr_stats.load_stage < 2)
		return;

	if(get_mode() == VIEW_MODE)
		return;

	if(curr_stats.number_of_windows == 1)
		return;

	if(draw_abandoned_view_mode())
	{
		return;
	}

	werase(other_view->win);
	werase(other_view->title);
	mvwaddstr(other_view->title, 0, 0, "File: ");
	wprint(other_view->title, view->dir_entry[view->list_pos].name);

	snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	switch(view->dir_entry[view->list_pos].type)
	{
		case CHARACTER_DEVICE:
			mvwaddstr(other_view->win, LINE, COL, "File is a Character Device");
			break;
		case BLOCK_DEVICE:
			mvwaddstr(other_view->win, LINE, COL, "File is a Block Device");
			break;
#ifndef _WIN32
		case SOCKET:
			mvwaddstr(other_view->win, LINE, COL, "File is a Socket");
			break;
#endif
		case FIFO:
			mvwaddstr(other_view->win, LINE, COL, "File is a Named Pipe");
			break;
		case LINK:
			if(get_link_target_abs(buf, view->curr_dir, buf, sizeof(buf)) != 0)
			{
				mvwaddstr(other_view->win, LINE, COL, "Cannot resolve Link");
				break;
			}
			if(!ends_with_slash(buf) && is_dir(buf))
			{
				strncat(buf, "/", sizeof(buf) - strlen(buf) - 1);
			}
			/* break intensionally omitted */
		case UNKNOWN:
		default:
			{
				const char *viewer;
				FILE *fp;

				viewer = get_viewer_for_file(get_last_path_component(buf));
				if(viewer == NULL && is_dir(buf))
				{
					mvwaddstr(other_view->win, LINE, COL, "File is a Directory");
					break;
				}
				if(is_null_or_empty(viewer))
					fp = fopen(buf, "rb");
				else
					fp = use_info_prog(viewer);

				if(fp == NULL)
				{
					mvwaddstr(other_view->win, LINE, COL, "Cannot open file");
					break;
				}

				colmgr_reset();
				wattrset(other_view->win, 0);
				view_file(fp, cfg.wrap_quick_view);

				fclose(fp);
			}
			break;
	}
	refresh_view_win(other_view);
	wrefresh(other_view->title);
}

/* Displays contents read from the fp in the other pane starting from the second
 * line and second column.  The wrapped parameter determines whether lines
 * should be wrapped. */
static void
view_file(FILE *fp, int wrapped)
{
	const size_t max_width = other_view->window_width - 1;
	const size_t max_y = other_view->window_rows - 1;

	char line[PREVIEW_LINE_BUF_LEN];
	int line_continued = 0;
	int y = LINE;
	const char *res = get_line(fp, line, sizeof(line));
	esc_state state;
	esc_state_init(&state, &other_view->cs.color[WIN_COLOR]);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
