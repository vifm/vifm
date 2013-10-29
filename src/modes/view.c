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

#include "view.h"

#include <regex.h>

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* ptrdiff_t size_t */
#include <string.h> /* strcpy() strdup() strlen() */
#include <stdlib.h> /* malloc() free() */

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
	int win_size; /* Scroll window size. */
	int half_win;
	int width;
	FileView *view;
	regex_t re;
	int last_search_backward; /* Value -1 means no search was performed. */
	int search_repeat; /* Saved count prefix of search commands. */
	int wrap;
	int abandoned; /* Shows whether view mode was abandoned. */
	char *filename;
}view_info_t;

/* View information structure indexes and count. */
enum
{
	VI_QV,    /* Index of view information structure for quickview window. */
	VI_LWIN,  /* Index of view information structure for left window. */
	VI_RWIN,  /* Index of view information structure for right window. */
	VI_COUNT, /* Number of view information structures. */
};

static int try_ressurect_abandoned(const char full_path[], int explore);
static void reset_view_info(view_info_t *vi);
static void init_view_info(view_info_t *vi);
static void free_view_info(view_info_t *vi);
static void redraw(void);
static void calc_vlines(void);
static void calc_vlines_wrapped(view_info_t *vi);
static void calc_vlines_non_wrapped(view_info_t *vi);
static void draw(void);
static int get_part(const char line[], int offset, size_t max_len, char part[]);
static void display_error(const char error_msg[]);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info);
static FileView * get_active_view(void);
static void cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info);
static int is_right_or_bottom(void);
static int is_top_or_left(void);
static void cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_tab(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void pick_vi(int explore);
static void cmd_qmark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_R(key_info_t key_info, keys_info_t *keys_info);
static int load_view_data(view_info_t *vi, const char action[],
		const char file_to_view[]);
static int get_view_data(view_info_t *vi, const char file_to_view[]);
static void replace_vi(view_info_t *const orig, view_info_t *const new);
static void cmd_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void check_and_set_from_default_win(key_info_t *const key_info);
static void set_from_default_win(key_info_t *const key_info);
static void cmd_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void goto_search_result(int repeat_count, int inverse_direction);
static void search(int repeat_count, int backward);
static void find_previous(int vline_offset);
static void find_next(void);
static void cmd_q(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void update_with_half_win(key_info_t *const key_info);
static void cmd_v(key_info_t key_info, keys_info_t *keys_info);
static void cmd_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z(key_info_t key_info, keys_info_t *keys_info);
static void update_with_win(key_info_t *const key_info);
static int is_trying_the_same_file(void);
static int get_file_to_explore(const FileView *view, char buf[],
		size_t buf_len);

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
	{L"\x17"L"b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wb}}},
	{L"\x17\x08", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17\x09", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wj}}},
	{L"\x17j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wj}}},
	{L"\x17\x0b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wk}}},
	{L"\x17k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wk}}},
	{L"\x17\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17\x0f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17o", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17p", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17\x14", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wt}}},
	{L"\x17t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wt}}},
	{L"\x17\x17", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17w", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17\x18", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
	{L"\x17x", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
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
	{L"R", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_R}}},
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
	char full_path[PATH_MAX];

	if(get_file_to_explore(curr_view, full_path, sizeof(full_path)) != 0)
	{
		show_error_msg("File exploring", "The file cannot be explored");
		return;
	}

	/* Either make use of abandoned view or prune it. */
	if(try_ressurect_abandoned(full_path, explore) == 0)
	{
		return;
	}

	pick_vi(explore);

	if(load_view_data(vi, "File exploring", full_path) != 0)
	{
		return;
	}

	vi->filename = strdup(full_path);

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

/* Either makes use of abandoned view or prunes it.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
try_ressurect_abandoned(const char full_path[], int explore)
{
	const int same_file = vi->abandoned
	                   && vi->view == (explore ? curr_view : other_view)
	                   && vi->filename != NULL
	                   && stroscmp(vi->filename, full_path) == 0;
	if(!same_file)
	{
		return 1;
	}

	if(explore)
	{
		vi->view->explore_mode = 0;
		reset_view_info(vi);
		return 1;
	}
	else
	{
		*mode = VIEW_MODE;
		return 0;
	}
}

void
try_activate_view_mode(void)
{
	if(curr_view->explore_mode)
	{
		*mode = VIEW_MODE;
		pick_vi(curr_view->explore_mode);
	}
}

void
view_pre(void)
{
	if(curr_stats.save_msg == 0)
	{
		status_bar_message("-- VIEW -- ");
		curr_stats.save_msg = 2;
	}
}

void
view_post(void)
{
	update_screen(curr_stats.need_update);
	view_draw_pos();
}

void
view_draw_pos(void)
{
	char buf[POS_WIN_WIDTH + 1];
	snprintf(buf, sizeof(buf), "%d-%d ", vi->line + 1, vi->nlines);

	ui_pos_window_set(buf);
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

	reset_view_info(vi);

	if(curr_view->explore_mode || other_view->explore_mode)
	{
		view_redraw();
	}
}

void
view_explore_mode_quit(FileView *view)
{
	assert(*mode != VIEW_MODE && "Unexpected mode.");
	if(!view->explore_mode)
	{
		return;
	}

	view->explore_mode = 0;

	reset_view_info(&view_info[(view == &lwin) ? VI_LWIN : VI_RWIN]);

	redraw_view(view);
	update_view_title(view);
}

/* Frees and initializes anew view_into_t structure instance. */
static void
reset_view_info(view_info_t *vi)
{
	free_view_info(vi);
	init_view_info(vi);
}

/* Initializes view_into_t structure instance with safe default values. */
static void
init_view_info(view_info_t *vi)
{
	vi->lines = NULL;
	vi->widths = NULL;
	vi->nlines = 0;
	vi->nlinesv = 0;
	vi->line = 0;
	vi->linev = 0;
	vi->win_size = -1;
	vi->half_win = -1;
	vi->width = -1;
	vi->view = NULL;
	vi->last_search_backward = -1;
	vi->search_repeat = NO_COUNT_GIVEN;
	vi->wrap = 0;
	vi->filename = NULL;
	vi->abandoned = 0;
}

/* Frees all resources allocated by view_into_t structure instance. */
static void
free_view_info(view_info_t *vi)
{
	free_string_array(vi->lines, vi->nlines);
	free(vi->widths);
	if(vi->last_search_backward != -1)
		regfree(&vi->re);
	free(vi->filename);
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

	search(vi->search_repeat, backward);

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
	if(is_right_or_bottom())
	{
		view_switch_views();
	}
	move_window(get_active_view(), 0, 1);
}

static void
cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_top_or_left())
	{
		view_switch_views();
	}
	move_window(get_active_view(), 1, 0);
}

static void
cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_right_or_bottom())
	{
		view_switch_views();
	}
	move_window(get_active_view(), 1, 1);
}

static void
cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_top_or_left())
	{
		view_switch_views();
	}
	move_window(get_active_view(), 0, 0);
}

/* Gets pointer to the currently active view from the view point of the
 * view-mode.  Returns that pointer. */
static FileView *
get_active_view(void)
{
	return curr_stats.view ? other_view : curr_view;
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

/* Go to bottom-right window. */
static void
cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

/* Leave only one (current) pane. */
static void
cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->view->explore_mode)
	{
		only();
	}
	else
	{
		display_error("Can't leave preview window alone on the screen.");
	}
}

static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	split_view(HSPLIT);
	view_redraw();
}

static void
cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

/* Checks whether active window is right of bottom one.  Returns non-zero if it
 * is, otherwise zero is returned. */
static int
is_right_or_bottom(void)
{
	return !is_top_or_left();
}

/* Checks whether active window is top of left one.  Returns non-zero if it is,
 * otherwise zero is returned. */
static int
is_top_or_left(void)
{
	const FileView *const top_or_left = curr_view->explore_mode ? &lwin : &rwin;
	return curr_view == top_or_left;
}

static void
cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info)
{
	split_view(VSPLIT);
	view_redraw();
}

static void
cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info)
{
	vi->abandoned = 1;
	*mode = NORMAL_MODE;
	if(curr_view->explore_mode)
	{
		go_to_other_pane();
	}
}

static void
cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info)
{
	vi->abandoned = 1;
	*mode = NORMAL_MODE;
	switch_panes();
	if(curr_stats.view)
	{
		change_window();
	}
}

static void
cmd_meta_space(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
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
	vi->search_repeat = key_info.count;
	enter_cmdline_mode(VIEW_SEARCH_FORWARD_SUBMODE, L"", NULL);
}

static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
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
	goto_search_result(key_info.count, 1);
}

/* Handles view data reloading key. */
static void
cmd_R(key_info_t key_info, keys_info_t *keys_info)
{
	view_info_t new_vi;

	init_view_info(&new_vi);

	if(load_view_data(&new_vi, "File exploring reload", vi->filename) == 0)
	{
		replace_vi(vi, &new_vi);
		view_redraw();
	}
}

/* Loads list of strings and related data into view_info_t structure from
 * specified file.  The action parameter is a title to be used for error
 * messages.  Returns non-zero on error, otherwise zero is returned. */
static int
load_view_data(view_info_t *vi, const char action[], const char file_to_view[])
{
	switch(get_view_data(vi, file_to_view))
	{
		case 0:
			break;

		case 1:
			show_error_msg(action, "Can't explore a directory");
			return 1;
		case 2:
			show_error_msg(action, "Can't open file for reading");
			return 1;
		case 3:
			show_error_msg(action, "Can't get data from viewer");
			return 1;
		case 4:
			show_error_msg(action, "Nothing to explore");
			return 1;

		default:
			assert(0 && "Unhandled error code.");
			return 1;
	}

	vi->widths = malloc(sizeof(*vi->widths)*vi->nlines);
	if(vi->widths == NULL)
	{
		free_string_array(vi->lines, vi->nlines);
		vi->lines = NULL;
		vi->nlines = 0;
		show_error_msg(action, "Not enough memory");
		return 1;
	}

	return 0;
}

/* Reads data to be displayed handling error cases.  Returns zero on success, 1
 * if file is a directory, 2 on file reading error, 3 on issues with viewer or
 * 4 on empty input. */
static int
get_view_data(view_info_t *vi, const char file_to_view[])
{
	FILE *fp;

	const char *const viewer =
		get_viewer_for_file(get_last_path_component(file_to_view));

	if(is_null_or_empty(viewer))
	{
		if(is_dir(file_to_view))
		{
			return 1;
		}
		else if((fp = fopen(file_to_view, "rb")) == NULL)
		{
			return 2;
		}
	}
	else if((fp = use_info_prog(viewer)) == NULL)
	{
		return 3;
	}

	vi->lines = is_null_or_empty(viewer)
		? read_file_lines(fp, &vi->nlines)
		: read_stream_lines(fp, &vi->nlines);

	fclose(fp);

	if(vi->lines == NULL || vi->nlines == 0)
	{
		return 4;
	}

	return 0;
}

/* Replaces view_info_t structure with another one preserving as much as
 * possible. */
static void
replace_vi(view_info_t *const orig, view_info_t *const new)
{
	new->filename = orig->filename;
	orig->filename = NULL;

	if(orig->last_search_backward != -1)
	{
		new->last_search_backward = orig->last_search_backward;
		new->re = orig->re;
		orig->last_search_backward = -1;
	}

	new->win_size = orig->win_size;
	new->half_win = orig->half_win;
	new->line = orig->line;
	new->linev = orig->linev;
	new->view = orig->view;

	free_view_info(orig);
	*orig = *new;
}

static void
cmd_b(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
	cmd_k(key_info, keys_info);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_half_win(&key_info);
	cmd_j(key_info, keys_info);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
	cmd_j(key_info, keys_info);
}

/* Sets key count from scroll window size when count is not specified. */
static void
check_and_set_from_default_win(key_info_t *const key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		set_from_default_win(key_info);
	}
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
	goto_search_result(key_info.count, 0);
}

/* Goes to the next/previous match of the last search. */
static void
goto_search_result(int repeat_count, int inverse_direction)
{
	const int backward = inverse_direction ?
		!vi->last_search_backward : vi->last_search_backward;
	search(repeat_count, backward);
}

/* Performs search and navigation to the first match. */
static void
search(int repeat_count, int backward)
{
	if(vi->last_search_backward == -1)
	{
		return;
	}

	if(repeat_count == NO_COUNT_GIVEN)
	{
		repeat_count = 1;
	}

	while(repeat_count-- > 0 && curr_stats.save_msg == 0)
	{
		if(backward)
		{
			find_previous(1);
		}
		else
		{
			find_next();
		}
	}
}

static void
find_previous(int vline_offset)
{
	int i;
	int offset = 0;
	char buf[(vi->view->window_width - 1)*4];
	int vl, l;

	vl = vi->linev - vline_offset;
	l = vi->line;

	if(l > 0 && vl < vi->widths[l][0])
		l--;

	for(i = 0; i <= vl - vi->widths[l][0]; i++)
		offset = get_part(vi->lines[l], offset, vi->view->window_width - 1, buf);

	/* Don't stop until we go above first virtual line of the first line. */
	while(l >= 0 && vl >= 0)
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
		display_error("Pattern not found");
	}
}

static void
find_next(void)
{
	int i;
	int offset = 0;
	char buf[(vi->view->window_width - 1)*4];
	int vl, l;

	vl = vi->linev + 1;
	l = vi->line;

	if(l < vi->nlines - 1 && vl == vi->widths[l + 1][0])
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
		display_error("Pattern not found");
	}
}

/* Extracts part of the line replacing all occurrences of horizontal tabulation
 * character with appropriate number of spaces.  The offset specifies beginning
 * of the part in the line.  The max_len parameter designates the maximum number
 * of screen characters to put into the part.  Returns number of processed items
 * of the line. */
static int
get_part(const char line[], int offset, size_t max_len, char part[])
{
	char *const no_esc = esc_remove(line);
	const char *const begin = no_esc + offset;
	const char *const end = expand_tabulation(begin, max_len, cfg.tab_stop, part);
	const ptrdiff_t processed_chars = end - no_esc;
	free(no_esc);
	return processed_chars;
}

/* Displays the error message in the status bar. */
static void
display_error(const char error_msg[])
{
	status_bar_error(error_msg);
	curr_stats.save_msg = 1;
}

static void
cmd_q(key_info_t key_info, keys_info_t *keys_info)
{
	leave_view_mode();
}

static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_half_win(&key_info);
	cmd_k(key_info, keys_info);
}

/* Sets key count from half window size when count is not specified, otherwise
 * specified count is stored as new size of the half window size. */
static void
update_with_half_win(key_info_t *const key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		key_info->count = (vi->half_win > 0)
			? vi->half_win
			: (vi->view->window_rows - 1)/2;
	}
	else
	{
		vi->half_win = key_info->count;
	}
}

static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	(void)view_file(buf, vi->line + (vi->view->window_rows - 1)/2, -1, 1);
	/* In some cases two redraw operations are needed, otherwise TUI is not fully
	 * redrawn. */
	update_screen(UT_REDRAW);
	schedule_redraw();
}

static void
cmd_w(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_win(&key_info);
	cmd_k(key_info, keys_info);
}

static void
cmd_z(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_win(&key_info);
	cmd_j(key_info, keys_info);
}

/* Sets key count from scroll window size when count is not specified, otherwise
 * specified count is stored as new size of the scroll window. */
static void
update_with_win(key_info_t *const key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		set_from_default_win(key_info);
	}
	else
	{
		vi->win_size = key_info->count;
	}
}

/* Sets key count from scroll window size. */
static void
set_from_default_win(key_info_t *const key_info)
{
	key_info->count = (vi->win_size > 0)
		? vi->win_size
		: (vi->view->window_rows - 2);
}

int
draw_abandoned_view_mode(void)
{
	if(!vi->abandoned)
	{
		return 0;
	}

	if(is_trying_the_same_file())
	{
		redraw();
		return 1;
	}

	reset_view_info(vi);
	return 0;
}

static int
is_trying_the_same_file(void)
{
	char full_path[PATH_MAX];

	if(get_file_to_explore(curr_view, full_path, sizeof(full_path)) != 0)
	{
		return 0;
	}

	if(stroscmp(vi->filename, full_path) != 0)
	{
		return 0;
	}

	return 1;
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
