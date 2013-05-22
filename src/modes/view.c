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

#include <regex.h>

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <string.h> /* strcpy() strlen() */
#include <stdlib.h> /* free() */

#include "../cfg/config.h"
#include "../engine/keys.h"
#include "../menus/menus.h"
#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../color_manager.h"
#include "../commands.h"
#include "../escape.h"
#include "../filelist.h"
#include "../fileops.h"
#include "../filetype.h"
#include "../quickview.h"
#include "../running.h"
#include "../status.h"
#include "../ui.h"
#include "cmdline.h"
#include "modes.h"
#include "normal.h"

#include "view.h"

/* Column at which view content should be displayed. */
#define COL 1

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
	int wrap;
}view_info_t;

/* View information structure indexes and count. */
enum
{
	VI_QV,    /* Index of view information structure for quickview window. */
	VI_LWIN,  /* Index of view information structure for left window. */
	VI_RWIN,  /* Index of view information structure for right window. */
	VI_COUNT, /* Number of view information structures. */
};

static int get_file_to_explore(const FileView *view, char buf[],
		size_t buf_len);
static void init_view_info(view_info_t *vi);
static void redraw(void);
static void calc_vlines(void);
static void calc_vlines_wrapped(view_info_t *vi);
static void calc_vlines_non_wrapped(view_info_t *vi);
static void draw(void);
static int get_part(const char line[], int offset, size_t max_len, char part[]);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_tab(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void pick_vi(int explore);
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
view_info_t view_info[VI_COUNT];
view_info_t* vi = &view_info[VI_QV];

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
	{L"\x17H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wH}}},
	{L"\x17J", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wJ}}},
	{L"\x17K", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wK}}},
	{L"\x17L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wL}}},
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

	init_view_info(&view_info[VI_QV]);
	init_view_info(&view_info[VI_LWIN]);
	init_view_info(&view_info[VI_RWIN]);
}

void
enter_view_mode(int explore)
{
	char buf[PATH_MAX];
	const char *viewer;
	FILE *fp;

	if(get_file_to_explore(curr_view, buf, sizeof(buf)) != 0)
	{
		show_error_msg("File exploring", "The file cannot be explored");
		return;
	}

	/* FIXME: same code is in ../quickview.c */
	viewer = get_viewer_for_file(get_last_path_component(buf));
	if(is_null_or_empty(viewer))
		fp = fopen(buf, "r");
	else
		fp = use_info_prog(viewer);

	if(fp == NULL)
	{
		show_error_msg("File exploring", "Cannot open file for reading");
		return;
	}

	pick_vi(explore);
	vi->lines = read_file_lines(fp, &vi->nlines);
	fclose(fp);

	if(vi->lines == NULL)
	{
		show_error_msg("File exploring", "Won't explore empty file");
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

	view_redraw();
}

/* Gets full path to the file that will be explored (the current file of the
 * view).  Returns non-zero if file cannot be explored. */
static int
get_file_to_explore(const FileView *view, char buf[], size_t buf_len)
{
	const dir_entry_t *entry = &view->dir_entry[view->list_pos];

	snprintf(buf, buf_len, "%s/%s", view->curr_dir, entry->name);
	switch(entry->type)
	{
		case CHARACTER_DEVICE:
		case BLOCK_DEVICE:
		case FIFO:
#ifndef _WIN32
		case SOCKET:
#endif
			return 1;
		case LINK:
			if(get_link_target_abs(buf, view->curr_dir, buf, buf_len) != 0)
			{
				return 1;
			}
			return (access(buf, R_OK) != 0);

		default:
			return 0;
	}
}

void
activate_view_mode(void)
{
	*mode = VIEW_MODE;
	pick_vi(curr_view->explore_mode);
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
	char buf[POS_WIN_WIDTH + 1];

	update_screen(curr_stats.need_update);

	werase(pos_win);
	snprintf(buf, sizeof(buf), "%d-%d ", vi->line + 1, vi->nlines);
	mvwaddstr(pos_win, 0, POS_WIN_WIDTH - strlen(buf), buf);
	wnoutrefresh(pos_win);
}

void
view_redraw(void)
{
	view_info_t *saved_vi = vi;

	colmgr_reset();

	if(lwin.explore_mode)
	{
		vi = &view_info[VI_LWIN];
		redraw();
	}
	if(rwin.explore_mode)
	{
		vi = &view_info[VI_RWIN];
		redraw();
	}
	if(!lwin.explore_mode && !rwin.explore_mode)
	{
		redraw();
	}

	vi = saved_vi;
}

void
leave_view_mode(void)
{
	*mode = NORMAL_MODE;

	if(curr_view->explore_mode)
	{
		curr_view->explore_mode = 0;
		redraw_current_view();
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

	if(curr_view->explore_mode || other_view->explore_mode)
	{
		view_redraw();
	}
}

static void
init_view_info(view_info_t *vi)
{
	vi->lines = NULL;
	vi->widths = NULL;
	vi->nlines = 0;
	vi->nlinesv = 0;
	vi->line = 0;
	vi->linev = 0;
	vi->win = -1;
	vi->half_win = -1;
	vi->width = -1;
	vi->view = NULL;
	vi->last_search_backward = -1;
	vi->search_repeat = 0;
	vi->wrap = 0;
}

/* Updates line width and redraws the view. */
static void
redraw(void)
{
	update_view_title(vi->view);
	calc_vlines();
	draw();
}

/* Recalculates virtual lines of a view if display options require it. */
static void
calc_vlines(void)
{
	if(vi->view->window_width - 1 == vi->width && vi->wrap == cfg.wrap_quick_view)
	{
		return;
	}

	vi->width = vi->view->window_width - 1;
	vi->wrap = cfg.wrap_quick_view;

	if(vi->wrap)
	{
		calc_vlines_wrapped(vi);
	}
	else
	{
		calc_vlines_non_wrapped(vi);
	}
}

/* Recalculates virtual lines of a view with line wrapping. */
static void
calc_vlines_wrapped(view_info_t *vi)
{
	int i;
	vi->nlinesv = 0;
	for(i = 0; i < vi->nlines; i++)
	{
		vi->widths[i][0] = vi->nlinesv++;
		vi->widths[i][1] = get_screen_string_length(vi->lines[i]) -
			esc_str_overhead(vi->lines[i]);
		vi->nlinesv += vi->widths[i][1]/vi->width;
	}
}

/* Recalculates virtual lines of a view without line wrapping. */
static void
calc_vlines_non_wrapped(view_info_t *vi)
{
	int i;
	vi->nlinesv = vi->nlines;
	for(i = 0; i < vi->nlines; i++)
	{
		vi->widths[i][0] = i;
		vi->widths[i][1] = vi->width;
	}
}

static void
draw(void)
{
	int l, vl;
	const int height = vi->view->window_rows - 1;
	const int width = vi->view->window_width - 1;
	const int max_l = MIN(vi->line + height, vi->nlines);
	const int searched = (vi->last_search_backward != -1);
	esc_state state;
	esc_state_init(&state, &vi->view->cs.color[WIN_COLOR]);
	werase(vi->view->win);
	for(vl = 0, l = vi->line; l < max_l && vl < height; l++)
	{
		int offset = 0;
		int t = 0;
		char *const line = vi->lines[l];
		char *p = searched ? esc_highlight_pattern(line, &vi->re) : line;
		do
		{
			int printed;
			int vis = l != vi->line || vl + t >= vi->linev - vi->widths[vi->line][0];
			offset += esc_print_line(p + offset, vi->view->win, COL, 1 + vl, width,
					!vis, &state, &printed);
			vl += vis;
			t++;
		}
		while(vi->wrap && p[offset] != '\0' && vl < height);
		if(searched)
		{
			free(p);
		}
	}
	refresh_view_win(vi->view);
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
		status_bar_errorf("Invalid pattern: %s", get_regexp_error(err, &vi->re));
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
cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &lwin)
	{
		view_switch_views();
	}
	normal_cmd_ctrl_wH(key_info, keys_info);
}

static void
cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &rwin)
	{
		view_switch_views();
	}
	normal_cmd_ctrl_wJ(key_info, keys_info);
}

static void
cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &lwin)
	{
		view_switch_views();
	}
	normal_cmd_ctrl_wK(key_info, keys_info);
}

static void
cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &rwin)
	{
		view_switch_views();
	}
	normal_cmd_ctrl_wL(key_info, keys_info);
}

void
view_switch_views(void)
{
	view_info_t saved_vi = view_info[VI_LWIN];
	view_info[VI_LWIN] = view_info[VI_RWIN];
	view_info[VI_RWIN] = saved_vi;

	if(vi == &view_info[VI_LWIN])
	{
		vi = &view_info[VI_RWIN];
	}
	else if(vi == &view_info[VI_RWIN])
	{
		vi = &view_info[VI_LWIN];
	}
	else
	{
		view_info[VI_QV].view = curr_view;
	}

	view_info[VI_LWIN].view = &lwin;
	view_info[VI_RWIN].view = &rwin;
}

static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(HSPLIT);
	view_redraw();
}

static void
cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(VSPLIT);
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
	pick_vi(curr_view->explore_mode);

	update_view_title(&lwin);
	update_view_title(&rwin);
}

/* Updates value of the vi variable and points it to the correct element of the
 * view_info array according to view mode. */
static void
pick_vi(int explore)
{
	const int index = !explore ? VI_QV : (curr_view == &lwin ? VI_LWIN : VI_RWIN);
	vi = &view_info[index];
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
	key_info.count = MAX(1, key_info.count);

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
		offset = get_part(vi->lines[l], offset, vi->view->window_width - 1, buf);

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
				offset = get_part(vi->lines[l], offset, vi->view->window_width - 1,
						buf);
		}
		else
			offset = get_part(vi->lines[l], offset, vi->view->window_width - 1, buf);
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
		offset = get_part(vi->lines[l], offset, vi->view->window_width - 1, buf);

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
		offset = get_part(vi->lines[l], offset, vi->view->window_width - 1, buf);
		vl++;
	}
	draw();
	if(vi->line != l)
	{
		status_bar_error("Pattern not found");
		curr_stats.save_msg = 1;
	}
}

/* Extracts part of the line replacing all occurrences of horizontal tabulation
 * character with appropriate number of spaces.  The offset specifies beginning
 * of the part in the line.  The max_len parameter designates the maximum number
 * of screen characters to put into the part.  Returns newly allocated
 * string. */
static int
get_part(const char line[], int offset, size_t max_len, char part[])
{
	char *no_esc = esc_remove(line);
	const char *const begin = no_esc + offset;
	const char *const end = expand_tabulation(begin, max_len, cfg.tab_stop, part);
	free(no_esc);
	return end - no_esc;
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
	schedule_redraw();
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
