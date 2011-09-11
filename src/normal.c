/* vifm
 * Copyright (C) 2001 Ken Steen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * (at your option) any later version.
 * the Free Software Foundation; either version 2 of the License, or
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

#include <curses.h>

#include <assert.h>
#include <string.h>
#include <wchar.h> /* swprintf */

#include "../config.h"

#include "background.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "macros.h"
#include "menus.h"
#include "modes.h"
#include "permissions_dialog.h"
#include "registers.h"
#include "search.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"
#include "visual.h"

#include "normal.h"

static int *mode;
static int last_fast_search_char;
static int last_fast_search_backward = -1;

static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_d(struct key_info, struct keys_info *);
static void cmd_ctrl_e(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_ctrl_g(struct key_info, struct keys_info *);
static void cmd_space(struct key_info, struct keys_info *);
static void cmd_emarkemark(struct key_info, struct keys_info *);
static void cmd_emark_selector(struct key_info, struct keys_info *);
static void cmd_ctrl_i(struct key_info, struct keys_info *);
static void cmd_ctrl_l(struct key_info, struct keys_info *);
static void cmd_ctrl_o(struct key_info, struct keys_info *);
static void cmd_ctrl_r(struct key_info, struct keys_info *);
static void cmd_ctrl_u(struct key_info, struct keys_info *);
static void cmd_ctrl_wl(struct key_info, struct keys_info *);
static void cmd_ctrl_wh(struct key_info, struct keys_info *);
static void cmd_ctrl_wo(struct key_info, struct keys_info *);
static void cmd_ctrl_wv(struct key_info, struct keys_info *);
static void cmd_ctrl_ww(struct key_info, struct keys_info *);
static void cmd_ctrl_wx(struct key_info, struct keys_info *);
static void cmd_ctrl_y(struct key_info, struct keys_info *);
static void cmd_quote(struct key_info, struct keys_info *);
static void cmd_percent(struct key_info, struct keys_info *);
static void cmd_comma(struct key_info, struct keys_info *);
static void cmd_dot(struct key_info, struct keys_info *);
static void cmd_colon(struct key_info, struct keys_info *);
static void cmd_semicolon(struct key_info, struct keys_info *);
static void cmd_slash(struct key_info, struct keys_info *);
static void cmd_question(struct key_info, struct keys_info *);
static void cmd_C(struct key_info, struct keys_info *);
static void cmd_F(struct key_info, struct keys_info *);
static void find_goto(int ch, int count, int backward,
		struct keys_info * keys_info);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_H(struct key_info, struct keys_info *);
static void cmd_L(struct key_info, struct keys_info *);
static void cmd_M(struct key_info, struct keys_info *);
static void cmd_N(struct key_info, struct keys_info *);
static void cmd_P(struct key_info, struct keys_info *);
static void cmd_V(struct key_info, struct keys_info *);
static void cmd_ZQ(struct key_info, struct keys_info *);
static void cmd_ZZ(struct key_info, struct keys_info *);
static void cmd_al(struct key_info, struct keys_info *);
static void cmd_cW(struct key_info, struct keys_info *);
#ifndef _WIN32
static void cmd_cg(struct key_info, struct keys_info *);
#endif
static void cmd_cl(struct key_info, struct keys_info *);
#ifndef _WIN32
static void cmd_co(struct key_info, struct keys_info *);
static void cmd_cp(struct key_info, struct keys_info *);
#endif
static void cmd_cw(struct key_info, struct keys_info *);
static void cmd_DD(struct key_info, struct keys_info *);
static void cmd_dd(struct key_info, struct keys_info *);
static void delete(struct key_info, int use_trash);
static void cmd_D_selector(struct key_info, struct keys_info *);
static void cmd_d_selector(struct key_info, struct keys_info *);
static void delete_with_selector(struct key_info, struct keys_info *,
		int use_trash);
static void cmd_f(struct key_info, struct keys_info *);
static void cmd_gA(struct key_info, struct keys_info *);
static void cmd_ga(struct key_info, struct keys_info *);
static void cmd_gf(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void cmd_gs(struct key_info, struct keys_info *);
static void cmd_gU(struct key_info, struct keys_info *);
static void cmd_gUgg(struct key_info, struct keys_info *);
static void cmd_gu(struct key_info, struct keys_info *);
static void cmd_gugg(struct key_info, struct keys_info *);
static void cmd_gv(struct key_info, struct keys_info *);
static void cmd_h(struct key_info, struct keys_info *);
static void cmd_i(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_n(struct key_info, struct keys_info *);
static void cmd_l(struct key_info, struct keys_info *);
static void cmd_p(struct key_info, struct keys_info *);
static void cmd_m(struct key_info, struct keys_info *);
static void cmd_rl(struct key_info, struct keys_info *);
static void cmd_t(struct key_info, struct keys_info *);
static void cmd_u(struct key_info, struct keys_info *);
static void cmd_yy(struct key_info, struct keys_info *);
static void cmd_y_selector(struct key_info, struct keys_info *);
static void cmd_zM(struct key_info, struct keys_info *);
static void cmd_zO(struct key_info, struct keys_info *);
static void cmd_zR(struct key_info, struct keys_info *);
static void cmd_za(struct key_info, struct keys_info *);
static void cmd_zf(struct key_info, struct keys_info *);
static void cmd_zm(struct key_info, struct keys_info *);
static void cmd_zo(struct key_info, struct keys_info *);
static void pick_files(FileView *view, int end, struct keys_info *keys_info);
static void selector_S(struct key_info, struct keys_info *);
static void selector_a(struct key_info, struct keys_info *);
static void selector_s(struct key_info, struct keys_info *);

static struct keys_add_info builtin_cmds[] = {
	{L"\x02", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x07", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_g}}},
	{L"\x09", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0c", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x0d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{L"\x0f", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_o}}},
	{L"\x12", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_r}}},
	{L"\x15", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17\x08", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17h", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17\x0f", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17o", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17\x0c", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17\x13", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17s", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17\x16", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17v", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17\x17", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17w", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17\x18", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
	{L"\x17x", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
	{L"\x19", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
	/* escape */
	{L"\x1b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"'", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L" ", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"!!", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_emarkemark}}},
	{L"!", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_emark_selector}}},
	{L"%", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L".", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dot}}},
	{L":", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L";", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"/", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L"?", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_question}}},
	{L"C", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_C}}},
	{L"F", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"N", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"P", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_P}}},
	{L"V", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_V}}},
	{L"Y", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_yy}}},
	{L"ZQ", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ZQ}}},
	{L"ZZ", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ZZ}}},
	{L"al", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_al}}},
	{L"cW", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cW}}},
#ifndef _WIN32
	{L"cg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cg}}},
#endif
	{L"cl", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cl}}},
#ifndef _WIN32
	{L"co", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_co}}},
	{L"cp", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cp}}},
#endif
	{L"cw", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cw}}},
	{L"DD", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_DD}}},
	{L"dd", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dd}}},
	{L"D", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_D_selector}}},
	{L"d", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_d_selector}}},
	{L"f", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gA", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gA}}},
	{L"ga", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ga}}},
	{L"gf", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gf}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"gs", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gs}}},
	{L"gU", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_gU}}},
	{L"gUgU", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"gUgg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gUgg}}},
	{L"gUU", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"gu", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_gu}}},
	{L"gugg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gugg}}},
	{L"gugu", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"guu", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"gv", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gv}}},
	{L"h", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L"i", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_i}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{L"m", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_m}}},
	{L"n", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"p", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_p}}},
	{L"rl", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_rl}}},
	{L"t", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_t}}},
	{L"u", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"yy", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_yy}}},
	{L"y", {BUILDIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_y_selector}}},
	{L"v", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_V}}},
	{L"zM", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zM}}},
	{L"zO", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zO}}},
	{L"zR", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zR}}},
	{L"za", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_za}}},
	{L"zb", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zb}}},
	{L"zf", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zf}}},
	{L"zm", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zm}}},
	{L"zo", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zo}}},
	{L"zt", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zt}}},
	{L"zz", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zz}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_LEFT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_RIGHT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{{KEY_HOME}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

static struct keys_add_info selectors[] = {
	{L"'", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L"%", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L";", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"F", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"S", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_S}}},
	{L"a", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_a}}},
	{L"f", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"s", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_s}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_HOME}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_normal_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), NORMAL_MODE);
	assert(ret_code == 0);
	ret_code = add_selectors(selectors, ARRAY_LEN(selectors), NORMAL_MODE);
	assert(ret_code == 0);
}

static void
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	int s;
	int l = curr_view->window_rows - 1;

	if(curr_view->top_line == 0)
		return;

	curr_view->list_pos = curr_view->top_line + 1;
	curr_view->top_line -= l;
	if(curr_view->top_line < 0)
		curr_view->top_line = 0;
	s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 &&
			curr_view->top_line + curr_view->window_rows - curr_view->list_pos < s)
		curr_view->list_pos -= s - (curr_view->top_line + curr_view->window_rows -
				curr_view->list_pos);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
	curs_set(FALSE);
}

static void
cmd_ctrl_d(struct key_info key_info, struct keys_info *keys_info)
{
	int s;
	curr_view->top_line += (curr_view->window_rows + 1)/2;
	curr_view->list_pos += (curr_view->window_rows + 1)/2;
	s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 && curr_view->list_pos - curr_view->top_line < s)
		curr_view->list_pos += s - (curr_view->list_pos - curr_view->top_line);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Scroll pane one line down. */
static void
cmd_ctrl_e(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;
	if(curr_view->top_line == curr_view->list_rows - curr_view->window_rows - 1)
		return;
	if(curr_view->list_pos == curr_view->top_line)
		curr_view->list_pos++;
	curr_view->top_line++;
	scroll_view(curr_view);
}

static void
cmd_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	int s;
	int l = curr_view->window_rows - 1;

	if(curr_view->top_line + 1 == curr_view->list_rows - (l + 1))
		return;

	curr_view->list_pos = curr_view->top_line + l;
	curr_view->top_line += l;
	if(curr_view->top_line > curr_view->list_rows)
		curr_view->top_line = curr_view->list_rows - l;
	s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 && curr_view->list_pos - curr_view->top_line < s)
		curr_view->list_pos += s - (curr_view->list_pos - curr_view->top_line);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
cmd_ctrl_g(struct key_info key_info, struct keys_info *keys_info)
{
	if(!curr_stats.show_full)
		curr_stats.show_full = 1;
}

static void
cmd_space(struct key_info key_info, struct keys_info *keys_info)
{
	change_window();
}

static void
cmd_emarkemark(struct key_info key_info, struct keys_info *keys_info)
{
	wchar_t buf[16] = L".!";
	if(key_info.count != NO_COUNT_GIVEN)
#ifndef _WIN32
		swprintf(buf, ARRAY_LEN(buf), L".,.+%d!", key_info.count - 1);
#else
		swprintf(buf, L".,.+%d!", key_info.count - 1);
#endif
	enter_cmdline_mode(CMD_SUBMODE, buf, NULL);
}

static void
cmd_emark_selector(struct key_info key_info, struct keys_info *keys_info)
{
	int i, m;
	wchar_t buf[16] = L".!";

	if(keys_info->count == 0)
	{
		cmd_emarkemark(key_info, keys_info);
		return;
	}

	m = keys_info->indexes[0];
	for(i = 1; i < keys_info->count; i++)
	{
		if(keys_info->indexes[i] > m)
			m = keys_info->indexes[i];
	}

	free(keys_info->indexes);
	keys_info->indexes = NULL;
	keys_info->count = 0;

	if(key_info.count != NO_COUNT_GIVEN)
		m += curr_view->list_pos + key_info.count + 1;

	if(m >= curr_view->list_rows - 1)
	{
		wcscpy(buf, L".,$!");
		enter_cmdline_mode(CMD_SUBMODE, buf, NULL);
	}
	else
	{
		key_info.count = m - curr_view->list_pos + 1;
		cmd_emarkemark(key_info, keys_info);
	}
}

static void
cmd_ctrl_i(struct key_info key_info, struct keys_info *keys_info)
{
#ifdef ENABLE_COMPATIBILITY_MODE
	change_window();
#else /* ENABLE_COMPATIBILITY_MODE */
	if(curr_view->history_pos >= curr_view->history_num - 1)
		return;

	goto_history_pos(curr_view, curr_view->history_pos + 1);
#endif /* ENABLE_COMPATIBILITY_MODE */
}

/* Clear screen and redraw. */
static void
cmd_ctrl_l(struct key_info key_info, struct keys_info *keys_info)
{
	redraw_window();
	curs_set(FALSE);
}

static void
cmd_ctrl_o(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->history_pos == 0)
		return;

	goto_history_pos(curr_view, curr_view->history_pos - 1);
}

static void
cmd_ctrl_r(struct key_info key_info, struct keys_info *keys_info)
{
	int ret;

	curr_stats.confirmed = 0;

	ret = redo_group();
	if(ret == 0)
	{
		if(curr_stats.view)
		{
			load_saving_pos(curr_view, 1);
		}
		else
		{
			load_saving_pos(&lwin, 1);
			load_saving_pos(&rwin, 1);
		}
		status_bar_message("Redone one group");
	}
	else if(ret == -2)
	{
		if(curr_stats.view)
		{
			load_saving_pos(curr_view, 1);
		}
		else
		{
			load_saving_pos(&lwin, 1);
			load_saving_pos(&rwin, 1);
		}
		status_bar_error("Redone one group with errors");
	}
	else if(ret == -1)
	{
		status_bar_message("Nothing to redo");
	}
	else if(ret == -3)
	{
		status_bar_error("Can't redo group, it was skipped");
	}
	else if(ret == -4)
	{
		status_bar_error("Can't redo what wasn't undone");
	}
	else if(ret == -6)
	{
		status_bar_message("Group redo skipped by user");
	}
	else if(ret == 1)
	{
		status_bar_error("Redo operation was skipped due to previous errors");
	}
	curr_stats.save_msg = 1;
}

static void
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	int s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 &&
			curr_view->top_line + curr_view->window_rows - curr_view->list_pos < s)
		curr_view->list_pos -= s - (curr_view->top_line + curr_view->window_rows -
				curr_view->list_pos);
	curr_view->top_line -= (curr_view->window_rows + 1)/2;
	curr_view->list_pos -= (curr_view->window_rows + 1)/2;
	draw_dir_list(curr_view, MAX(curr_view->top_line, 0));
	move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
cmd_ctrl_wl(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->win == lwin.win)
		change_window();
}

static void
cmd_ctrl_wh(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->win == rwin.win)
		change_window();
}

/* Leave only one pane. */
static void
cmd_ctrl_wo(struct key_info key_info, struct keys_info *keys_info)
{
	comm_only();
}

/* To split pane. */
static void
cmd_ctrl_wv(struct key_info key_info, struct keys_info *keys_info)
{
	comm_split();
}

static void
cmd_ctrl_ww(struct key_info key_info, struct keys_info *keys_info)
{
	change_window();
}

/* Switch views. */
static void
cmd_ctrl_wx(struct key_info key_info, struct keys_info *keys_info)
{
	FileView tmp_view;
	WINDOW* tmp;

	tmp = lwin.win;
	lwin.win = rwin.win;
	rwin.win = tmp;

	tmp = lwin.title;
	lwin.title = rwin.title;
	rwin.title = tmp;

	tmp_view = lwin;
	lwin = rwin;
	rwin = tmp_view;

	load_dir_list(curr_view, 1);
	move_to_list_pos(curr_view, curr_view->list_pos);

	if(curr_stats.view)
		quick_view_file(curr_view);
	else
		load_dir_list(other_view, 1);
	wrefresh(other_view->win);
}

static void
cmd_ctrl_y(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1
			|| curr_view->top_line == 0)
		return;
	if(curr_view->list_pos == curr_view->top_line + curr_view->window_rows)
		curr_view->list_pos--;
	curr_view->top_line--;
	scroll_view(curr_view);
}

/* Clone file. */
static void
cmd_C(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.save_msg = clone_files(curr_view, NULL, 0, 0);
}

static void
cmd_F(struct key_info key_info, struct keys_info *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(key_info.multi, key_info.count, 1, keys_info);
}

/* Returns negative number if nothing was found */
int
ffind(int ch, int backward, int wrap)
{
	int x;
	wchar_t ch_buf[] = {ch, L'\0'};

	x = curr_view->list_pos;
	do
	{
		if(backward)
		{
			x--;
			if(x < 0)
			{
				if(wrap)
					x = curr_view->list_rows - 1;
				else
					return -1;
			}
		}
		else
		{
			x++;
			if(x > curr_view->list_rows - 1)
			{
				if(wrap)
					x = 0;
				else
					return -1;
			}
		}

		if(ch > 255)
		{
			char tbuf[9];
			wchar_t wbuf[8];

			strncpy(tbuf, curr_view->dir_entry[x].name, ARRAY_LEN(tbuf) - 1);
			tbuf[ARRAY_LEN(tbuf) - 1] = '\0';
			mbstowcs(wbuf, tbuf, ARRAY_LEN(wbuf));

			if(wcsncmp(wbuf, ch_buf, 1) == 0)
				break;
		}
		else if(curr_view->dir_entry[x].name[0] == ch)
		{
			break;
		}
	}while(x != curr_view->list_pos);

	return x;
}

static void
find_goto(int ch, int count, int backward, struct keys_info *keys_info)
{
	int pos;
	int old_pos = curr_view->list_pos;

	pos = ffind(ch, backward, !keys_info->selector);
	if(pos < 0 || pos == curr_view->list_pos)
		return;
	while(--count > 0)
	{
		curr_view->list_pos = pos;
		pos = ffind(ch, backward, !keys_info->selector);
	}

	if(keys_info->selector)
	{
		curr_view->list_pos = old_pos;
		if(pos >= 0)
			pick_files(curr_view, pos, keys_info);
	}
	else
	{
		move_to_list_pos(curr_view, pos);
	}
}

/* Jump to bottom of the list or to specified line. */
static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;

	if(keys_info->selector)
		pick_files(curr_view, key_info.count - 1, keys_info);
	else
		move_to_list_pos(curr_view, key_info.count - 1);
}

static void
cmd_gA(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].type != DIRECTORY)
		return;

	status_bar_message("Calculating directory size...");
	wrefresh(status_bar);

	calc_dirsize(curr_view->dir_entry[curr_view->list_pos].name, 1);

	redraw_lists();
}

static void
cmd_ga(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].type != DIRECTORY)
		return;

	status_bar_message("Calculating directory size...");
	wrefresh(status_bar);

	calc_dirsize(curr_view->dir_entry[curr_view->list_pos].name, 0);

	redraw_lists();
}

static void
cmd_gf(struct key_info key_info, struct keys_info *keys_info)
{
	clean_selected_files(curr_view);
	handle_file(curr_view, 0, 1);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Jump to top of the list or to specified line. */
static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(keys_info->selector)
		pick_files(curr_view, key_info.count - 1, keys_info);
	else
		move_to_list_pos(curr_view, key_info.count - 1);
}

static void
cmd_gs(struct key_info key_info, struct keys_info *keys_info)
{
	int x;
	curr_view->selected_files = 0;
	for(x = 0; x < curr_view->list_rows; x++)
		curr_view->dir_entry[x].selected = 0;
	for(x = 0; x < curr_view->nsaved_selection; x++)
	{
		if(curr_view->saved_selection[x] != NULL)
		{
			int pos = find_file_pos_in_list(curr_view, curr_view->saved_selection[x]);
			if(pos >= 0 && pos < curr_view->list_rows)
			{
				curr_view->dir_entry[pos].selected = 1;
				curr_view->selected_files++;
			}
		}
	}
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
cmd_gU(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->count == 0)
	{
		curr_stats.save_msg = change_case(curr_view, 1, 1, &curr_view->list_pos);
	}
	else
	{
		curr_stats.save_msg = change_case(curr_view, 1, keys_info->count,
				keys_info->indexes);

		free(keys_info->indexes);
		keys_info->indexes = NULL;
		keys_info->count = 0;
	}
}

static void
cmd_gUgg(struct key_info key_info, struct keys_info *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	cmd_gU(key_info, keys_info);
}

static void
cmd_gu(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->count == 0)
	{
		curr_stats.save_msg = change_case(curr_view, 0, 1, &curr_view->list_pos);
	}
	else
	{
		curr_stats.save_msg = change_case(curr_view, 0, keys_info->count,
				keys_info->indexes);

		free(keys_info->indexes);
		keys_info->indexes = NULL;
		keys_info->count = 0;
	}
}

static void
cmd_gugg(struct key_info key_info, struct keys_info *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	cmd_gu(key_info, keys_info);
}

static void
cmd_gv(struct key_info key_info, struct keys_info *keys_info)
{
	enter_visual_mode(1);
}

/* Go to first file in window. */
static void
cmd_H(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->selector)
		pick_files(curr_view, curr_view->top_line, keys_info);
	else
		move_to_list_pos(curr_view, curr_view->top_line);
}

/* Go to last file in window. */
static void
cmd_L(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->selector)
		pick_files(curr_view, curr_view->top_line + curr_view->window_rows,
				keys_info);
	else
		move_to_list_pos(curr_view, curr_view->top_line + curr_view->window_rows);
}

/* Go to middle of window. */
static void
cmd_M(struct key_info key_info, struct keys_info *keys_info)
{
	int new_pos;
	if(curr_view->list_rows < curr_view->window_rows)
		new_pos = curr_view->list_rows/2;
	else
		new_pos = curr_view->top_line + (curr_view->window_rows/2);

	if(keys_info->selector)
		pick_files(curr_view, new_pos, keys_info);
	else
		move_to_list_pos(curr_view, new_pos);
}

static void
cmd_N(struct key_info key_info, struct keys_info *keys_info)
{
	if(cfg.search_history_num < 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(curr_view->matches == 0)
	{
		const char *pattern = (curr_view->regexp[0] == '\0') ?
				cfg.search_history[0] : curr_view->regexp;
		curr_stats.save_msg = find_pattern(curr_view, pattern,
				!curr_stats.last_search_backward, 1);
		key_info.count--;
	}

	if(curr_view->matches == 0)
		return;

	while(key_info.count-- > 0)
	{
		if(curr_stats.last_search_backward)
			find_next_pattern(curr_view, 1);
		else
			find_previous_pattern(curr_view, 1);
	}

	status_bar_messagef("%c%s", curr_stats.last_search_backward ? '/' : '?',
			curr_view->regexp);
	curr_stats.save_msg = 1;
}

/* Move files. */
static void
cmd_P(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_files_from_register(curr_view, key_info.reg, 1);
}

/* Visual selection of files. */
static void
cmd_V(struct key_info key_info, struct keys_info *keys_info)
{
	enter_visual_mode(0);
}

static void
cmd_ZQ(struct key_info key_info, struct keys_info *keys_info)
{
	comm_quit(0);
}

static void
cmd_ZZ(struct key_info key_info, struct keys_info *keys_info)
{
	comm_quit(1);
}

/* Mark. */
static void
cmd_quote(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->selector)
	{
		int pos;
		pos = check_mark_directory(curr_view, key_info.multi);
		if(pos < 0)
			return;
		pick_files(curr_view, pos, keys_info);
	}
	else
	{
		curr_stats.save_msg = get_bookmark(curr_view, key_info.multi);
	}
}

/* Jump to percent of file. */
static void
cmd_percent(struct key_info key_info, struct keys_info *keys_info)
{
	int line;
	if(key_info.count == NO_COUNT_GIVEN)
		return;
	if(key_info.count > 100)
		return;
	line = (key_info.count * curr_view->list_rows)/100;
	if(keys_info->selector)
		pick_files(curr_view, line - 1, keys_info);
	else
		move_to_list_pos(curr_view, line - 1);
}

static void
cmd_comma(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(last_fast_search_char, key_info.count, !last_fast_search_backward,
			keys_info);
}

/* Repeat last change. */
static void
cmd_dot(struct key_info key_info, struct keys_info *keys_info)
{
	if(0 > cfg.cmd_history_num)
		(void)show_error_msg("Command Error", "Command history list is empty.");
	else
		curr_stats.save_msg = exec_command(cfg.cmd_history[0], curr_view,
				GET_COMMAND);
}

/* Command. */
static void
cmd_colon(struct key_info key_info, struct keys_info *keys_info)
{
	wchar_t buf[16] = L"";
	if(key_info.count != NO_COUNT_GIVEN)
#ifndef _WIN32
		swprintf(buf, ARRAY_LEN(buf), L".,.+%d", key_info.count - 1);
#else
		swprintf(buf, L".,.+%d", key_info.count - 1);
#endif
	enter_cmdline_mode(CMD_SUBMODE, buf, NULL);
}

static void
cmd_semicolon(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(last_fast_search_char, key_info.count, last_fast_search_backward,
			keys_info);
}

/* Search forward. */
static void
cmd_slash(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.last_search_backward = 0;
	enter_cmdline_mode(SEARCH_FORWARD_SUBMODE, L"", NULL);
}

/* Search backward. */
static void
cmd_question(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.last_search_backward = 1;
	enter_cmdline_mode(SEARCH_BACKWARD_SUBMODE, L"", NULL);
}

/* Create link with absolute path */
static void
cmd_al(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_links(curr_view, key_info.reg, 0);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Change word (rename file without extension). */
static void
cmd_cW(struct key_info key_info, struct keys_info *keys_info)
{
	rename_file(curr_view, 1);
}

#ifndef _WIN32
/* Change group. */
static void
cmd_cg(struct key_info key_info, struct keys_info *keys_info)
{
	change_group();
}
#endif

/* Change symbolic link. */
static void
cmd_cl(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.save_msg = change_link(curr_view);
}

#ifndef _WIN32
/* Change owner. */
static void
cmd_co(struct key_info key_info, struct keys_info *keys_info)
{
	change_owner();
}

/* Change permissions. */
static void
cmd_cp(struct key_info key_info, struct keys_info *keys_info)
{
	enter_permissions_mode(curr_view);
}
#endif

/* Change word (rename file). */
static void
cmd_cw(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->selected_files > 1)
		rename_files(curr_view, NULL, 0);
	else
		rename_file(curr_view, 0);
}

/* Delete file. */
static void
cmd_DD(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 0);
}

/* Delete file. */
static void
cmd_dd(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 1);
}

static void
delete(struct key_info key_info, int use_trash)
{
#ifndef ENABLE_COMPATIBILITY_MODE
	int j, k;
	int *i;
#endif

	if(!is_dir_writable(0, curr_view->curr_dir))
		return;

	curr_stats.confirmed = 0;
	if(!use_trash && cfg.confirm)
	{
		if(!query_user_menu("Permanent deletion",
				"Are you sure you want to delete files permanently?"))
			return;
		curr_stats.confirmed = 1;
	}

#ifdef ENABLE_COMPATIBILITY_MODE
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	if(!curr_view->selected_files && key_info.count != NO_COUNT_GIVEN)
	{
		int x;
		int y = curr_view->list_pos;
		for(x = 0; x < key_info.count; x++)
		{
			curr_view->dir_entry[y].selected = 1;
			y++;
		}
	}
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, 0, NULL,
			use_trash);
#else /* ENABLE_COMPATIBILITY_MODE */
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	i = malloc(sizeof(int)*key_info.count);
	k = 0;
	for(j = curr_view->list_pos; j < curr_view->list_pos + key_info.count; j++)
		i[k++] = j;
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, k, i, use_trash);
	free(i);
#endif /* ENABLE_COMPATIBILITY_MODE */
}

static void
cmd_D_selector(struct key_info key_info, struct keys_info *keys_info)
{
	if(!is_dir_writable(0, curr_view->curr_dir))
		return;

	curr_stats.confirmed = 0;
	if(cfg.confirm)
	{
		if(!query_user_menu("Permanent deletion",
				"Are you sure you want to delete files permanently?"))
			return;
		curr_stats.confirmed = 1;
	}

	delete_with_selector(key_info, keys_info, 0);
}

static void
cmd_d_selector(struct key_info key_info, struct keys_info *keys_info)
{
	delete_with_selector(key_info, keys_info, 1);
}

static void
delete_with_selector(struct key_info key_info, struct keys_info *keys_info,
		int use_trash)
{
	if(keys_info->count == 0)
		return;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes, use_trash);

	free(keys_info->indexes);
	keys_info->indexes = NULL;
	keys_info->count = 0;
}

static void
cmd_f(struct key_info key_info, struct keys_info *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(key_info.multi, key_info.count, 0, keys_info);
}

/* Updir. */
static void
cmd_h(struct key_info key_info, struct keys_info *keys_info)
{
	cd_updir(curr_view);
}

static void
cmd_i(struct key_info key_info, struct keys_info *keys_info)
{
	handle_file(curr_view, 1, 0);
}

/* Move down one line. */
static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(keys_info->selector)
	{
		pick_files(curr_view, curr_view->list_pos + key_info.count, keys_info);
	}
	else
	{
		if(curr_view->list_pos == curr_view->list_rows - 1)
			return;
		move_to_list_pos(curr_view, curr_view->list_pos + key_info.count);
	}
}

/* Move up one line. */
static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(keys_info->selector)
	{
		pick_files(curr_view, curr_view->list_pos - key_info.count, keys_info);
	}
	else
	{
		if(curr_view->list_pos == 0)
			return;
		move_to_list_pos(curr_view, curr_view->list_pos - key_info.count);
	}
}

static void
cmd_l(struct key_info key_info, struct keys_info *keys_info)
{
	handle_file(curr_view, 0, 0);
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Set mark. */
static void
cmd_m(struct key_info key_info, struct keys_info *keys_info)
{
	add_bookmark(key_info.multi, curr_view->curr_dir,
			get_current_file_name(curr_view));
}

static void
cmd_n(struct key_info key_info, struct keys_info *keys_info)
{
	if(cfg.search_history_num < 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(curr_view->matches == 0)
	{
		const char *pattern = (curr_view->regexp[0] == '\0') ?
				cfg.search_history[0] : curr_view->regexp;
		curr_stats.save_msg = find_pattern(curr_view, pattern,
				curr_stats.last_search_backward, 1);
		key_info.count--;
	}

	if(curr_view->matches == 0)
		return;

	while(key_info.count-- > 0)
	{
		if(curr_stats.last_search_backward)
			find_previous_pattern(curr_view, 1);
		else
			find_next_pattern(curr_view, 1);
	}

	status_bar_messagef("%c%s", curr_stats.last_search_backward ? '?' : '/',
			curr_view->regexp);
	curr_stats.save_msg = 1;
}

/* Put files. */
static void
cmd_p(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_files_from_register(curr_view, key_info.reg, 0);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Create link with absolute path */
static void
cmd_rl(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_links(curr_view, key_info.reg, 1);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Tag file. */
static void
cmd_t(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].selected == 0)
	{
		/* The ../ dir cannot be selected */
		if(!strcmp(curr_view->dir_entry[curr_view->list_pos].name, "../"))
			return;

		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files++;
	}
	else
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 0;
		curr_view->selected_files--;
	}

	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Undo last command group. */
static void
cmd_u(struct key_info key_info, struct keys_info *keys_info)
{
	int ret;

	curr_stats.confirmed = 0;

	ret = undo_group();
	if(ret == 0)
	{
		if(curr_stats.view)
		{
			load_saving_pos(curr_view, 1);
		}
		else
		{
			load_saving_pos(&lwin, 1);
			load_saving_pos(&rwin, 1);
		}
		status_bar_message("Undone one group");
	}
	else if(ret == -2)
	{
		if(curr_stats.view)
		{
			load_saving_pos(curr_view, 1);
		}
		else
		{
			load_saving_pos(&lwin, 1);
			load_saving_pos(&rwin, 1);
		}
		status_bar_error("Undone one group with errors");
	}
	else if(ret == -1)
	{
		status_bar_message("Nothing to undo");
	}
	else if(ret == -3)
	{
		status_bar_error("Can't undo group, it was skipped");
	}
	else if(ret == -4)
	{
		status_bar_error("Can't undo what wasn't redone");
	}
	else if(ret == -5)
	{
		status_bar_error("Operation cannot be undone");
	}
	else if(ret == -6)
	{
		status_bar_message("Group undo skipped by user");
	}
	else if(ret == 1)
	{
		status_bar_error("Undo operation was skipped due to previous errors");
	}
	curr_stats.save_msg = 1;
}

/* Yank file. */
static void
cmd_yy(struct key_info key_info, struct keys_info *keys_info)
{
#ifndef ENABLE_COMPATIBILITY_MODE
	int j, k;
	int *i;
#endif

	if(key_info.count != NO_COUNT_GIVEN)
		pick_files(curr_view, curr_view->list_pos + key_info.count - 1, keys_info);
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

#ifdef ENABLE_COMPATIBILITY_MODE
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);
#else /* ENABLE_COMPATIBILITY_MODE */
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	i = malloc(sizeof(int)*key_info.count);
	k = 0;
	for(j = curr_view->list_pos; j < curr_view->list_pos + key_info.count; j++)
		i[k++] = j;
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, k, i);
	free(i);
#endif /* ENABLE_COMPATIBILITY_MODE */

	if(key_info.count != NO_COUNT_GIVEN)
		free(keys_info->indexes);
}

static void
cmd_y_selector(struct key_info key_info, struct keys_info *keys_info)
{
	if(keys_info->count == 0)
		return;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);

	free(keys_info->indexes);
	keys_info->indexes = NULL;
	keys_info->count = 0;
}

/* Filter the files matching the filename filter. */
static void
cmd_zM(struct key_info key_info, struct keys_info *keys_info)
{
	restore_filename_filter(curr_view);
	set_dot_files_visible(curr_view, 0);
}

/* Remove filename filter. */
static void
cmd_zO(struct key_info key_info, struct keys_info *keys_info)
{
	remove_filename_filter(curr_view);
}

/* Show all hidden files. */
static void
cmd_zR(struct key_info key_info, struct keys_info *keys_info)
{
	remove_filename_filter(curr_view);
	set_dot_files_visible(curr_view, 1);
}

/* Toggle dot files visibility. */
static void
cmd_za(struct key_info key_info, struct keys_info *keys_info)
{
	toggle_dot_files(curr_view);
}

/* Redraw with file in bottom of list. */
void
normal_cmd_zb(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;

	if(curr_view->list_pos < curr_view->window_rows)
	{
		curr_view->top_line = 0;
	}
	else
	{
		curr_view->top_line = curr_view->list_pos - curr_view->window_rows;
		if(cfg.scroll_off > 0)
		{
			int s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
			curr_view->top_line += s;
		}
	}
	scroll_view(curr_view);
}

/* Filter selected files. */
static void
cmd_zf(struct key_info key_info, struct keys_info *keys_info)
{
	filter_selected_files(curr_view);
}

/* Hide dot files. */
static void
cmd_zm(struct key_info key_info, struct keys_info *keys_info)
{
	set_dot_files_visible(curr_view, 0);
}

/* Show all the dot files. */
static void
cmd_zo(struct key_info key_info, struct keys_info *keys_info)
{
	set_dot_files_visible(curr_view, 1);
}

/* Redraw with file in top of list. */
void
normal_cmd_zt(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;

	if(curr_view->list_rows - curr_view->list_pos >= curr_view->window_rows)
	{
		curr_view->top_line = curr_view->list_pos;
		if(cfg.scroll_off > 0)
		{
			int s = MIN((curr_view->window_rows + 1)/2 - 1, cfg.scroll_off);
			curr_view->top_line -= s;
		}
		if(curr_view->top_line < 0)
			curr_view->top_line = 0;
	}
	else
	{
		curr_view->top_line = curr_view->list_rows - curr_view->window_rows;
	}
	scroll_view(curr_view);
}

/* Redraw with file in center of list. */
void
normal_cmd_zz(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;

	if(curr_view->list_pos < curr_view->window_rows/2)
		curr_view->top_line = 0;
	else if(curr_view->list_pos > curr_view->list_rows - curr_view->window_rows/2)
		curr_view->top_line = curr_view->list_rows - curr_view->window_rows;
	else
		curr_view->top_line = curr_view->list_pos - curr_view->window_rows/2;
	scroll_view(curr_view);
}

static void
pick_files(FileView *view, int end, struct keys_info *keys_info)
{
	int delta, i, x;

	end = MAX(0, end);
	end = MIN(view->list_rows - 1, end);

	keys_info->count = abs(view->list_pos - end) + 1;
	keys_info->indexes = calloc(keys_info->count, sizeof(int));
	if(keys_info->indexes == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	delta = (view->list_pos > end) ? -1 : +1;
	i = 0;
	x = view->list_pos - delta;
	do
	{
		x += delta;
		keys_info->indexes[i++] = x;
	} while(x != end);

	if(end < view->list_pos)
	{
		memset(&view->dir_mtime, 0, sizeof(view->dir_mtime));
		view->list_pos = end;
	}
}

static void
selector_S(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = curr_view->list_rows - curr_view->selected_files;
	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(strcmp(curr_view->dir_entry[x].name, "../") == 0)
			continue;
		if(curr_view->dir_entry[x].selected)
			continue;
		keys_info->indexes[i++] = x;
	}
	keys_info->count = i;
}

static void
selector_a(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = curr_view->list_rows;
	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(strcmp(curr_view->dir_entry[x].name, "../") == 0)
			continue;
		keys_info->indexes[i++] = x;
	}
	keys_info->count = i;
}

static void
selector_s(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = 0;
	for(x = 0; x < curr_view->list_rows; x++)
		if(curr_view->dir_entry[x].selected)
			keys_info->count++;

	if(keys_info->count == 0)
		return;

	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(curr_view->dir_entry[x].selected)
			keys_info->indexes[i++] = x;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
