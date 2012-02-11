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

#include <curses.h>

#include <pthread.h>

#include <assert.h>
#include <string.h>
#include <wctype.h> /* wtoupper */
#include <wchar.h> /* swprintf */

#include "../config.h"

#include "attr_dialog.h"
#include "background.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "macros.h"
#include "menus.h"
#include "modes.h"
#include "registers.h"
#include "search.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "undo.h"
#include "utf8.h"
#include "utils.h"
#include "view.h"
#include "visual.h"

#include "normal.h"

typedef struct
{
	char *path;
	int force;
}dir_size_t;

static void cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_emarkemark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_emark_selector(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info);
static void go_to_other_window(void);
static void cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info);
static FileView * get_view(void);
static void move_splitter(key_info_t key_info, int fact);
static void cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void cmd_quote(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_comma(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dot(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_semicolon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_question(key_info_t key_info, keys_info_t *keys_info);
static void cmd_C(key_info_t key_info, keys_info_t *keys_info);
static void cmd_F(key_info_t key_info, keys_info_t *keys_info);
static void find_goto(int ch, int count, int backward, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_H(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_P(key_info_t key_info, keys_info_t *keys_info);
static void cmd_V(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ZQ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ZZ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_al(key_info_t key_info, keys_info_t *keys_info);
static void cmd_cW(key_info_t key_info, keys_info_t *keys_info);
#ifndef _WIN32
static void cmd_cg(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_cl(key_info_t key_info, keys_info_t *keys_info);
#ifndef _WIN32
static void cmd_co(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_cp(key_info_t key_info, keys_info_t *keys_info);
static void cmd_cw(key_info_t key_info, keys_info_t *keys_info);
static void cmd_DD(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dd(key_info_t key_info, keys_info_t *keys_info);
static void delete(key_info_t key_info, int use_trash);
static void cmd_D_selector(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d_selector(key_info_t key_info, keys_info_t *keys_info);
static void delete_with_selector(key_info_t key_info, keys_info_t *keys_info,
		int use_trash);
static void cmd_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gA(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ga(key_info_t key_info, keys_info_t *keys_info);
static void start_dir_size_calc(const char *path, int force);
static void * dir_size_stub(void *arg);
static void cmd_gf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
#ifdef _WIN32
static void cmd_gl(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_gs(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gU(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gUgg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gu(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gugg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(key_info_t key_info, int backward);
static void cmd_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void put_files(key_info_t key_info, int move);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_yy(key_info_t key_info, keys_info_t *keys_info);
static void cmd_y_selector(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zM(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zO(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zR(key_info_t key_info, keys_info_t *keys_info);
static void cmd_za(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zm(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zo(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_paren(key_info_t key_info, keys_info_t *keys_info);
static const char * get_last_ext(const char *name);
static wchar_t get_first_wchar(const char *str);
static void pick_files(FileView *view, int end, keys_info_t *keys_info);
static void selector_S(key_info_t key_info, keys_info_t *keys_info);
static void selector_a(key_info_t key_info, keys_info_t *keys_info);
static void selector_s(key_info_t key_info, keys_info_t *keys_info);

static int *mode;
static int last_fast_search_char;
static int last_fast_search_backward = -1;
static int search_repeat;

static keys_add_info_t builtin_cmds[] = {
	{L"\x01", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_a}}},
	{L"\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x07", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_g}}},
	{L"\x09", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x0f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_o}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x12", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_r}}},
	{L"\x15", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wb}}},
	{L"\x17"L"b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wb}}},
	{L"\x17\x08", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wh}}},
	{L"\x17\x09", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wj}}},
	{L"\x17j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wj}}},
	{L"\x17\x0f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17o", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wo}}},
	{L"\x17\x0b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wk}}},
	{L"\x17k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wk}}},
	{L"\x17\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wl}}},
	{L"\x17\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17p", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17\x13", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ws}}},
	{L"\x17s", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ws}}},
	{L"\x17\x14", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wt}}},
	{L"\x17t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wt}}},
	{L"\x17\x16", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wv}}},
	{L"\x17\x17", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17w", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_ww}}},
	{L"\x17\x18", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
	{L"\x17x", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wx}}},
	{L"\x17=", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wequal}}},
	{L"\x17<", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wless}}},
	{L"\x17>", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wgreater}}},
	{L"\x17+", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wplus}}},
	{L"\x17-", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wminus}}},
	{L"\x17|", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wpipe}}},
	{L"\x17_", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_ctrl_wpipe}}},
	{L"\x18", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_x}}},
	{L"\x19", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_BTAB, 0}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
#else
	{L"\033"L"[Z", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
#endif
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"'", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"!!", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_emarkemark}}},
	{L"!", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_emark_selector}}},
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L".", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dot}}},
	{L":", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L";", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"/", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L"?", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_question}}},
	{L"C", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_C}}},
	{L"F", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"N", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"P", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_P}}},
	{L"V", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_V}}},
	{L"Y", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_yy}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ZQ}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ZZ}}},
	{L"al", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_al}}},
	{L"cW", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cW}}},
#ifndef _WIN32
	{L"cg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cg}}},
#endif
	{L"cl", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cl}}},
#ifndef _WIN32
	{L"co", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_co}}},
#endif
	{L"cp", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cp}}},
	{L"cw", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cw}}},
	{L"DD", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_DD}}},
	{L"dd", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dd}}},
	{L"D", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_D_selector}}},
	{L"d", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_d_selector}}},
	{L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_e}}},
	{L"f", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gA", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gA}}},
	{L"ga", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ga}}},
	{L"gf", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gf}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
#ifdef _WIN32
	{L"gl", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gl}}},
#endif
	{L"gs", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gs}}},
	{L"gU", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_gU}}},
	{L"gUgU", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"gUgg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gUgg}}},
	{L"gUU", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"gu", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_gu}}},
	{L"gugg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gugg}}},
	{L"gugu", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"guu", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"gv", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gv}}},
	{L"h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L"i", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_i}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{L"m", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_m}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"p", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_p}}},
	{L"rl", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_rl}}},
	{L"t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_t}}},
	{L"u", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"yy", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_yy}}},
	{L"y", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_y_selector}}},
	{L"v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_V}}},
	{L"zM", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zM}}},
	{L"zO", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zO}}},
	{L"zR", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zR}}},
	{L"za", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_za}}},
	{L"zb", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zb}}},
	{L"zf", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zf}}},
	{L"zm", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zm}}},
	{L"zo", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zo}}},
	{L"zt", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zt}}},
	{L"zz", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zz}}},
	{L"(", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left_paren}}},
	{L")", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right_paren}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_LEFT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

static keys_add_info_t selectors[] = {
	{L"'", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L";", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"F", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"S", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_S}}},
	{L"a", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_a}}},
	{L"f", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"s", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_s}}},
	{L"(", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left_paren}}},
	{L")", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right_paren}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
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
cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = incdec_names(curr_view, key_info.count);
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
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
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
	curs_set(FALSE);
}

static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
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
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(!correct_list_pos_on_scroll_down(curr_view, +1))
		return;

	curr_view->top_line++;
	scroll_view(curr_view);
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
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
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	enter_file_info_mode(curr_view);
}

static void
cmd_space(key_info_t key_info, keys_info_t *keys_info)
{
	go_to_other_window();
}

static void
cmd_emarkemark(key_info_t key_info, keys_info_t *keys_info)
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
cmd_emark_selector(key_info_t key_info, keys_info_t *keys_info)
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
cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info)
{
#ifdef ENABLE_COMPATIBILITY_MODE
	go_to_other_window();
#else /* ENABLE_COMPATIBILITY_MODE */
	if(curr_view->history_pos >= curr_view->history_num - 1)
		return;

	goto_history_pos(curr_view, curr_view->history_pos + 1);
#endif /* ENABLE_COMPATIBILITY_MODE */
}

/* Clear screen and redraw. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	redraw_window();
	curs_set(FALSE);
}

static void
cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->history_pos == 0)
		return;

	goto_history_pos(curr_view, curr_view->history_pos - 1);
}

static void
cmd_ctrl_r(key_info_t key_info, keys_info_t *keys_info)
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
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
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

/* Go to bottom-right window. */
static void
cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &rwin)
		go_to_other_window();
}

static void
cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && curr_view == &rwin)
		go_to_other_window();
}

static void
cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && curr_view == &lwin)
		go_to_other_window();
}

static void
cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && curr_view == &rwin)
		go_to_other_window();
}

static void
cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && curr_view == &lwin)
		go_to_other_window();
}

/* Leave only one pane. */
static void
cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info)
{
	comm_only();
}

/* To split pane horizontally. */
static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(0);
}

/* Go to top-left window. */
static void
cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view != &lwin)
		go_to_other_window();
}

/* To split pane vertically. */
static void
cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info)
{
	comm_split(1);
}

static void
cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info)
{
	go_to_other_window();
}

static void
go_to_other_window(void)
{
	change_window();
	if(curr_view->explore_mode)
		activate_view_mode();
}

void
normal_cmd_ctrl_wequal(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.splitter_pos = -1;
	redraw_window();
}

void
normal_cmd_ctrl_wless(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(key_info, (get_view() == &lwin) ? -1 : +1);
}

void
normal_cmd_ctrl_wgreater(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(key_info, (get_view() == &lwin) ? +1 : -1);
}

void
normal_cmd_ctrl_wplus(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(key_info, (get_view() == &lwin) ? +1 : -1);
}

void
normal_cmd_ctrl_wminus(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(key_info, (get_view() == &lwin) ? -1 : +1);
}

void
normal_cmd_ctrl_wpipe(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT)
		key_info.count = getmaxy(stdscr);
	else
		key_info.count = getmaxx(stdscr);
	move_splitter(key_info, (get_view() == &lwin) ? +1 : -1);
}

static FileView *
get_view(void)
{
	if(get_mode() == VIEW_MODE)
		return curr_view->explore_mode ? curr_view : other_view;
	else
		return curr_view;
}

static void
move_splitter(key_info_t key_info, int fact)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(curr_stats.splitter_pos < 0)
	{
		if(curr_stats.split == VSPLIT)
			curr_stats.splitter_pos = getmaxx(stdscr)/2 - 1 + getmaxx(stdscr)%2;
		else
			curr_stats.splitter_pos = getmaxy(stdscr)/2 - 1;
	}
	curr_stats.splitter_pos += fact*key_info.count;
	if(curr_stats.splitter_pos < 0)
		curr_stats.splitter_pos = 0;
	redraw_window();
}

/* Switch views. */
static void
cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info)
{
	FileView tmp_view;
	WINDOW* tmp;
	int t;

	tmp = lwin.win;
	lwin.win = rwin.win;
	rwin.win = tmp;

	t = lwin.window_rows;
	lwin.window_rows = rwin.window_rows;
	rwin.window_rows = t;

	t = lwin.window_width;
	lwin.window_width = rwin.window_width;
	rwin.window_width = t;

	tmp = lwin.title;
	lwin.title = rwin.title;
	rwin.title = tmp;

	tmp_view = lwin;
	lwin = rwin;
	rwin = tmp_view;

	load_dir_list(curr_view, 1);
	move_to_list_pos(curr_view, curr_view->list_pos);

	if(curr_stats.number_of_windows == 2)
	{
		if(curr_stats.view)
			quick_view_file(curr_view);
		else
			load_dir_list(other_view, 1);
		wrefresh(other_view->win);
	}
}

static void
cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = incdec_names(curr_view, -key_info.count);
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(!correct_list_pos_on_scroll_up(curr_view, -1))
		return;

	curr_view->top_line--;
	scroll_view(curr_view);
}

static void
cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.view)
		enter_view_mode(0);
	else if(other_view->explore_mode)
		go_to_other_window();
}

/* Clone file. */
static void
cmd_C(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = clone_files(curr_view, NULL, 0, 0, key_info.count);
}

static void
cmd_F(key_info_t key_info, keys_info_t *keys_info)
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
	int upcase = cfg.ignore_case && !(cfg.smart_case && iswupper(ch));

	if(upcase)
		ch = towupper(ch);

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
			wchar_t wc = get_first_wchar(curr_view->dir_entry[x].name);
			if(upcase)
				wc = towupper(wc);
			if(wc == ch)
				break;
		}
		else
		{
			int c = curr_view->dir_entry[x].name[0];
			if(upcase)
				c = towupper(c);
			if(c == ch)
				break;
		}
	}
	while(x != curr_view->list_pos);

	return x;
}

static void
find_goto(int ch, int count, int backward, keys_info_t *keys_info)
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
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;

	if(keys_info->selector)
		pick_files(curr_view, key_info.count - 1, keys_info);
	else
		move_to_list_pos(curr_view, key_info.count - 1);
}

static void
cmd_gA(key_info_t key_info, keys_info_t *keys_info)
{
	char full_path[PATH_MAX];

	if(curr_view->dir_entry[curr_view->list_pos].type != DIRECTORY)
		return;

	snprintf(full_path, sizeof(full_path), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	start_dir_size_calc(full_path, 1);
}

static void
cmd_ga(key_info_t key_info, keys_info_t *keys_info)
{
	char full_path[PATH_MAX];

	if(curr_view->dir_entry[curr_view->list_pos].type != DIRECTORY)
		return;

	snprintf(full_path, sizeof(full_path), "%s/%s", curr_view->curr_dir,
			curr_view->dir_entry[curr_view->list_pos].name);
	start_dir_size_calc(full_path, 0);
}

static void
start_dir_size_calc(const char *path, int force)
{
	pthread_t id;
	dir_size_t *dir_size;

	dir_size = malloc(sizeof(*dir_size));
	dir_size->path = strdup(path);
	dir_size->force = force;

	pthread_create(&id, NULL, dir_size_stub, dir_size);
}

static void *
dir_size_stub(void *arg)
{
	dir_size_t *dir_size = (dir_size_t *)arg;
	calc_dirsize(dir_size->path, dir_size->force);

	chosp(dir_size->path);
	*strrchr(dir_size->path, '/') = '\0';
	if(path_starts_with(lwin.curr_dir, dir_size->path))
		memset(&lwin.dir_mtime, 0, sizeof(lwin.dir_mtime));
	if(path_starts_with(rwin.curr_dir, dir_size->path))
		memset(&rwin.dir_mtime, 0, sizeof(rwin.dir_mtime));

	free(dir_size->path);
	free(dir_size);
	return NULL;
}

static void
cmd_gf(key_info_t key_info, keys_info_t *keys_info)
{
	clean_selected_files(curr_view);
	handle_file(curr_view, 0, 1);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Jump to top of the list or to specified line. */
static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	if(keys_info->selector)
		pick_files(curr_view, key_info.count - 1, keys_info);
	else
		move_to_list_pos(curr_view, key_info.count - 1);
}

#ifdef _WIN32
static void
cmd_gl(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.as_admin = 1;
	handle_file(curr_view, 0, 0);
	curr_stats.as_admin = 0;
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}
#endif

static void
cmd_gs(key_info_t key_info, keys_info_t *keys_info)
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
cmd_gU(key_info_t key_info, keys_info_t *keys_info)
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
cmd_gUgg(key_info_t key_info, keys_info_t *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	cmd_gU(key_info, keys_info);
}

static void
cmd_gu(key_info_t key_info, keys_info_t *keys_info)
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
cmd_gugg(key_info_t key_info, keys_info_t *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	cmd_gu(key_info, keys_info);
}

static void
cmd_gv(key_info_t key_info, keys_info_t *keys_info)
{
	enter_visual_mode(1);
}

/* Go to first file in window. */
static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	int top;
	int off = MAX(cfg.scroll_off, 0);
	if(off > curr_view->window_rows/2)
		return;

	if(curr_view->top_line == 0)
		top = 0;
	else
		top = curr_view->top_line + off;

	if(keys_info->selector)
		pick_files(curr_view, top, keys_info);
	else
		move_to_list_pos(curr_view, top);
}

/* Go to last file in window. */
static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	int top;
	int off = MAX(cfg.scroll_off, 0);
	if(off > curr_view->window_rows/2)
		return;

	if(curr_view->top_line + curr_view->window_rows < curr_view->list_rows - 1)
		top = curr_view->top_line + curr_view->window_rows - off;
	else
		top = curr_view->top_line + curr_view->window_rows;

	if(keys_info->selector)
		pick_files(curr_view, top, keys_info);
	else
		move_to_list_pos(curr_view, top);
}

/* Go to middle of window. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
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
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, !curr_stats.last_search_backward);
}

/* Move files. */
static void
cmd_P(key_info_t key_info, keys_info_t *keys_info)
{
	put_files(key_info, 1);
}

/* Visual selection of files. */
static void
cmd_V(key_info_t key_info, keys_info_t *keys_info)
{
	enter_visual_mode(0);
}

static void
cmd_ZQ(key_info_t key_info, keys_info_t *keys_info)
{
	comm_quit(0, 0);
}

static void
cmd_ZZ(key_info_t key_info, keys_info_t *keys_info)
{
	comm_quit(1, 0);
}

/* Goto mark. */
static void
cmd_quote(key_info_t key_info, keys_info_t *keys_info)
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
		if(!cfg.auto_ch_pos)
			move_to_list_pos(curr_view, 0);
	}
}

/* Jump to percent of file. */
static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
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
cmd_comma(key_info_t key_info, keys_info_t *keys_info)
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
cmd_dot(key_info_t key_info, keys_info_t *keys_info)
{
	if(0 > cfg.cmd_history_num)
		(void)show_error_msg("Command Error", "Command history list is empty.");
	else
		curr_stats.save_msg = exec_command(cfg.cmd_history[0], curr_view,
				GET_COMMAND);
}

/* Command. */
static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
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
cmd_semicolon(key_info_t key_info, keys_info_t *keys_info)
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
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	curr_stats.last_search_backward = 0;
	enter_cmdline_mode(SEARCH_FORWARD_SUBMODE, L"", NULL);
}

/* Search backward. */
static void
cmd_question(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	curr_stats.last_search_backward = 1;
	enter_cmdline_mode(SEARCH_BACKWARD_SUBMODE, L"", NULL);
}

/* Create link with absolute path */
static void
cmd_al(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_links(curr_view, key_info.reg, 0);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Change word (rename file without extension). */
static void
cmd_cW(key_info_t key_info, keys_info_t *keys_info)
{
	rename_file(curr_view, 1);
}

#ifndef _WIN32
/* Change group. */
static void
cmd_cg(key_info_t key_info, keys_info_t *keys_info)
{
	change_group();
}
#endif

/* Change symbolic link. */
static void
cmd_cl(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = change_link(curr_view);
}

#ifndef _WIN32
/* Change owner. */
static void
cmd_co(key_info_t key_info, keys_info_t *keys_info)
{
	change_owner();
}
#endif

/* Change file attributes (permissions or properties). */
static void
cmd_cp(key_info_t key_info, keys_info_t *keys_info)
{
	enter_attr_mode(curr_view);
}

/* Change word (rename file or files). */
static void
cmd_cw(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->selected_files > 1)
		rename_files(curr_view, NULL, 0, 0);
	else
		rename_file(curr_view, 0);
}

/* Delete file. */
static void
cmd_DD(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 0);
}

/* Delete file. */
static void
cmd_dd(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 1);
}

static void
delete(key_info_t key_info, int use_trash)
{
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

	if(cfg.selection_cp)
	{
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
	}
	else
	{
		int j, k;
		int *i;

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
	}
}

static void
cmd_D_selector(key_info_t key_info, keys_info_t *keys_info)
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
cmd_d_selector(key_info_t key_info, keys_info_t *keys_info)
{
	delete_with_selector(key_info, keys_info, 1);
}

static void
delete_with_selector(key_info_t key_info, keys_info_t *keys_info,
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
cmd_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.view)
	{
		status_bar_error("Another type of file viewing is activated");
		curr_stats.save_msg = 1;
		return;
	}
	enter_view_mode(1);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(key_info.multi, key_info.count, 0, keys_info);
}

/* Updir. */
static void
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	cd_updir(curr_view);
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	handle_file(curr_view, 1, 0);
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Move down one line. */
static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
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
cmd_k(key_info_t key_info, keys_info_t *keys_info)
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
cmd_l(key_info_t key_info, keys_info_t *keys_info)
{
	handle_file(curr_view, 0, 0);
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

/* Set mark. */
static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	add_bookmark(key_info.multi, curr_view->curr_dir,
			get_current_file_name(curr_view));
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, curr_stats.last_search_backward);
}

static void
search(key_info_t key_info, int backward)
{
	int found;

	if(cfg.search_history_num < 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	found = 0;
	if(curr_view->matches == 0)
	{
		const char *pattern = (curr_view->regexp[0] == '\0') ?
				cfg.search_history[0] : curr_view->regexp;
		found = find_pattern(curr_view, pattern, backward, 1);
		curr_stats.save_msg = found;
		key_info.count--;
	}

	if(curr_view->matches == 0)
		return;

	while(key_info.count-- > 0)
	{
		if(backward)
			found += find_previous_pattern(curr_view, cfg.wrap_scan) != 0;
		else
			found += find_next_pattern(curr_view, cfg.wrap_scan) != 0;
	}

	if(!found)
	{
		if(backward)
			status_bar_errorf("Search hit TOP without match for: %s",
					curr_view->regexp);
		else
			status_bar_errorf("Search hit BOTTOM without match for: %s",
					curr_view->regexp);
		curr_stats.save_msg = 1;
		return;
	}

	status_bar_messagef("%c%s", backward ? '?' : '/', curr_view->regexp);
	curr_stats.save_msg = 1;
}

/* Put files. */
static void
cmd_p(key_info_t key_info, keys_info_t *keys_info)
{
	put_files(key_info, 0);
}

static void
put_files(key_info_t key_info, int move)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_files_from_register(curr_view, key_info.reg, move);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Create link with absolute path */
static void
cmd_rl(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_links(curr_view, key_info.reg, 1);
	load_saving_pos(&lwin, 1);
	load_saving_pos(&rwin, 1);
}

/* Tag file. */
static void
cmd_t(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].selected == 0)
	{
		/* The ../ dir cannot be selected */
		if(!pathcmp(curr_view->dir_entry[curr_view->list_pos].name, "../"))
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
cmd_u(key_info_t key_info, keys_info_t *keys_info)
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
cmd_yy(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		pick_files(curr_view, curr_view->list_pos + key_info.count - 1, keys_info);
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

	if(cfg.selection_cp)
	{
		curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
				keys_info->indexes);
	}
	else
	{
		int j, k;
		int *i;

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
	}

	if(key_info.count != NO_COUNT_GIVEN)
		free(keys_info->indexes);
}

static void
cmd_y_selector(key_info_t key_info, keys_info_t *keys_info)
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
cmd_zM(key_info_t key_info, keys_info_t *keys_info)
{
	restore_filename_filter(curr_view);
	set_dot_files_visible(curr_view, 0);
}

/* Remove filename filter. */
static void
cmd_zO(key_info_t key_info, keys_info_t *keys_info)
{
	remove_filename_filter(curr_view);
}

/* Show all hidden files. */
static void
cmd_zR(key_info_t key_info, keys_info_t *keys_info)
{
	remove_filename_filter(curr_view);
	set_dot_files_visible(curr_view, 1);
}

/* Toggle dot files visibility. */
static void
cmd_za(key_info_t key_info, keys_info_t *keys_info)
{
	toggle_dot_files(curr_view);
}

/* Redraw with file in bottom of list. */
void
normal_cmd_zb(key_info_t key_info, keys_info_t *keys_info)
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
cmd_zf(key_info_t key_info, keys_info_t *keys_info)
{
	filter_selected_files(curr_view);
}

/* Hide dot files. */
static void
cmd_zm(key_info_t key_info, keys_info_t *keys_info)
{
	set_dot_files_visible(curr_view, 0);
}

/* Show all the dot files. */
static void
cmd_zo(key_info_t key_info, keys_info_t *keys_info)
{
	set_dot_files_visible(curr_view, 1);
}

static void
cmd_left_paren(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = cmd_paren(0, curr_view->list_rows, -1);
	if(keys_info->selector)
		pick_files(curr_view, pos, keys_info);
	else
		move_to_list_pos(curr_view, pos);
}

static void
cmd_right_paren(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = cmd_paren(-1, curr_view->list_rows - 1, +1);
	if(keys_info->selector)
		pick_files(curr_view, pos, keys_info);
	else
		move_to_list_pos(curr_view, pos);
}

int
cmd_paren(int lb, int ub, int inc)
{
	int pos = curr_view->list_pos;
	switch(abs(curr_view->sort[0]))
	{
		case SORT_BY_EXTENSION:
			{
				const char *name = curr_view->dir_entry[pos].name;
				const char *ext = get_last_ext(name);
				while(pos > lb && pos < ub)
				{
					pos += inc;
					name = curr_view->dir_entry[pos].name;
					if(strcmp(get_last_ext(name), ext) != 0)
						break;
				}
			}
			break;
		case SORT_BY_NAME:
			{
				const char *name = curr_view->dir_entry[pos].name;
				size_t len = get_char_width(name);
				while(pos > lb && pos < ub)
				{
					const char *tmp;
					pos += inc;
					tmp = curr_view->dir_entry[pos].name;
					if(strncmp(name, tmp, len) != 0)
						break;
				}
			}
			break;
		case SORT_BY_INAME:
			{
				const char *name = curr_view->dir_entry[pos].name;
				wchar_t ch = towupper(get_first_wchar(name));
				while(pos > lb && pos < ub)
				{
					const char *tmp;
					tmp = curr_view->dir_entry[pos].name;
					if(towupper(get_first_wchar(tmp)) != ch)
						break;
				}
			}
			break;
#ifndef _WIN32
		case SORT_BY_GROUP_NAME:
		case SORT_BY_GROUP_ID:
			{
				gid_t g = curr_view->dir_entry[pos].gid;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].gid != g)
						break;
				}
			}
			break;
		case SORT_BY_OWNER_NAME:
		case SORT_BY_OWNER_ID:
			{
				uid_t u = curr_view->dir_entry[pos].uid;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].uid != u)
						break;
				}
			}
			break;
		case SORT_BY_MODE:
			{
				mode_t mode = curr_view->dir_entry[pos].mode;
				const char *mode_str = get_mode_str(mode);
				while(pos > lb && pos < ub)
				{
					pos += inc;
					mode = curr_view->dir_entry[pos].mode;
					if(get_mode_str(mode) != mode_str)
						break;
				}
			}
			break;
#endif
		case SORT_BY_SIZE:
			{
				unsigned long long size =
						curr_view->dir_entry[pos].size;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].size != size)
						break;
				}
			}
			break;
		case SORT_BY_TIME_ACCESSED:
			{
				time_t time = curr_view->dir_entry[pos].atime;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].atime != time)
						break;
				}
			}
			break;
		case SORT_BY_TIME_CHANGED:
			{
				time_t time = curr_view->dir_entry[pos].ctime;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].ctime != time)
						break;
				}
			}
			break;
		case SORT_BY_TIME_MODIFIED:
			{
				time_t time = curr_view->dir_entry[pos].mtime;
				while(pos > lb && pos < ub)
				{
					pos += inc;
					if(curr_view->dir_entry[pos].mtime != time)
						break;
				}
			}
			break;
	}
	return pos;
}

static const char *
get_last_ext(const char *name)
{
	const char *ext = strrchr(name, '.');
	if(ext == NULL)
		return "";
	else
		return ext + 1;
}

static wchar_t
get_first_wchar(const char *str)
{
	char tbuf[9];
	wchar_t wbuf[8];

	strncpy(tbuf, str, ARRAY_LEN(tbuf) - 1);
	tbuf[ARRAY_LEN(tbuf) - 1] = '\0';
	mbstowcs(wbuf, tbuf, ARRAY_LEN(wbuf));

	return wbuf[0];
}

/* Redraw with file in top of list. */
void
normal_cmd_zt(key_info_t key_info, keys_info_t *keys_info)
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
normal_cmd_zz(key_info_t key_info, keys_info_t *keys_info)
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
pick_files(FileView *view, int end, keys_info_t *keys_info)
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
	}
	while(x != end);

	if(end < view->list_pos)
	{
#ifndef _WIN32
		memset(&view->dir_mtime, 0, sizeof(view->dir_mtime));
#endif
		view->list_pos = end;
	}
}

static void
selector_S(key_info_t key_info, keys_info_t *keys_info)
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
		if(pathcmp(curr_view->dir_entry[x].name, "../") == 0)
			continue;
		if(curr_view->dir_entry[x].selected)
			continue;
		keys_info->indexes[i++] = x;
	}
	keys_info->count = i;
}

static void
selector_a(key_info_t key_info, keys_info_t *keys_info)
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
		if(pathcmp(curr_view->dir_entry[x].name, "../") == 0)
			continue;
		keys_info->indexes[i++] = x;
	}
	keys_info->count = i;
}

static void
selector_s(key_info_t key_info, keys_info_t *keys_info)
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

int
find_npattern(FileView *view, const char *pattern, int backward, int move)
{
	int i;
	int found = find_pattern(view, pattern, backward, move);
	for(i = 0; i < search_repeat - 1; i++)
	{
		if(backward)
			found += find_previous_pattern(view, cfg.wrap_scan) != 0;
		else
			found += find_next_pattern(view, cfg.wrap_scan) != 0;
	}
	return found;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
