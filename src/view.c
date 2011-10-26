/* vifm
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "../config.h"

#include <curses.h>

#include <assert.h>
#include <string.h>

#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "keys.h"
#include "macros.h"
#include "modes.h"
#include "normal.h"
#include "status.h"
#include "string_array.h"
#include "ui.h"
#include "utf8.h"
#include "utils.h"

#include "view.h"

static void leave_view_mode(void);
static void calc_vlines(void);
static void draw(void);
static void draw_wraped(FileView *view);
static void draw_not_wraped(FileView *view);
static void cmd_ctrl_l(struct key_info, struct keys_info *);
static void cmd_ctrl_ws(struct key_info, struct keys_info *);
static void cmd_ctrl_wv(struct key_info, struct keys_info *);
static void cmd_meta_space(struct key_info, struct keys_info *);
static void cmd_percent(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_b(struct key_info, struct keys_info *);
static void cmd_d(struct key_info, struct keys_info *);
static void cmd_f(struct key_info, struct keys_info *);
static void cmd_g(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_q(struct key_info, struct keys_info *);
static void cmd_u(struct key_info, struct keys_info *);
static void cmd_v(struct key_info, struct keys_info *);
static void cmd_w(struct key_info, struct keys_info *);
static void cmd_z(struct key_info, struct keys_info *);

static int *mode;
static char **lines;
static int (*widths)[2];
static int nlines;
static int nlinesv;
static int line;
static int linev;
static int win = -1;
static int half_win = -1;
static int width = -1;

static struct keys_add_info builtin_cmds[] = {
	{L"\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{L"\x04", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"\x05", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x06", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"\x09", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"\x0b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x12", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x15", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"\x16", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"\x17\x13", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ws}}},
	{L"\x17s", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ws}}},
	{L"\x17\x16", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17=", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wequal}}},
	{L"\x17<", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wless}}},
	{L"\x17>", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wgreater}}},
	{L"\x17+", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wplus}}},
	{L"\x17-", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wminus}}},
	{L"\x17|", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wpipe}}},
	{L"\x17_", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wpipe}}},
	{L"\x19", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_BTAB, 0}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
#else
	{L"\033"L"[Z", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
#endif
	{L"<", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L">", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"\033"L"<", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L"\033"L">", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"\033"L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_space}}},
	{L"\033"L"v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"Q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"R", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{L"d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"g", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"p", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"r", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"u", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_v}}},
	{L"y", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"w", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_w}}},
	{L"z", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_z}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{{KEY_NPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_view_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), VIEW_MODE);
	assert(ret_code == 0);
}

void
enter_view_mode(void)
{
	char buf[PATH_MAX];
	char link[PATH_MAX];
	const char *viewer;
	FILE *fp;

	snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	switch(curr_view->dir_entry[curr_view->list_pos].type)
	{
		case DEVICE:
			return;
#ifndef _WIN32
		case SOCKET:
			return;
#endif
		case FIFO:
			return;
		case LINK:
			if(get_link_target(buf, link, sizeof(link)) != 0)
				return;
			if(is_path_absolute(link))
				strcpy(buf, link);
			else
				snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir, link);
			if(access(buf, R_OK) != 0)
				return;
	}

	viewer = get_viewer_for_file(buf);
#ifndef _WIN32
	if(viewer != NULL && viewer[0] != '\0')
		fp = use_info_prog(viewer);
	else
#endif
		fp = fopen(buf, "r");

	if(fp == NULL)
		return;

	lines = read_file_lines(fp, &nlines);
	fclose(fp);

	if(lines == NULL)
		return;
	free(widths);
	widths = malloc(sizeof(*widths)*nlines);
	if(widths == NULL)
	{
		free_string_array(lines, nlines);
		lines = NULL;
		nlines = 0;
		return;
	}

	*mode = VIEW_MODE;
	update_view_title(&lwin);
	update_view_title(&rwin);

	view_redraw();
}

void
view_pre(void)
{
	status_bar_message("-- VIEW -- ");
	curr_stats.save_msg = 2;
}

void
view_post(void)
{
	char buf[13];

	werase(pos_win);
	snprintf(buf, sizeof(buf), "%d-%d ", line + 1, nlines);
	mvwaddstr(pos_win, 0, 13 - strlen(buf), buf);
	wnoutrefresh(pos_win);
}

void
view_redraw(void)
{
	calc_vlines();
	draw();
}

static void
leave_view_mode(void)
{
	*mode = NORMAL_MODE;
	quick_view_file(curr_view);
	update_view_title(curr_view);

	free_string_array(lines, nlines);
	lines = NULL;
	nlines = 0;
	nlinesv = 0;
	line = 0;
	linev = 0;
	win = -1;
	half_win = -1;
	width = -1;
}

static void
calc_vlines(void)
{
	if(other_view->window_width - 1 == width)
		return;

	width = other_view->window_width - 1;

	if(cfg.wrap_quick_view)
	{
		int i;
		nlinesv = 0;
		for(i = 0; i < nlines; i++)
		{
			widths[i][0] = nlinesv++;
			widths[i][1] = get_utf8_string_length(lines[i]);
			nlinesv += widths[i][1]/width;
		}
	}
	else
	{
		int i;
		nlinesv = nlines;
		for(i = 0; i < nlines; i++)
		{
			widths[i][0] = i;
			widths[i][1] = width;
		}
	}
}

static void
draw(void)
{
	if(cfg.wrap_quick_view)
		draw_wraped(other_view);
	else
		draw_not_wraped(other_view);
}

static void
draw_wraped(FileView *view)
{
	int l, vl;
	int max = MIN(line + view->window_rows - 1, nlines);
	werase(view->win);
	for(vl = 0, l = line; l < max && vl < view->window_rows - 1; l++)
	{
		int offset = 0;
		int t = 0;
		do
		{
			char buf[view->window_width*4];
			int i = 0;
			size_t len = 0;
			buf[0] = '\0';
			while(i < view->window_width - 1 && lines[l][offset] != '\0')
			{
				size_t char_width = get_char_width(lines[l] + offset);
				snprintf(buf + len, char_width + 1, "%s", lines[l] + offset);
				len += char_width;
				offset += char_width;
				i += 1;
			}

			if(l != line || vl + t >= linev - widths[line][0])
			{
				wmove(view->win, 1 + vl, 1);
				wprint(view->win, buf);
				vl++;
			}
			t++;
		}
		while(lines[l][offset] != '\0' && vl < view->window_rows - 1);
	}
	wrefresh(view->win);
}

static void
draw_not_wraped(FileView *view)
{
	int l;
	int max = MIN(line + view->window_rows - 1, nlines);
	werase(view->win);
	for(l = line; l < max; l++)
	{
		char buf[view->window_width*4];
		int i = 0, offset = 0;
		size_t len = 0;
		while(i < view->window_width - 1)
		{
			size_t char_width = get_char_width(lines[l] + offset);
			snprintf(buf + len, char_width + 1, "%s", lines[l] + offset);
			len += char_width;
			offset += char_width;
			i += 1;
		}

		wmove(view->win, 1 + l - line, 1);
		wprint(view->win, buf);
	}
	wrefresh(view->win);
}

static void
cmd_ctrl_l(struct key_info key_info, struct keys_info *keys_info)
{
	view_redraw();
}

static void
cmd_ctrl_ws(struct key_info key_info, struct keys_info *keys_info)
{
	comm_split(0);
	view_redraw();
}

static void
cmd_ctrl_wv(struct key_info key_info, struct keys_info *keys_info)
{
	comm_split(1);
	view_redraw();
}

static void
cmd_meta_space(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(win > 0)
			key_info.count = win;
		else
			key_info.count = other_view->window_rows - 2;
	}
	key_info.reg = 1;
	cmd_j(key_info, keys_info);
}

static void
cmd_percent(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 0;
	if(key_info.count > 100)
		key_info.count = 100;

	line = (key_info.count*nlinesv)/100;
	if(line >= nlines)
		line = nlines - 1;
	linev = widths[line][0];
	draw();
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
	{
		cmd_g(key_info, keys_info);
		return;
	}

	if(linev + 1 + other_view->window_rows - 1 > nlinesv)
		return;

	linev = nlinesv - (other_view->window_rows - 1);
	for(line = 0; line < nlines - 1; line++)
		if(linev < widths[line + 1][0])
			break;

	draw();
}

static void
cmd_b(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(win > 0)
			key_info.count = win;
		else
			key_info.count = other_view->window_rows - 2;
	}
	cmd_k(key_info, keys_info);
}

static void
cmd_d(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		half_win = key_info.count;
	else if(half_win > 0)
		key_info.count = half_win;
	else
		key_info.count = (other_view->window_rows - 1)/2;
	cmd_j(key_info, keys_info);
}

static void
cmd_f(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(win > 0)
			key_info.count = win;
		else
			key_info.count = other_view->window_rows - 2;
	}
	cmd_j(key_info, keys_info);
}

static void
cmd_g(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	key_info.count = MIN(nlinesv - (other_view->window_rows - 1), key_info.count);
	key_info.count = MAX(0, key_info.count);

	if(linev == widths[key_info.count - 1][0])
		return;
	line = key_info.count - 1;
	linev = widths[line][0];
	draw();
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
	{
		if((linev + 1) + (other_view->window_rows - 1) > nlinesv)
			return;
	}
	else
	{
		if(linev + 1 > nlinesv)
			return;
	}

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.count = MIN(key_info.count,
				nlinesv - (other_view->window_rows - 1) - linev);
	else
		key_info.count = MIN(key_info.count, nlinesv - linev - 1);

	while(key_info.count-- > 0)
	{
		size_t height = (widths[line][1] + width - 1)/width;
		height = MAX(height, 1);
		if(linev + 1 >= widths[line][0] + height)
			line++;

		linev++;
	}

	draw();
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(linev == 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	key_info.count = MIN(key_info.count, linev);

	while(key_info.count-- > 0)
	{
		if(linev - 1 < widths[line][0])
			line--;

		linev--;
	}

	draw();
}

static void
cmd_q(struct key_info key_info, struct keys_info *keys_info)
{
	leave_view_mode();
}

static void
cmd_u(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		half_win = key_info.count;
	else if(half_win > 0)
		key_info.count = half_win;
	else
		key_info.count = (other_view->window_rows - 1)/2;
	cmd_k(key_info, keys_info);
}

static void
cmd_v(struct key_info key_info, struct keys_info *keys_info)
{
	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	view_file(buf, line + (other_view->window_rows - 1)/2);
}

static void
cmd_w(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		win = key_info.count;
	else if(win > 0)
		key_info.count = win;
	else
		key_info.count = other_view->window_rows - 2;
	cmd_k(key_info, keys_info);
}

static void
cmd_z(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		win = key_info.count;
	else if(win > 0)
		key_info.count = win;
	else
		key_info.count = other_view->window_rows - 2;
	cmd_j(key_info, keys_info);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
