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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "../config.h"

#include <regex.h>

#include <curses.h>

#include <assert.h>
#include <string.h>

#include "cmdline.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "keys.h"
#include "macros.h"
#include "menus.h"
#include "modes.h"
#include "normal.h"
#include "status.h"
#include "string_array.h"
#include "ui.h"
#include "utf8.h"
#include "utils.h"

#include "view.h"

typedef struct
{
	char **lines;
	int (*widths)[2];
	int nlines;
	int nlinesv;
	int line;
	int linev;
	int win;
	int half_win;
	int width;
	FileView *view;
	regex_t re;
	int last_search_backward;
	int search_repeat;
}view_info_t;

static int can_be_explored(FileView *view, char *buf);
static void pick_vi(void);
static void init_view_info(view_info_t *vi);
static void calc_vlines(void);
static void draw(void);
static int gets_line(const char *line, int offset, int max_len, char *buf);
static void puts_line(FileView *view, char *line);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_tab(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_qmark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void find_previous(int o);
static void find_next(int o);
static void cmd_q(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_v(key_info_t key_info, keys_info_t *keys_info);
static void cmd_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z(key_info_t key_info, keys_info_t *keys_info);

static int *mode;
view_info_t view_info[3];
view_info_t* vi = &view_info[0];

static keys_add_info_t builtin_cmds[] = {
	{L"\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{L"\x04", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"\x05", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x06", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"\x09", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_tab}}},
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
	{L"/", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L"?", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_qmark}}},
	{L"<", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L">", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"\033"L"<", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L"\033"L">", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"\033"L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_space}}},
#ifndef _WIN32
	{L"\033"L"v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
#else
	{{ALT_V}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
#endif
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"N", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"Q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"R", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q}}},
	{L"b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_b}}},
	{L"d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"g", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_g}}},
	{L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
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

	init_view_info(&view_info[0]);
	init_view_info(&view_info[1]);
	init_view_info(&view_info[2]);
}

void
enter_view_mode(int explore)
{
	char buf[PATH_MAX];
#ifndef _WIN32
	const char *viewer;
#endif
	FILE *fp;

	if(!can_be_explored(curr_view, buf))
	{
		(void)show_error_msg("File exploring", "The file cannot be explored");
		return;
	}

#ifndef _WIN32
	viewer = get_viewer_for_file(buf);
	if(viewer != NULL && viewer[0] != '\0')
		fp = use_info_prog(viewer);
	else
#endif
		fp = fopen(buf, "r");

	if(fp == NULL)
	{
		(void)show_error_msg("File exploring", "Cannot open file for reading");
		return;
	}

	if(!explore)
		vi = &view_info[0];
	else if(curr_view == &lwin)
		vi = &view_info[1];
	else
		vi = &view_info[2];

	vi->lines = read_file_lines(fp, &vi->nlines);
	fclose(fp);

	if(vi->lines == NULL)
	{
		(void)show_error_msg("File exploring", "Wont explore empty file");
		return;
	}
	vi->widths = malloc(sizeof(*vi->widths)*vi->nlines);
	if(vi->widths == NULL)
	{
		free_string_array(vi->lines, vi->nlines);
		vi->lines = NULL;
		vi->nlines = 0;
		return;
	}

	*mode = VIEW_MODE;
	update_view_title(&lwin);
	update_view_title(&rwin);

	if(explore)
	{
		vi->view = curr_view;
		curr_view->explore_mode = 1;
	}
	else
	{
		vi->view = other_view;
	}

	update_view_title(vi->view);
	view_redraw();
}

/* assumed that buf size is at least PATH_MAX characters */
static int
can_be_explored(FileView *view, char *buf)
{
	char link[PATH_MAX];

	snprintf(buf, PATH_MAX, "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);
	switch(view->dir_entry[view->list_pos].type)
	{
		case DEVICE:
			return 0;
#ifndef _WIN32
		case SOCKET:
			return 0;
#endif
		case FIFO:
			return 0;
		case LINK:
			if(get_link_target(buf, link, sizeof(link)) != 0)
				return 0;
			if(is_path_absolute(link))
				strcpy(buf, link);
			else
				snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, link);
			if(access(buf, R_OK) != 0)
				return 0;
			break;
	}
	return 1;
}

void
activate_view_mode(void)
{
	*mode = VIEW_MODE;
	pick_vi();
}

static void
pick_vi(void)
{
	if(!curr_view->explore_mode)
		vi = &view_info[0];
	else if(curr_view == &lwin)
		vi = &view_info[1];
	else
		vi = &view_info[2];
}

void
view_pre(void)
{
	if(curr_stats.save_msg != 0)
		return;

	status_bar_message("-- VIEW -- ");
	curr_stats.save_msg = 2;
}

void
view_post(void)
{
	char buf[13];

	werase(pos_win);
	snprintf(buf, sizeof(buf), "%d-%d ", vi->line + 1, vi->nlines);
	mvwaddstr(pos_win, 0, 13 - strlen(buf), buf);
	wnoutrefresh(pos_win);
}

void
view_redraw(void)
{
	view_info_t *tmp = vi;

	if(lwin.explore_mode)
	{
		vi = &view_info[1];
		calc_vlines();
		draw();
	}
	if(rwin.explore_mode)
	{
		vi = &view_info[2];
		calc_vlines();
		draw();
	}
	if(!lwin.explore_mode && !rwin.explore_mode)
	{
		calc_vlines();
		draw();
	}

	vi = tmp;
}

void
leave_view_mode(void)
{
	*mode = NORMAL_MODE;

	if(curr_view->explore_mode)
	{
		curr_view->explore_mode = 0;
		draw_dir_list(curr_view, curr_view->top_line);
		move_to_list_pos(curr_view, curr_view->list_pos);
	}
	else
	{
		quick_view_file(curr_view);
	}

	update_view_title(curr_view);

	free_string_array(vi->lines, vi->nlines);
	free(vi->widths);
	if(vi->last_search_backward != -1)
		regfree(&vi->re);
	init_view_info(vi);
}

static void
init_view_info(view_info_t *vi)
{
	vi->lines = NULL;
	vi->nlines = 0;
	vi->nlinesv = 0;
	vi->line = 0;
	vi->linev = 0;
	vi->win = -1;
	vi->half_win = -1;
	vi->width = -1;
	vi->last_search_backward = -1;
}

static void
calc_vlines(void)
{
	if(vi->view->window_width - 1 == vi->width)
		return;

	vi->width = vi->view->window_width - 1;

	if(cfg.wrap_quick_view)
	{
		int i;
		vi->nlinesv = 0;
		for(i = 0; i < vi->nlines; i++)
		{
			vi->widths[i][0] = vi->nlinesv++;
			vi->widths[i][1] = get_utf8_string_length(vi->lines[i]);
			vi->nlinesv += vi->widths[i][1]/vi->width;
		}
	}
	else
	{
		int i;
		vi->nlinesv = vi->nlines;
		for(i = 0; i < vi->nlines; i++)
		{
			vi->widths[i][0] = i;
			vi->widths[i][1] = vi->width;
		}
	}
}

static void
draw(void)
{
	int l, vl;
	int max = MIN(vi->line + vi->view->window_rows - 1, vi->nlines);
	werase(vi->view->win);
	for(vl = 0, l = vi->line; l < max && vl < vi->view->window_rows - 1; l++)
	{
		int offset = 0;
		int t = 0;
		do
		{
			char buf[vi->view->window_width*4];
			offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1, buf);

			if(l != vi->line || vl + t >= vi->linev - vi->widths[vi->line][0])
			{
				wmove(vi->view->win, 1 + vl, 1);
				puts_line(vi->view, buf);
				vl++;
			}
			t++;
		}
		while(vi->lines[l][offset] != '\0' && vl < vi->view->window_rows - 1 &&
				cfg.wrap_quick_view);
	}
	wrefresh(vi->view->win);
}

static int
gets_line(const char *line, int offset, int max_len, char *buf)
{
	int i = 0;
	size_t len = 0;
	buf[0] = '\0';
	while(i < max_len && line[offset] != '\0')
	{
		size_t char_width = get_char_width(line + offset);
		if(char_width == 1 && line[offset] == '\t')
		{
			int k;
			char_width = cfg.tab_stop - i%cfg.tab_stop;

			k = char_width;
			while(k-- > 0)
				strcat(buf + len, " ");

			offset += 1;
			i += char_width;
		}
		else
		{
			snprintf(buf + len, char_width + 1, "%s", line + offset);
			offset += char_width;
			i += 1;
		}
		len += char_width;
	}
	return offset;
}

static void
puts_line(FileView *view, char *line)
{
	regmatch_t match;
	char c;

	if(vi->last_search_backward == -1)
	{
		wprint(view->win, line);
		return;
	}

	if(regexec(&vi->re, line, 1, &match, 0) != 0)
	{
		wprint(view->win, line);
		return;
	}

	c = line[match.rm_so];
	line[match.rm_so] = '\0';
	wprint(view->win, line);
	line[match.rm_so] = c;

	wattron(view->win, A_REVERSE | A_BOLD);

	c = line[match.rm_eo];
	line[match.rm_eo] = '\0';
	wprint(view->win, line + match.rm_so);
	line[match.rm_eo] = c;

	wattroff(view->win, A_REVERSE | A_BOLD);

	wprint(view->win, line + match.rm_eo);
}

int
find_vwpattern(const char *pattern, int backward)
{
	int err;

	if(pattern == NULL)
		return 0;

	if(vi->last_search_backward != -1)
		regfree(&vi->re);
	vi->last_search_backward = -1;
	if((err = regcomp(&vi->re, pattern, get_regexp_cflags(pattern))) != 0)
	{
		status_bar_errorf("Filter not set: %s", get_regexp_error(err, &vi->re));
		regfree(&vi->re);
		draw();
		return 1;
	}

	vi->last_search_backward = backward;

	while(vi->search_repeat-- > 0)
	{
		if(backward)
			find_previous(0);
		else
			find_next(0);
	}

	return curr_stats.save_msg;
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	view_redraw();
}

static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(0);
	view_redraw();
}

static void
cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(1);
	view_redraw();
}

static void
cmd_meta_space(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(vi->win > 0)
			key_info.count = vi->win;
		else
			key_info.count = vi->view->window_rows - 2;
	}
	key_info.reg = 1;
	cmd_j(key_info, keys_info);
}

static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 0;
	if(key_info.count > 100)
		key_info.count = 100;

	vi->line = (key_info.count*vi->nlinesv)/100;
	if(vi->line >= vi->nlines)
		vi->line = vi->nlines - 1;
	vi->linev = vi->widths[vi->line][0];
	draw();
}

static void
cmd_tab(key_info_t key_info, keys_info_t *keys_info)
{
	if(!curr_view->explore_mode)
	{
		leave_view_mode();
		return;
	}

	change_window();
	if(!curr_view->explore_mode)
		*mode = NORMAL_MODE;
	pick_vi();

	update_view_title(&lwin);
	update_view_title(&rwin);
}

static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		vi->search_repeat = 1;
	else
		vi->search_repeat = key_info.count;
	enter_cmdline_mode(VIEW_SEARCH_FORWARD_SUBMODE, L"", NULL);
}

static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		vi->search_repeat = 1;
	else
		vi->search_repeat = key_info.count;
	enter_cmdline_mode(VIEW_SEARCH_BACKWARD_SUBMODE, L"", NULL);
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
	{
		cmd_g(key_info, keys_info);
		return;
	}

	if(vi->linev + 1 + vi->view->window_rows - 1 > vi->nlinesv)
		return;

	vi->linev = vi->nlinesv - (vi->view->window_rows - 1);
	for(vi->line = 0; vi->line < vi->nlines - 1; vi->line++)
		if(vi->linev < vi->widths[vi->line + 1][0])
			break;

	draw();
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	while(key_info.count-- > 0 && curr_stats.save_msg == 0)
	{
		if(vi->last_search_backward)
			find_next(1);
		else
			find_previous(1);
	}
}

static void
cmd_b(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(vi->win > 0)
			key_info.count = vi->win;
		else
			key_info.count = vi->view->window_rows - 2;
	}
	cmd_k(key_info, keys_info);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		vi->half_win = key_info.count;
	else if(vi->half_win > 0)
		key_info.count = vi->half_win;
	else
		key_info.count = (vi->view->window_rows - 1)/2;
	cmd_j(key_info, keys_info);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		if(vi->win > 0)
			key_info.count = vi->win;
		else
			key_info.count = vi->view->window_rows - 2;
	}
	cmd_j(key_info, keys_info);
}

static void
cmd_g(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	key_info.count = MIN(vi->nlinesv - (vi->view->window_rows - 1),
			key_info.count);
	key_info.count = MAX(0, key_info.count);

	if(vi->linev == vi->widths[key_info.count - 1][0])
		return;
	vi->line = key_info.count - 1;
	vi->linev = vi->widths[vi->line][0];
	draw();
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
	{
		if((vi->linev + 1) + (vi->view->window_rows - 1) > vi->nlinesv)
			return;
	}
	else
	{
		if(vi->linev + 1 > vi->nlinesv)
			return;
	}

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.count = MIN(key_info.count,
				vi->nlinesv - (vi->view->window_rows - 1) - vi->linev);
	else
		key_info.count = MIN(key_info.count, vi->nlinesv - vi->linev - 1);

	while(key_info.count-- > 0)
	{
		size_t height = (vi->widths[vi->line][1] + vi->width - 1)/vi->width;
		height = MAX(height, 1);
		if(vi->linev + 1 >= vi->widths[vi->line][0] + height)
			vi->line++;

		vi->linev++;
	}

	draw();
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->linev == 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	key_info.count = MIN(key_info.count, vi->linev);

	while(key_info.count-- > 0)
	{
		if(vi->linev - 1 < vi->widths[vi->line][0])
			vi->line--;

		vi->linev--;
	}

	draw();
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	while(key_info.count-- > 0 && curr_stats.save_msg == 0)
	{
		if(vi->last_search_backward)
			find_previous(1);
		else
			find_next(1);
	}
}

static void
find_previous(int o)
{
	int i;
	int offset = 0;
	char buf[(vi->view->window_width - 1)*4];
	int vl, l;

	if(vi->last_search_backward == -1)
		return;

	vl = vi->linev - o;
	l = vi->line;

	if(l > 0 && vl < vi->widths[l][0])
		l--;

	for(i = 0; i <= vl - vi->widths[l][0]; i++)
		offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1, buf);

	while(l > 0)
	{
		if(regexec(&vi->re, buf, 0, NULL, 0) == 0)
		{
			vi->linev = vl;
			vi->line = l;
			break;
		}
		if(l > 0 && vl - 1 < vi->widths[l][0])
		{
			l--;
			offset = 0;
			for(i = 0; i <= vl - 1 - vi->widths[l][0]; i++)
				offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1,
						buf);
		}
		else
			offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1, buf);
		vl--;
	}
	draw();
	if(vi->line != l)
	{
		status_bar_error("Pattern not found");
		curr_stats.save_msg = 1;
	}
}

static void
find_next(int o)
{
	int i;
	int offset = 0;
	char buf[(vi->view->window_width - 1)*4];
	int vl, l;

	if(vi->last_search_backward == -1)
		return;

	vl = vi->linev + 1;
	l = vi->line;

	if(l < vi->nlines && vl == vi->widths[l + 1][0])
		l++;

	for(i = 0; i <= vl - vi->widths[l][0]; i++)
		offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1, buf);

	while(l < vi->nlines)
	{
		if(regexec(&vi->re, buf, 0, NULL, 0) == 0)
		{
			vi->linev = vl;
			vi->line = l;
			break;
		}
		if(l < vi->nlines && (l == vi->nlines - 1 ||
				vl + 1 >= vi->widths[l + 1][0]))
		{
			if(l == vi->nlines - 1)
				break;
			l++;
			offset = 0;
		}
		offset = gets_line(vi->lines[l], offset, vi->view->window_width - 1, buf);
		vl++;
	}
	draw();
	if(vi->line != l)
	{
		status_bar_error("Pattern not found");
		curr_stats.save_msg = 1;
	}
}

static void
cmd_q(key_info_t key_info, keys_info_t *keys_info)
{
	leave_view_mode();
}

static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		vi->half_win = key_info.count;
	else if(vi->half_win > 0)
		key_info.count = vi->half_win;
	else
		key_info.count = (vi->view->window_rows - 1)/2;
	cmd_k(key_info, keys_info);
}

static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	view_file(buf, vi->line + (vi->view->window_rows - 1)/2, 1);
}

static void
cmd_w(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		vi->win = key_info.count;
	else if(vi->win > 0)
		key_info.count = vi->win;
	else
		key_info.count = vi->view->window_rows - 2;
	cmd_k(key_info, keys_info);
}

static void
cmd_z(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		vi->win = key_info.count;
	else if(vi->win > 0)
		key_info.count = vi->win;
	else
		key_info.count = vi->view->window_rows - 2;
	cmd_j(key_info, keys_info);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
