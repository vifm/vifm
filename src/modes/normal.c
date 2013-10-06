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

#include "normal.h"

#include <curses.h>

#include <pthread.h>

#include <assert.h> /* assert() */
#include <stdlib.h> /* free() */
#include <string.h>
#include <wctype.h> /* wtoupper() */

#include "../cfg/config.h"
#include "../engine/keys.h"
#include "../menus/menus.h"
#include "../utils/fs_limits.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/tree.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../bookmarks.h"
#include "../color_scheme.h"
#include "../commands.h"
#include "../filelist.h"
#include "../fileops.h"
#include "../quickview.h"
#include "../registers.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "../ui.h"
#include "../undo.h"
#include "dialogs/attr_dialog.h"
#include "file_info.h"
#include "cmdline.h"
#include "modes.h"
#include "view.h"
#include "visual.h"

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
static void page_scroll(int base, int direction);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_emarkemark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_emark_selector(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void scroll_view(ssize_t offset);
static void cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info);
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
static void cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info);
static FileView * get_view(void);
static void move_splitter(key_info_t key_info, int fact);
static void cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void go_to_other_window(void);
static int try_switch_into_view_mode(void);
static void cmd_quote(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dollar(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_equal(key_info_t key_info, keys_info_t *keys_info);
static void cmd_comma(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dot(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zero(key_info_t key_info, keys_info_t *keys_info);
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
static void pick_or_move(keys_info_t *keys_info, int new_pos);
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
static void cmd_gh(key_info_t key_info, keys_info_t *keys_info);
#ifdef _WIN32
static void cmd_gr(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_gs(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gU(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gUgg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gu(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gugg(key_info_t key_info, keys_info_t *keys_info);
static void do_gu(key_info_t key_info, keys_info_t *keys_info, int upper);
static void cmd_gv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void go_to_prev(key_info_t key_info, keys_info_t *keys_info, int def,
		int step);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(key_info_t key_info, int backward);
static void cmd_l(key_info_t key_info, keys_info_t *keys_info);
static void go_to_next(key_info_t key_info, keys_info_t *keys_info, int def,
		int step);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void put_files(key_info_t key_info, int move);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_question(key_info_t key_info, keys_info_t *keys_info);
static void activate_search(int count, int back, int external);
static void cmd_q_equals(key_info_t key_info, keys_info_t *keys_info);
static void cmd_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_yy(key_info_t key_info, keys_info_t *keys_info);
static int calc_pick_files_end_pos(const FileView *view, int count);
static void cmd_y_selector(key_info_t key_info, keys_info_t *keys_info);
static void free_list_of_file_indexes(keys_info_t *keys_info);
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
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x0f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_o}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x12", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_r}}},
	{L"\x15", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_wb}}},
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
	{L"!", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_emark_selector}}},
	{L"!!", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_emarkemark}}},
	{L"^", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zero}}},
	{L"$", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dollar}}},
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L"=", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_equal}}},
	{L",", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L".", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dot}}},
	{L"0", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zero}}},
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
	{L"DD", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_DD}}},
	{L"dd", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dd}}},
	{L"D", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_D_selector}}},
	{L"d", {BUILTIN_WAIT_POINT, FOLLOWED_BY_SELECTOR, {.handler = cmd_d_selector}}},
	{L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_e}}},
	{L"f", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gA", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gA}}},
	{L"ga", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ga}}},
	{L"gf", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gf}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"gh", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gh}}},
	{L"gj", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"gk", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"gl", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
#ifdef _WIN32
	{L"gr", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gr}}},
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
	{L"q:", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q_colon}}},
	{L"q/", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q_slash}}},
	{L"q?", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q_question}}},
	{L"q=", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_q_equals}}},
	{L"t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_t}}},
	{L"u", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"yy", {BUILTIN_NIM_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_yy}}},
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
	{L"^", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zero}}},
	{L"$", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dollar}}},
	{L",", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L"0", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zero}}},
	{L";", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}}, /* Ctrl-N */
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}}, /* Ctrl-P */
	{L"F", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"S", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_S}}},
	{L"a", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = selector_a}}},
	{L"f", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
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
	if(can_scroll_up(curr_view))
	{
		page_scroll(get_last_visible_file(curr_view), -1);
	}
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	clean_selected_files(curr_view);
	redraw_current_view();
	curs_set(FALSE);
}

/* Scroll view down by half of its height. */
static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_last_line(curr_view))
	{
		scroll_view(curr_view->window_cells/2);
	}
}

/* Scroll pane one line down. */
static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(correct_list_pos_on_scroll_down(curr_view, 1))
	{
		scroll_down(curr_view, 1);
		redraw_current_view();
	}
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_down(curr_view))
	{
		page_scroll(curr_view->top_line, 1);
	}
}

/* Scrolls pane by one view in both directions. The direction should be 1 or
 * -1. */
static void
page_scroll(int base, int direction)
{
	/* Two lines gap. */
	int offset = (curr_view->window_rows - 1)*curr_view->column_count;
	curr_view->list_pos = base + direction*offset;
	scroll_by_files(curr_view, direction*offset);
	redraw_current_view();
}

static void
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	enter_file_info_mode(curr_view);
}

static void
cmd_space(key_info_t key_info, keys_info_t *keys_info)
{
	go_to_other_pane();
}

/* Processes !! normal mode command, which can be prepended by a count, which is
 * treated as number of lines to be processed. */
static void
cmd_emarkemark(key_info_t key_info, keys_info_t *keys_info)
{
	wchar_t buf[16] = L".!";
	if(key_info.count != NO_COUNT_GIVEN && key_info.count != 1)
	{
		if(curr_view->list_pos + key_info.count - 1 >= curr_view->list_rows - 1)
		{
			wcscpy(buf, L".,$!");
		}
		else
		{
			my_swprintf(buf, ARRAY_LEN(buf), L".,.+%d!", key_info.count - 1);
		}
	}
	enter_cmdline_mode(CMD_SUBMODE, buf, NULL);
}

/* Processes !<selector> normal mode command.  Processes results of applying
 * selector and invokes cmd_emarkemark(...) to do the rest. */
static void
cmd_emark_selector(key_info_t key_info, keys_info_t *keys_info)
{
	int i, m;

	if(keys_info->count == 0)
	{
		cmd_emarkemark(key_info, keys_info);
		return;
	}

	m = keys_info->indexes[0];
	for(i = 1; i < keys_info->count; i++)
	{
		if(keys_info->indexes[i] > m)
		{
			m = keys_info->indexes[i];
		}
	}

	free_list_of_file_indexes(keys_info);

	key_info.count = m - curr_view->list_pos + 1;
	cmd_emarkemark(key_info, keys_info);
}

static void
cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info)
{
	if(cfg.tab_switches_pane)
	{
		cmd_space(key_info, keys_info);
	}
	else
	{
		if(curr_view->history_pos < curr_view->history_num - 1)
		{
			goto_history_pos(curr_view, curr_view->history_pos + 1);
		}
	}
}

/* Clear screen and redraw. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	update_screen(UT_FULL);
	curs_set(FALSE);
}

/* Enters directory or runs file. */
static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	handle_file(curr_view, 0, 0);
	clean_selected_files(curr_view);
	redraw_current_view();
}

static void
cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->history_pos != 0)
	{
		goto_history_pos(curr_view, curr_view->history_pos - 1);
	}
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

/* Scroll view up by half of its height. */
static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_first_line(curr_view))
	{
		scroll_view(-(ssize_t)curr_view->window_cells/2);
	}
}

/* Scrolls view by specified number of files. */
static void
scroll_view(ssize_t offset)
{
	offset = ROUND_DOWN(offset, curr_view->column_count);
	curr_view->list_pos += offset;
	correct_list_pos(curr_view, offset);
	go_to_start_of_line(curr_view);
	scroll_by_files(curr_view, offset);
	redraw_current_view();
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

/* Leave only one (current) pane. */
static void
cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info)
{
	only();
}

/* To split pane horizontally. */
static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	split_view(HSPLIT);
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
	split_view(VSPLIT);
}

static void
cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info)
{
	go_to_other_window();
}

static void
cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(curr_view, 0, 1);
}

static void
cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(curr_view, 1, 0);
}

static void
cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(curr_view, 1, 1);
}

static void
cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(curr_view, 0, 0);
}

void
normal_cmd_ctrl_wequal(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.splitter_pos = -1;
	update_screen(UT_REDRAW);
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
	update_screen(UT_REDRAW);
}

/* Switch views. */
static void
cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info)
{
	switch_panes();
	if(curr_stats.view)
	{
		change_window();
		(void)try_switch_into_view_mode();
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
	if(correct_list_pos_on_scroll_up(curr_view, 1))
	{
		scroll_up(curr_view, 1);
		redraw_current_view();
	}
}

static void
cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info)
{
	if(!try_switch_into_view_mode())
	{
		if(other_view->explore_mode)
		{
			go_to_other_window();
		}
	}
}

/* Activates view mode on the preview, or just switches active pane. */
static void
go_to_other_window(void)
{
	if(!try_switch_into_view_mode())
	{
		go_to_other_pane();
	}
}

/* Tries to go into view mode in case the other pane displays preview.  Returns
 * non-zero on success, otherwise zero is returned. */
static int
try_switch_into_view_mode(void)
{
	if(curr_stats.view)
	{
		enter_view_mode(0);
		return 1;
	}
	return 0;
}

/* Clone selection.  Count specifies number of copies of each file or directory
 * to create (one by default). */
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
	int new_pos;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;

	new_pos = ROUND_DOWN(key_info.count - 1, curr_view->column_count);
	pick_or_move(keys_info, new_pos);
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

	remove_last_path_component(dir_size->path);
	if(path_starts_with(lwin.curr_dir, dir_size->path))
		request_view_update(&lwin);
	if(path_starts_with(rwin.curr_dir, dir_size->path))
		request_view_update(&rwin);

	free(dir_size->path);
	free(dir_size);
	return NULL;
}

static void
cmd_gf(key_info_t key_info, keys_info_t *keys_info)
{
	clean_selected_files(curr_view);
	handle_file(curr_view, 0, 1);
	redraw_current_view();
}

/* Jump to the top of the list or to specified line. */
static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	pick_or_move(keys_info, key_info.count - 1);
}

/* Goes to parent directory regardless of ls-like state. */
static void
cmd_gh(key_info_t key_info, keys_info_t *keys_info)
{
	cd_updir(curr_view);
}

#ifdef _WIN32
static void
cmd_gr(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.as_admin = 1;
	handle_file(curr_view, 0, 0);
	curr_stats.as_admin = 0;
	clean_selected_files(curr_view);
	redraw_current_view();
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
	redraw_current_view();
}

/* Handles gU<selector>, gUgU and gUU normal mode commands, which convert file
 * name symbols to uppercase. */
static void
cmd_gU(key_info_t key_info, keys_info_t *keys_info)
{
	do_gu(key_info, keys_info, 1);
}

/* Special handler for combination of gU<selector> and gg motion. */
static void
cmd_gUgg(key_info_t key_info, keys_info_t *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	do_gu(key_info, keys_info, 1);
}

/* Handles gu<selector>, gugu and guu normal mode commands, which convert file
 * name symbols to lowercase. */
static void
cmd_gu(key_info_t key_info, keys_info_t *keys_info)
{
	do_gu(key_info, keys_info, 0);
}

/* Special handler for combination of gu<selector> and gg motion. */
static void
cmd_gugg(key_info_t key_info, keys_info_t *keys_info)
{
	pick_files(curr_view, 0, keys_info);
	do_gu(key_info, keys_info, 0);
}

/* Handles gUU, gU<selector>, gUgU, gu<selector>, guu and gugu commands. */
static void
do_gu(key_info_t key_info, keys_info_t *keys_info, int upper)
{
	/* If nothing was selected, do selection count elements down. */
	if(keys_info->count == 0)
	{
		const int count = (key_info.count == NO_COUNT_GIVEN)
			? 0
			: (key_info.count - 1);
		pick_files(curr_view, curr_view->list_pos + count, keys_info);
	}

	curr_stats.save_msg = change_case(curr_view, upper, keys_info->count,
			keys_info->indexes);
	free_list_of_file_indexes(keys_info);
}

static void
cmd_gv(key_info_t key_info, keys_info_t *keys_info)
{
	enter_visual_mode(1);
}

/* Go to the first file in window. */
static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	size_t new_pos = get_window_top_pos(curr_view);
	pick_or_move(keys_info, new_pos);
}

/* Go to the last file in window. */
static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	size_t new_pos = get_window_bottom_pos(curr_view);
	pick_or_move(keys_info, new_pos);
}

/* Go to middle line of window. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	size_t new_pos = get_window_middle_pos(curr_view);
	pick_or_move(keys_info, new_pos);
}

/* Picks files or moves cursor depending whether key was pressed as a selector
 * or not. */
static void
pick_or_move(keys_info_t *keys_info, int new_pos)
{
	if(keys_info->selector)
	{
		pick_files(curr_view, new_pos, keys_info);
	}
	else
	{
		move_to_list_pos(curr_view, new_pos);
	}
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
		int pos = check_mark_directory(curr_view, key_info.multi);
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

/* Move cursor to the last column in ls-view sub-mode. */
static void
cmd_dollar(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_last_column(curr_view) || keys_info->selector)
	{
		pick_or_move(keys_info, get_end_of_line(curr_view));
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
	pick_or_move(keys_info, line - 1);
}

/* Go to local filter mode. */
static void
cmd_equal(key_info_t key_info, keys_info_t *keys_info)
{
	wchar_t *previous = to_wide(curr_view->local_filter.filter.raw);
	enter_cmdline_mode(FILTER_SUBMODE, previous, NULL);
	free(previous);
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
	curr_stats.save_msg = exec_commands(curr_stats.last_cmdline_command,
			curr_view, GET_COMMAND);
}

/* Move cursor to the first column in ls-view sub-mode. */
static void
cmd_zero(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_first_column(curr_view) || keys_info->selector)
	{
		pick_or_move(keys_info, get_start_of_line(curr_view));
	}
}

/* Switch to command-line mode. */
static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	wchar_t buf[16] = L"";
	if(key_info.count != NO_COUNT_GIVEN)
		my_swprintf(buf, ARRAY_LEN(buf), L".,.+%d", key_info.count - 1);
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
	activate_search(key_info.count, 0, 0);
}

/* Search backward. */
static void
cmd_question(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 1, 0);
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
	rename_current_file(curr_view, 1);
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
		rename_current_file(curr_view, 0);
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
	keys_info_t keys_info = {};

	if(!check_if_dir_writable(DR_CURRENT, curr_view->curr_dir))
		return;

	curr_stats.confirmed = 0;
	if(!use_trash && cfg.confirm)
	{
		if(!query_user_menu("Permanent deletion",
					"Are you sure you want to delete files permanently?"))
			return;
		curr_stats.confirmed = 1;
	}

	if(key_info.count != NO_COUNT_GIVEN)
	{
		const int end_pos = calc_pick_files_end_pos(curr_view, key_info.count);
		pick_files(curr_view, end_pos, &keys_info);
	}
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

	if(!cfg.selection_is_primary && key_info.count == NO_COUNT_GIVEN)
	{
		pick_files(curr_view, curr_view->list_pos, &keys_info);
	}
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, keys_info.count,
			keys_info.indexes, use_trash);

	free_list_of_file_indexes(&keys_info);
}

static void
cmd_D_selector(key_info_t key_info, keys_info_t *keys_info)
{
	if(!check_if_dir_writable(DR_CURRENT, curr_view->curr_dir))
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

/* Removes (permanently or just moving to trash) files using selector.
 * Processes d<selector> and D<selector> normal mode commands.  On moving to
 * trash files are put in specified register (unnamed by default). */
static void
delete_with_selector(key_info_t key_info, keys_info_t *keys_info, int use_trash)
{
	if(keys_info->count == 0)
		return;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes, use_trash);

	free_list_of_file_indexes(keys_info);
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

/* Updir or one file left in less-like sub-mode. */
static void
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->ls_view)
	{
		go_to_prev(key_info, keys_info, 1, 1);
	}
	else
	{
		cmd_gh(key_info, keys_info);
	}
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	handle_file(curr_view, 1, 0);
	clean_selected_files(curr_view);
	redraw_current_view();
}

/* Move down one line. */
static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_last_line(curr_view))
	{
		go_to_next(key_info, keys_info, 1, curr_view->column_count);
	}
}

/* Move up one line. */
static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(!at_first_line(curr_view))
	{
		go_to_prev(key_info, keys_info, 1, curr_view->column_count);
	}
}

/* Moves cursor to one of previous files in the list. */
static void
go_to_prev(key_info_t key_info, keys_info_t *keys_info, int def, int step)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = def;
	key_info.count *= step;

	pick_or_move(keys_info, curr_view->list_pos - key_info.count);
}

static void
cmd_l(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->ls_view)
	{
		go_to_next(key_info, keys_info, 1, 1);
	}
	else
	{
		cmd_ctrl_m(key_info, keys_info);
	}
}

/* Moves cursor to one of next files in the list. */
static void
go_to_next(key_info_t key_info, keys_info_t *keys_info, int def, int step)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = def;
	key_info.count *= step;

	pick_or_move(keys_info, curr_view->list_pos + key_info.count);
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

	if(hist_is_empty(&cfg.search_hist))
	{
		return;
	}

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	found = 0;
	if(curr_view->matches == 0)
	{
		const char *pattern = (curr_view->regexp[0] == '\0') ?
				cfg.search_hist.items[0] : curr_view->regexp;
		curr_stats.save_msg = find_pattern(curr_view, pattern, backward, 1, &found);
		if(!found)
		{
			return;
		}
		key_info.count--;
	}

	while(key_info.count-- > 0)
	{
		found += goto_search_match(curr_view, backward) != 0;
	}

	if(found)
	{
		status_bar_messagef("%c%s", backward ? '?' : '/', curr_view->regexp);
	}
	else
	{
		print_search_fail_msg(curr_view, backward);
	}

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

/* Runs external editor to get command-line command and then executes it. */
static void
cmd_q_colon(key_info_t key_info, keys_info_t *keys_info)
{
	get_and_execute_command("", 0U, GET_COMMAND);
}

/* Runs external editor to get search pattern and then executes it. */
static void
cmd_q_slash(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 0, 1);
}

/* Runs external editor to get search pattern and then executes it. */
static void
cmd_q_question(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 1, 1);
}

/* Activates search of different kinds. */
static void
activate_search(int count, int back, int external)
{
	search_repeat = (count == NO_COUNT_GIVEN) ? 1 : count;
	curr_stats.last_search_backward = back;
	if(external)
	{
		const int type = back ? GET_BSEARCH_PATTERN : GET_FSEARCH_PATTERN;
		get_and_execute_command("", 0U, type);
	}
	else
	{
		const int type = back ? SEARCH_BACKWARD_SUBMODE : SEARCH_FORWARD_SUBMODE;
		enter_cmdline_mode(type, L"", NULL);
	}
}

/* Runs external editor to get local filter pattern and then executes it. */
static void
cmd_q_equals(key_info_t key_info, keys_info_t *keys_info)
{
	get_and_execute_command("", 0U, GET_FILTER_PATTERN);
}

/* Tag file. */
static void
cmd_t(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].selected == 0)
	{
		/* The ../ dir cannot be selected */
		if(is_parent_dir(curr_view->dir_entry[curr_view->list_pos].name))
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
	{
		const int end_pos = calc_pick_files_end_pos(curr_view, key_info.count);
		pick_files(curr_view, end_pos, keys_info);
	}
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

	if(!cfg.selection_is_primary && key_info.count == NO_COUNT_GIVEN)
	{
		pick_files(curr_view, curr_view->list_pos, keys_info);
	}
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);

	free(keys_info->indexes);
}

/* Calculates end position for pick_files(...) function using cursor position
 * and count of a command.  Considers possible integer overflow. */
static int
calc_pick_files_end_pos(const FileView *view, int count)
{
	/* Way of comparing values makes difference!  This way it will work even when
	 * count equals to INT_MAX.  Don't change it! */
	if(count > view->list_rows - view->list_pos)
	{
		return view->list_rows - 1;
	}
	else
	{
		return view->list_pos + count - 1;
	}
}

/* Processes y<selector> normal mode command, which copies files to one of
 * registers (unnamed by default). */
static void
cmd_y_selector(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->count == 0)
		return;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);

	free_list_of_file_indexes(keys_info);
}

/* Frees memory allocated for selected files list in keys_info_t structure and
 * clears it. */
static void
free_list_of_file_indexes(keys_info_t *keys_info)
{
	free(keys_info->indexes);
	keys_info->indexes = NULL;
	keys_info->count = 0;
}

/* Filter the files matching the filename filter. */
static void
cmd_zM(key_info_t key_info, keys_info_t *keys_info)
{
	restore_filename_filter(curr_view);
	local_filter_restore(curr_view);
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
	local_filter_remove(curr_view);
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
	if(can_scroll_up(curr_view))
	{
		int bottom = get_window_bottom_pos(curr_view);
		scroll_up(curr_view, bottom - curr_view->list_pos);
		redraw_current_view();
	}
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
	pick_or_move(keys_info, pos);
}

static void
cmd_right_paren(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = cmd_paren(-1, curr_view->list_rows - 1, +1);
	pick_or_move(keys_info, pos);
}

int
cmd_paren(int lb, int ub, int inc)
{
	int pos = curr_view->list_pos;
	dir_entry_t *pentry = &curr_view->dir_entry[pos];
	const char *ext = get_last_ext(pentry->name);
	size_t char_width = get_char_width(pentry->name);
	wchar_t ch = towupper(get_first_wchar(pentry->name));
#ifndef _WIN32
	const char *mode_str = get_mode_str(pentry->mode);
#endif
	int sort_key = abs(curr_view->sort[0]);
	while(pos > lb && pos < ub)
	{
		dir_entry_t *nentry;
		pos += inc;
		nentry = &curr_view->dir_entry[pos];
		switch(sort_key)
		{
			case SORT_BY_EXTENSION:
				if(strcmp(get_last_ext(nentry->name), ext) != 0)
					return pos;
				break;
			case SORT_BY_NAME:
				if(strncmp(pentry->name, nentry->name, char_width) != 0)
					return pos;
				break;
			case SORT_BY_INAME:
				if(towupper(get_first_wchar(nentry->name)) != ch)
					return pos;
				break;
#ifndef _WIN32
		case SORT_BY_GROUP_NAME:
		case SORT_BY_GROUP_ID:
				if(nentry->gid != pentry->gid)
					return pos;
				break;
		case SORT_BY_OWNER_NAME:
		case SORT_BY_OWNER_ID:
				if(nentry->uid != pentry->uid)
					return pos;
				break;
		case SORT_BY_MODE:
				if(get_mode_str(nentry->mode) != mode_str)
					return pos;
				break;
#endif
		case SORT_BY_SIZE:
				if(nentry->size != pentry->size)
					return pos;
				break;
		case SORT_BY_TIME_ACCESSED:
				if(nentry->atime != pentry->atime)
					return pos;
				break;
		case SORT_BY_TIME_CHANGED:
				if(nentry->ctime != pentry->ctime)
					return pos;
				break;
		case SORT_BY_TIME_MODIFIED:
				if(nentry->mtime != pentry->mtime)
					return pos;
				break;

		default:
				assert(0);
				break;
		}
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

/* Redraw with file in top of list. */
void
normal_cmd_zt(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_down(curr_view))
	{
		int top = get_window_top_pos(curr_view);
		scroll_down(curr_view, curr_view->list_pos - top);
		redraw_current_view();
	}
}

/* Redraw with file in center of list. */
void
normal_cmd_zz(key_info_t key_info, keys_info_t *keys_info)
{
	if(!all_files_visible(curr_view))
	{
		int middle = get_window_middle_pos(curr_view);
		scroll_by_files(curr_view, curr_view->list_pos - middle);
		redraw_current_view();
	}
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
		show_error_msg("Memory Error", "Unable to allocate enough memory");
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
		request_view_update(view);
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
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(is_parent_dir(curr_view->dir_entry[x].name))
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
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(is_parent_dir(curr_view->dir_entry[x].name))
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
		show_error_msg("Memory Error", "Unable to allocate enough memory");
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
find_npattern(FileView *view, const char *pattern, int backward)
{
	int i;
	int found;
	(void)find_pattern(view, pattern, backward, 1, &found);
	for(i = 0; i < search_repeat - 1; i++)
	{
		found += goto_search_match(view, backward) != 0;
	}
	return found;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
