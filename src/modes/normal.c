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

#include <regex.h> /* regexec() */

#include <sys/types.h> /* ssize_t */

#include <assert.h> /* assert() */
#include <ctype.h> /* tolower() */
#include <stddef.h> /* size_t wchar_t */
#include <stdio.h>  /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strncmp() strspn() */
#include <wctype.h> /* towupper() */
#include <wchar.h> /* wcscpy() */

#include "../cfg/config.h"
#include "../compat/curses.h"
#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../engine/variables.h"
#include "../menus/filetypes_menu.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../ui/fileview.h"
#include "../ui/quickview.h"
#include "../ui/statusbar.h"
#include "../ui/tabs.h"
#include "../ui/ui.h"
#include "../utils/hist.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../compare.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../filtering.h"
#include "../flist_pos.h"
#include "../flist_sel.h"
#include "../fops_common.h"
#include "../fops_misc.h"
#include "../fops_put.h"
#include "../fops_rename.h"
#include "../marks.h"
#include "../registers.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "../types.h"
#include "../undo.h"
#include "../vifm.h"
#include "dialogs/attr_dialog.h"
#include "file_info.h"
#include "cmdline.h"
#include "modes.h"
#include "view.h"
#include "visual.h"
#include "wk.h"

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
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
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
static void cmd_ctrl_wz(key_info_t key_info, keys_info_t *keys_info);
static int is_left_or_top(void);
static view_t * pick_view(void);
static void cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void go_to_other_window(void);
static int try_switch_into_view_mode(void);
static void cmd_quote(key_info_t key_info, keys_info_t *keys_info);
static void sug_cmd_quote(vle_keys_list_cb cb);
static void sug_sel_quote(vle_keys_list_cb cb);
static void cmd_dollar(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_equal(key_info_t key_info, keys_info_t *keys_info);
static void cmd_comma(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dot(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zero(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_semicolon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_qmark(key_info_t key_info, keys_info_t *keys_info);
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
static void cmd_av(key_info_t key_info, keys_info_t *keys_info);
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
static void call_delete(key_info_t key_info, keys_info_t *keys_info,
		int use_trash);
static void cmd_do(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dp(key_info_t key_info, keys_info_t *keys_info);
static void cmd_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gA(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ga(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gF(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gh(key_info_t key_info, keys_info_t *keys_info);
#ifdef _WIN32
static void cmd_gr(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_gs(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gT(key_info_t key_info, keys_info_t *keys_info);
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
static void go_to_prev(key_info_t key_info, keys_info_t *keys_info, int step);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(key_info_t key_info, int backward);
static void cmd_l(key_info_t key_info, keys_info_t *keys_info);
static void go_to_next(key_info_t key_info, keys_info_t *keys_info, int step);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void call_put_files(key_info_t key_info, int move);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rl(key_info_t key_info, keys_info_t *keys_info);
static void call_put_links(key_info_t key_info, int relative);
static void cmd_q_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_question(key_info_t key_info, keys_info_t *keys_info);
static void activate_search(int count, int back, int external);
static void cmd_q_equals(key_info_t key_info, keys_info_t *keys_info);
static void cmd_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_yy(key_info_t key_info, keys_info_t *keys_info);
static int calc_pick_files_end_pos(const view_t *view, int count);
static void cmd_y_selector(key_info_t key_info, keys_info_t *keys_info);
static void yank(key_info_t key_info, keys_info_t *keys_info);
static void free_list_of_file_indexes(keys_info_t *keys_info);
static void cmd_zM(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zO(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zR(key_info_t key_info, keys_info_t *keys_info);
static void cmd_za(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zm(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zo(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zr(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zx(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_R(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_R(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_z(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_z(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_curly_bracket(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_curly_bracket(key_info_t key_info,
		keys_info_t *keys_info);
static void pick_files(view_t *view, int end, keys_info_t *keys_info);
static void selector_S(key_info_t key_info, keys_info_t *keys_info);
static void selector_a(key_info_t key_info, keys_info_t *keys_info);
static void selector_s(key_info_t key_info, keys_info_t *keys_info);
static void set_pos_in_curr_view(int pos);
static void handle_mouse_event(key_info_t key_info, keys_info_t *keys_info);

static int last_fast_search_char;
static int last_fast_search_backward = -1;

/* Number of search repeats (e.g. counter passed to n or N key). */
static int search_repeat;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_a,           {{&cmd_ctrl_a}, .descr = "increment number in names"}},
	{WK_C_b,           {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{WK_C_c,           {{&cmd_ctrl_c}, .descr = "reset selection and highlight"}},
	{WK_C_d,           {{&cmd_ctrl_d}, .descr = "scroll half-page down"}},
	{WK_C_e,           {{&cmd_ctrl_e}, .descr = "scroll one line down"}},
	{WK_C_f,           {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{WK_C_g,           {{&cmd_ctrl_g}, .descr = "display file info"}},
	{WK_C_i,           {{&cmd_ctrl_i}, .descr = "switch pane/history forward"}},
	{WK_C_l,           {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_C_m,           {{&cmd_return}, .descr = "open current item(s)"}},
	{WK_C_n,           {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_o,           {{&cmd_ctrl_o}, .descr = "history backward"}},
	{WK_C_p,           {{&cmd_k},      .descr = "go to item above"}},
	{WK_C_r,           {{&cmd_ctrl_r}, .descr = "redo operation"}},
	{WK_C_u,           {{&cmd_ctrl_u}, .descr = "scroll half-page up"}},
	{WK_C_w WK_H,      {{&cmd_ctrl_wH}, .descr = "move window to the left"}},
	{WK_C_w WK_J,      {{&cmd_ctrl_wJ}, .descr = "move window to the bottom"}},
	{WK_C_w WK_K,      {{&cmd_ctrl_wK}, .descr = "move window to the top"}},
	{WK_C_w WK_L,      {{&cmd_ctrl_wL}, .descr = "move window to the right"}},
	{WK_C_w WK_C_b,    {{&cmd_ctrl_wb}, .descr = "go to bottom-right window"}},
	{WK_C_w WK_b,      {{&cmd_ctrl_wb}, .descr = "go to bottom-right window"}},
	{WK_C_w WK_C_h,    {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{WK_C_w WK_h,      {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{WK_C_w WK_C_j,    {{&cmd_ctrl_wj}, .descr = "go to bottom window"}},
	{WK_C_w WK_j,      {{&cmd_ctrl_wj}, .descr = "go to bottom window"}},
	{WK_C_w WK_C_k,    {{&cmd_ctrl_wk}, .descr = "go to top window"}},
	{WK_C_w WK_k,      {{&cmd_ctrl_wk}, .descr = "go to top window"}},
	{WK_C_w WK_C_l,    {{&cmd_ctrl_wl}, .descr = "go to right window"}},
	{WK_C_w WK_l,      {{&cmd_ctrl_wl}, .descr = "go to right window"}},
	{WK_C_w WK_C_o,    {{&cmd_ctrl_wo}, .descr = "single-pane mode"}},
	{WK_C_w WK_o,      {{&cmd_ctrl_wo}, .descr = "single-pane mode"}},
	{WK_C_w WK_C_p,    {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_p,      {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_C_s,    {{&cmd_ctrl_ws}, .descr = "horizontal split layout"}},
	{WK_C_w WK_s,      {{&cmd_ctrl_ws}, .descr = "horizontal split layout"}},
	{WK_C_w WK_C_t,    {{&cmd_ctrl_wt}, .descr = "go to top-left window"}},
	{WK_C_w WK_t,      {{&cmd_ctrl_wt}, .descr = "go to top-left window"}},
	{WK_C_w WK_C_v,    {{&cmd_ctrl_wv}, .descr = "vertical split layout"}},
	{WK_C_w WK_v,      {{&cmd_ctrl_wv}, .descr = "vertical split layout"}},
	{WK_C_w WK_C_w,    {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_w,      {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_C_x,    {{&cmd_ctrl_wx}, .descr = "exchange panes"}},
	{WK_C_w WK_x,      {{&cmd_ctrl_wx}, .descr = "exchange panes"}},
	{WK_C_w WK_C_z,    {{&cmd_ctrl_wz}, .descr = "exit preview/view modes"}},
	{WK_C_w WK_z,      {{&cmd_ctrl_wz}, .descr = "exit preview/view modes"}},
	{WK_C_w WK_EQUALS, {{&modnorm_ctrl_wequal},   .nim = 1, .descr = "size panes equally"}},
	{WK_C_w WK_LT,     {{&modnorm_ctrl_wless},    .nim = 1, .descr = "decrease pane size by one"}},
	{WK_C_w WK_GT,     {{&modnorm_ctrl_wgreater}, .nim = 1, .descr = "increase pane size by one"}},
	{WK_C_w WK_PLUS,   {{&modnorm_ctrl_wplus},    .nim = 1, .descr = "increase pane size by one"}},
	{WK_C_w WK_MINUS,  {{&modnorm_ctrl_wminus},   .nim = 1, .descr = "decrease pane size by one"}},
	{WK_C_w WK_BAR,    {{&modnorm_ctrl_wpipe},    .nim = 1, .descr = "maximize pane size"}},
	{WK_C_w WK_USCORE, {{&modnorm_ctrl_wpipe},    .nim = 1, .descr = "maximize pane size"}},
	{WK_C_x,           {{&cmd_ctrl_x}, .descr = "decrement number in names"}},
	{WK_C_y,           {{&cmd_ctrl_y}, .descr = "scroll one line up"}},
	{WK_ESC,           {{&cmd_ctrl_c}, .descr = "reset selection and highlight"}},
	{WK_QUOTE,         {{&cmd_quote}, FOLLOWED_BY_MULTIKEY, .descr = "navigate to mark", .suggest = &sug_cmd_quote}},
	{WK_SPACE,         {{&cmd_space},  .descr = "switch pane"}},
	{WK_EM,            {{&cmd_emark_selector}, FOLLOWED_BY_SELECTOR, .descr = "make cmdline range"}},
	{WK_EM WK_EM,      {{&cmd_emarkemark}, .descr = "make cmdline range"}},
	{WK_CARET,         {{&cmd_zero},    .descr = "go to first column"}},
	{WK_DOLLAR,        {{&cmd_dollar},  .descr = "go to last column"}},
	{WK_PERCENT,       {{&cmd_percent}, .descr = "go to [count]% position"}},
	{WK_EQUALS,        {{&cmd_equal},   .descr = "edit local filter"}},
	{WK_COMMA,         {{&cmd_comma},   .descr = "repeat char-search backward"}},
	{WK_DOT,           {{&cmd_dot},     .descr = "repeat last cmdline command"}},
	{WK_ZERO,          {{&cmd_zero},    .descr = "go to first column"}},
	{WK_COLON,         {{&cmd_colon},   .descr = "go to cmdline mode"}},
	{WK_SCOLON,        {{&cmd_semicolon}, .descr = "repeat char-search forward"}},
	{WK_SLASH,         {{&cmd_slash},     .descr = "search forward"}},
	{WK_QM,            {{&cmd_qmark},     .descr = "search backward"}},
	{WK_C,             {{&cmd_C}, .descr = "clone files"}},
	{WK_F,             {{&cmd_F}, FOLLOWED_BY_MULTIKEY, .descr = "char-search backward"}},
	{WK_G,             {{&cmd_G}, .descr = "go to the last item"}},
	{WK_H,             {{&cmd_H}, .descr = "go to top of viewport"}},
	{WK_L,             {{&cmd_L}, .descr = "go to bottom of viewport"}},
	{WK_M,             {{&cmd_M}, .descr = "go to middle of viewport"}},
	{WK_N,             {{&cmd_N}, .descr = "go to previous search match"}},
	{WK_P,             {{&cmd_P}, .descr = "put files by moving them"}},
	{WK_V,             {{&cmd_V}, .descr = "go to visual mode"}},
	{WK_Y,             {{&cmd_yy}, .descr = "yank files"}},
	{WK_Z WK_Q,        {{&cmd_ZQ}, .descr = "exit without saving state"}},
	{WK_Z WK_Z,        {{&cmd_ZZ}, .descr = "exit the application"}},
	{WK_a WK_l,        {{&cmd_al}, .descr = "put files creating absolute symlinks"}},
	{WK_a WK_v,        {{&cmd_av}, .descr = "go to visual amend mode"}},
	{WK_c WK_W,        {{&cmd_cW}, .descr = "rename root of current file"}},
	{WK_c WK_l,        {{&cmd_cl}, .descr = "change symlink target"}},
	{WK_c WK_p,        {{&cmd_cp}, .descr = "change file permissions/attributes"}},
	{WK_c WK_w,        {{&cmd_cw}, .descr = "rename files"}},
	{WK_D WK_D,        {{&cmd_DD}, .nim = 1, .descr = "remove files permanently"}},
	{WK_d WK_d,        {{&cmd_dd}, .nim = 1, .descr = "remove files"}},
	{WK_d WK_o,        {{&cmd_do}, .descr = "obtain files"}},
	{WK_d WK_p,        {{&cmd_dp}, .descr = "put files"}},
	{WK_D,             {{&cmd_D_selector}, FOLLOWED_BY_SELECTOR, .descr = "remove files permanently"}},
	{WK_d,             {{&cmd_d_selector}, FOLLOWED_BY_SELECTOR, .descr = "remove files"}},
	{WK_e,             {{&cmd_e}, .descr = "explore file contents"}},
	{WK_f,             {{&cmd_f}, FOLLOWED_BY_MULTIKEY, .descr = "char-search forward"}},
	{WK_g WK_A,        {{&cmd_gA}, .descr = "(re)calculate size"}},
	{WK_g WK_a,        {{&cmd_ga}, .descr = "calculate size"}},
	{WK_g WK_f,        {{&cmd_gf}, .descr = "navigate to link target"}},
	{WK_g WK_F,        {{&cmd_gF}, .descr = "navigate to ultimate link target"}},
	{WK_g WK_g,        {{&cmd_gg}, .descr = "go to the first item"}},
	{WK_g WK_h,        {{&cmd_gh}, .descr = "go to parent directory"}},
	{WK_g WK_j,        {{&cmd_j},  .descr = "go to item below"}},
	{WK_g WK_k,        {{&cmd_k},  .descr = "go to item above"}},
	{WK_g WK_l,        {{&cmd_return}, .descr = "open current item(s)"}},
	{WK_g WK_s,        {{&cmd_gs}, .descr = "restore/make selection"}},
	{WK_g WK_t,        {{&cmd_gt}, .descr = "next or n-th tab"}},
	{WK_g WK_T,        {{&cmd_gT}, .descr = "n-th previous tab"}},
	{WK_g WK_U,        {{&cmd_gU}, FOLLOWED_BY_SELECTOR, .descr = "convert to uppercase"}},
	{WK_g WK_u,        {{&cmd_gu}, FOLLOWED_BY_SELECTOR, .descr = "convert to lowercase"}},
	{WK_g WK_U WK_U,      {{&cmd_gU}, .descr = "convert to uppercase"}},
	{WK_g WK_u WK_u,      {{&cmd_gu}, .descr = "convert to lowercase"}},
	{WK_g WK_U WK_g WK_U, {{&cmd_gU}, .descr = "convert to uppercase"}},
	{WK_g WK_u WK_g WK_u, {{&cmd_gu}, .descr = "convert to lowercase"}},
	{WK_g WK_U WK_g WK_g, {{&cmd_gUgg}, .skip_suggestion = 1, .descr = "convert to uppercase"}},
	{WK_g WK_u WK_g WK_g, {{&cmd_gugg}, .skip_suggestion = 1, .descr = "convert to lowercase"}},
	{WK_g WK_v,        {{&cmd_gv}, .descr = "restore visual mode"}},
	{WK_h,             {{&cmd_h},  .descr = "go to parent directory/item to the left"}},
	{WK_i,             {{&cmd_i},  .descr = "open file, but don't execute it"}},
	{WK_j,             {{&cmd_j},  .descr = "go to item below"}},
	{WK_k,             {{&cmd_k},  .descr = "go to item above"}},
	{WK_l,             {{&cmd_l},  .descr = "open file/go to item to the right"}},
	{WK_m,             {{&cmd_m}, FOLLOWED_BY_MULTIKEY, .descr = "set a mark", .suggest = &sug_cmd_quote}},
	{WK_n,             {{&cmd_n},  .descr = "go to next search match"}},
	{WK_p,             {{&cmd_p},  .descr = "put files by copying them"}},
	{WK_r WK_l,        {{&cmd_rl}, .descr = "put files creating relative symlinks"}},
	{WK_q WK_COLON,    {{&cmd_q_colon},    .descr = "edit cmdline in editor"}},
	{WK_q WK_SLASH,    {{&cmd_q_slash},    .descr = "edit forward search pattern in editor"}},
	{WK_q WK_QM,       {{&cmd_q_question}, .descr = "edit backward search pattern in editor"}},
	{WK_q WK_EQUALS,   {{&cmd_q_equals},   .descr = "edit local filter pattern in editor"}},
	{WK_t,             {{&cmd_t},            .descr = "select current file"}},
	{WK_u,             {{&cmd_u},            .descr = "undo file operation"}},
	{WK_y WK_y,        {{&cmd_yy}, .nim = 1, .descr = "yank files"}},
	{WK_y,             {{&cmd_y_selector}, FOLLOWED_BY_SELECTOR, .descr = "yank files"}},
	{WK_v,             {{&cmd_V},  .descr = "go to visual mode"}},
	{WK_z WK_M,        {{&cmd_zM}, .descr = "restore all filters"}},
	{WK_z WK_O,        {{&cmd_zO}, .descr = "reset permanent filter"}},
	{WK_z WK_R,        {{&cmd_zR}, .descr = "save and reset all filters"}},
	{WK_z WK_a,        {{&cmd_za}, .descr = "toggle dot files visibility"}},
	{WK_z WK_d,        {{&cmd_zd}, .descr = "exclude custom view entry"}},
	{WK_z WK_b,        {{&modnorm_zb}, .descr = "push cursor to the bottom"}},
	{WK_z WK_f,        {{&cmd_zf}, .descr = "add current file to filter"}},
	{WK_z WK_m,        {{&cmd_zm}, .descr = "hide dot files"}},
	{WK_z WK_o,        {{&cmd_zo}, .descr = "show dot files"}},
	{WK_z WK_r,        {{&cmd_zr}, .descr = "clear local filter"}},
	{WK_z WK_x,        {{&cmd_zx}, .descr = "toggle directory fold"}},
	{WK_z WK_t,        {{&modnorm_zt},      .descr = "push cursor to the top"}},
	{WK_z WK_z,        {{&modnorm_zz},      .descr = "center cursor position"}},
	{WK_LP,            {{&cmd_left_paren},  .descr = "go to previous group of files"}},
	{WK_RP,            {{&cmd_right_paren}, .descr = "go to next group of files"}},
	{WK_z WK_k,        {{&cmd_z_k},  .descr = "go to previous sibling dir"}},
	{WK_z WK_j,        {{&cmd_z_j},  .descr = "go to next sibling dir"}},
	{WK_LB WK_c,       {{&cmd_lb_c}, .descr = "go to previous compare change"}},
	{WK_RB WK_c,       {{&cmd_rb_c}, .descr = "go to next compare change"}},
	{WK_LB WK_d,       {{&cmd_lb_d}, .descr = "go to previous dir"}},
	{WK_RB WK_d,       {{&cmd_rb_d}, .descr = "go to next dir"}},
	{WK_LB WK_r,       {{&cmd_lb_r}, .descr = "navigate to previous sibling dir"}},
	{WK_RB WK_r,       {{&cmd_rb_r}, .descr = "navigate to next sibling dir"}},
	{WK_LB WK_R,       {{&cmd_lb_R}, .descr = "navigate to previous sibling dir (wrap)"}},
	{WK_RB WK_R,       {{&cmd_rb_R}, .descr = "navigate to next sibling dir (wrap)"}},
	{WK_LB WK_s,       {{&cmd_lb_s}, .descr = "go to previous selected entry"}},
	{WK_RB WK_s,       {{&cmd_rb_s}, .descr = "go to next selected entry"}},
	{WK_LB WK_z,       {{&cmd_lb_z}, .descr = "go to first sibling"}},
	{WK_RB WK_z,       {{&cmd_rb_z}, .descr = "go to last sibling"}},
	{WK_LCB,           {{&cmd_left_curly_bracket},  .descr = "go to previous file/dir group"}},
	{WK_RCB,           {{&cmd_right_curly_bracket}, .descr = "go to next file/dir group"}},
#ifndef _WIN32
	{WK_c WK_g,        {{&cmd_cg}, .descr = "change group"}},
	{WK_c WK_o,        {{&cmd_co}, .descr = "change owner"}},
#else
	{WK_g WK_r,        {{&cmd_gr}, .descr = "open file with rights elevation"}},
#endif
#ifdef ENABLE_EXTENDED_KEYS
	{{WC_C_w, K(KEY_BACKSPACE)}, {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{{K(KEY_PPAGE)},             {{&cmd_ctrl_b},  .descr = "scroll page up"}},
	{{K(KEY_NPAGE)},             {{&cmd_ctrl_f},  .descr = "scroll page down"}},
	{{K(KEY_LEFT)},              {{&cmd_h},       .descr = "go to parent directory/item to the left"}},
	{{K(KEY_DOWN)},              {{&cmd_j},       .descr = "go to item below"}},
	{{K(KEY_UP)},                {{&cmd_k},       .descr = "go to item above"}},
	{{K(KEY_RIGHT)},             {{&cmd_l},       .descr = "open file/go to item to the right"}},
	{{K(KEY_HOME)},              {{&cmd_gg},      .descr = "go to the first item"}},
	{{K(KEY_END)},               {{&cmd_G},       .descr = "go to the last item"}},
	{{K(KEY_BTAB)},              {{&cmd_shift_tab}, .descr = "switch to view pane"}},
#else
	{WK_ESC L"[Z",               {{&cmd_shift_tab}, .descr = "switch to view pane"}},
#endif /* ENABLE_EXTENDED_KEYS */
	{{K(KEY_MOUSE)}, {{&handle_mouse_event}, FOLLOWED_BY_NONE}},
};

static keys_add_info_t selectors[] = {
	{WK_QUOTE,   {{&cmd_quote}, FOLLOWED_BY_MULTIKEY, .descr = "to mark", .suggest = &sug_sel_quote}},
	{WK_PERCENT, {{&cmd_percent},   .descr = "to [count]% position"}},
	{WK_CARET,   {{&cmd_zero},      .descr = "to first column"}},
	{WK_DOLLAR,  {{&cmd_dollar},    .descr = "to last column"}},
	{WK_COMMA,   {{&cmd_comma},     .descr = "to previous char-search match"}},
	{WK_ZERO,    {{&cmd_zero},      .descr = "to first column"}},
	{WK_SCOLON,  {{&cmd_semicolon}, .descr = "to next char-search match"}},
	{WK_C_n,     {{&cmd_j},         .descr = "to item below"}},
	{WK_C_p,     {{&cmd_k},         .descr = "to item above"}},
	{WK_F,       {{&cmd_F}, FOLLOWED_BY_MULTIKEY, .descr = "to char-search match forward"}},
	{WK_G,       {{&cmd_G},         .descr = "to the last item"}},
	{WK_H,       {{&cmd_H},         .descr = "to top of viewport"}},
	{WK_L,       {{&cmd_L},         .descr = "to bottom of viewport"}},
	{WK_M,       {{&cmd_M},         .descr = "to middle of viewport"}},
	{WK_S,       {{&selector_S},    .descr = "non-selected files"}},
	{WK_a,       {{&selector_a},    .descr = "all files"}},
	{WK_f,       {{&cmd_f}, FOLLOWED_BY_MULTIKEY, .descr = "to char-search match backward"}},
	{WK_g WK_g,  {{&cmd_gg}, .descr = "to the first item"}},
	{WK_h,       {{&cmd_h},  .descr = "to item to the left"}},
	{WK_j,       {{&cmd_j},  .descr = "to item below"}},
	{WK_k,       {{&cmd_k},  .descr = "to item above"}},
	{WK_l,       {{&cmd_l},  .descr = "to item to the right"}},
	{WK_s,       {{&selector_s},      .descr = "selected files"}},
	{WK_LP,      {{&cmd_left_paren},  .descr = "to previous group of files"}},
	{WK_RP,      {{&cmd_right_paren}, .descr = "to next group of files"}},
	{WK_z WK_k,  {{&cmd_z_k},  .descr = "to previous sibling dir"}},
	{WK_z WK_j,  {{&cmd_z_j},  .descr = "to next sibling dir"}},
	{WK_LB WK_c, {{&cmd_lb_c}, .descr = "to previous compare change"}},
	{WK_RB WK_c, {{&cmd_rb_c}, .descr = "to next compare change"}},
	{WK_LB WK_d, {{&cmd_lb_d}, .descr = "to previous dir"}},
	{WK_RB WK_d, {{&cmd_rb_d}, .descr = "to next dir"}},
	{WK_LB WK_s, {{&cmd_lb_s}, .descr = "to previous selected entry"}},
	{WK_RB WK_s, {{&cmd_rb_s}, .descr = "to next selected entry"}},
	{WK_LB WK_z, {{&cmd_lb_z}, .descr = "to first sibling"}},
	{WK_RB WK_z, {{&cmd_rb_z}, .descr = "to last sibling"}},
	{WK_LCB,     {{&cmd_left_curly_bracket},  .descr = "to previous file/dir group"}},
	{WK_RCB,     {{&cmd_right_curly_bracket}, .descr = "to next file/dir group"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_DOWN)}, {{&cmd_j},  .descr = "to item below"}},
	{{K(KEY_UP)},   {{&cmd_k},  .descr = "to item above"}},
	{{K(KEY_HOME)}, {{&cmd_gg}, .descr = "to the first item"}},
	{{K(KEY_END)},  {{&cmd_G},  .descr = "to the last item"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
modnorm_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), NORMAL_MODE);
	assert(ret_code == 0);

	ret_code = vle_keys_add_selectors(selectors, ARRAY_LEN(selectors),
			NORMAL_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

/* Increments first number in names of marked files of the view [count=1]
 * times. */
static void
cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info)
{
	flist_set_marking(curr_view, 0);
	curr_stats.save_msg = fops_incdec(curr_view, def_count(key_info.count));
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	fview_scroll_page_up(curr_view);
}

/* Resets selection and search highlight. */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	ui_view_reset_search_highlight(curr_view);
	flist_sel_stash(curr_view);
	redraw_current_view();
}

/* Scroll view down by half of its height. */
static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_down(curr_view))
	{
		ui_view_schedule_redraw(curr_view);
		fpos_set_pos(curr_view, fpos_half_scroll(curr_view, 1));
	}
}

/* Scroll pane one line down. */
static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_fwd(curr_view) && fpos_scroll_down(curr_view, 1))
	{
		fview_scroll_fwd_by(curr_view, 1);
		redraw_current_view();
	}
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	fview_scroll_page_down(curr_view);
}

static void
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	modfinfo_enter(curr_view);
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
	char prefix[32];
	if(key_info.count != NO_COUNT_GIVEN && key_info.count != 1)
	{
		if(curr_view->list_pos + key_info.count >= curr_view->list_rows)
		{
			strcpy(prefix, ".,$!");
		}
		else
		{
			snprintf(prefix, ARRAY_LEN(prefix), ".,.+%d!", key_info.count - 1);
		}
	}
	else
	{
		strcpy(prefix, ".!");
	}

	cmds_vars_set_count(key_info.count);
	modcline_enter(CLS_COMMAND, prefix);
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
		flist_hist_go_forward(curr_view);
	}
}

/* Clear screen and redraw. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	update_screen(UT_FULL);
}

/* Enters directory or runs file. */
static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	rn_open(curr_view, FHE_RUN);
	flist_sel_stash(curr_view);
	redraw_current_view();
}

static void
cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info)
{
	flist_hist_go_back(curr_view);
}

static void
cmd_ctrl_r(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.confirmed = 0;
	ui_cancellation_push_off();

	ui_sb_msg("Redoing...");

	switch(un_group_redo())
	{
		case UN_ERR_SUCCESS:
			ui_views_reload_visible_filelists();
			ui_sb_msg("Redone one group");
			break;
		case UN_ERR_FAIL:
			ui_views_reload_visible_filelists();
			ui_sb_err("Redone one group with errors");
			break;
		case UN_ERR_NONE:
			ui_sb_msg("Nothing to redo");
			break;
		case UN_ERR_BROKEN:
			ui_sb_err("Can't redo group, it was skipped");
			break;
		case UN_ERR_BALANCE:
			ui_sb_err("Can't redo what wasn't undone");
			break;
		case UN_ERR_SKIPPED:
			ui_sb_msg("Group redo skipped by user");
			break;
		case UN_ERR_CANCELLED:
			ui_views_reload_visible_filelists();
			ui_sb_msg("Redoing was cancelled");
			break;
		case UN_ERR_ERRORS:
			ui_sb_err("Redo operation was skipped due to previous errors");
			break;
		case UN_ERR_NOUNDO:
			assert(0 && "un_group_redo() shouldn't return UN_ERR_NOUNDO!");
			break;
	}

	curr_stats.save_msg = 1;
	ui_cancellation_pop();
}

/* Scroll view up by half of its height. */
static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_up(curr_view))
	{
		ui_view_schedule_redraw(curr_view);
		fpos_set_pos(curr_view, fpos_half_scroll(curr_view, 0));
	}
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
modnorm_ctrl_wequal(key_info_t key_info, keys_info_t *keys_info)
{
	stats_set_splitter_pos(-1);
}

void
modnorm_ctrl_wless(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(def_count(key_info.count), is_left_or_top() ? -1 : +1);
}

void
modnorm_ctrl_wgreater(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(def_count(key_info.count), is_left_or_top() ? +1 : -1);
}

void
modnorm_ctrl_wplus(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(def_count(key_info.count), is_left_or_top() ? +1 : -1);
}

void
modnorm_ctrl_wminus(key_info_t key_info, keys_info_t *keys_info)
{
	move_splitter(def_count(key_info.count), is_left_or_top() ? -1 : +1);
}

void
modnorm_ctrl_wpipe(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		key_info.count = (curr_stats.split == HSPLIT)
		               ? getmaxy(stdscr)
		               : getmaxx(stdscr);
	}

	ui_view_resize(pick_view(), key_info.count);
}

/* Checks whether current view is left/top or right/bottom.  Returns non-zero
 * in first case and zero in the second one. */
static int
is_left_or_top(void)
{
	return (pick_view() == &lwin);
}

/* Picks view to operate on for Ctrl-W set of shortcuts.  Returns the view. */
static view_t *
pick_view(void)
{
	if(vle_mode_is(VIEW_MODE))
	{
		return curr_view->explore_mode ? curr_view : other_view;
	}

	return curr_view;
}

/* Switches views. */
static void
cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info)
{
	switch_panes();
	if(curr_stats.preview.on)
	{
		change_window();
		(void)try_switch_into_view_mode();
	}
}

/* Quits preview pane or view modes. */
static void
cmd_ctrl_wz(key_info_t key_info, keys_info_t *keys_info)
{
	qv_hide();
}

/* Decrements first number in names of marked files of the view [count=1]
 * times. */
static void
cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info)
{
	flist_set_marking(curr_view, 0);
	curr_stats.save_msg = fops_incdec(curr_view, -def_count(key_info.count));
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_back(curr_view) && fpos_scroll_up(curr_view, 1))
	{
		fview_scroll_back_by(curr_view, 1);
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
	if(curr_stats.preview.on && curr_stats.number_of_windows != 1)
	{
		modview_enter(other_view, 0);
		return 1;
	}
	return 0;
}

/* Clone selection.  Count specifies number of copies of each file or directory
 * to create (one by default). */
static void
cmd_C(key_info_t key_info, keys_info_t *keys_info)
{
	flist_set_marking(curr_view, 0);
	curr_stats.save_msg = fops_clone(curr_view, NULL, 0, 0,
			def_count(key_info.count));
}

/* Navigates to previous word which starts with specified character. */
static void
cmd_F(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;

	find_goto(key_info.multi, def_count(key_info.count), 1, keys_info);
}

/* Navigates to next/previous file which starts with given character. */
static void
find_goto(int ch, int count, int backward, keys_info_t *keys_info)
{
	int pos;
	int old_pos = curr_view->list_pos;

	pos = fpos_find_by_ch(curr_view, ch, backward, !keys_info->selector);
	if(pos < 0 || pos == curr_view->list_pos)
		return;
	while(--count > 0)
	{
		curr_view->list_pos = pos;
		pos = fpos_find_by_ch(curr_view, ch, backward, !keys_info->selector);
	}

	if(keys_info->selector)
	{
		curr_view->list_pos = old_pos;
		if(pos >= 0)
			pick_files(curr_view, pos, keys_info);
	}
	else
	{
		fpos_set_pos(curr_view, pos);
	}
}

/* Jump to bottom of the list or to specified line. */
static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		key_info.count = curr_view->list_rows;
	}
	pick_or_move(keys_info, key_info.count - 1);
}

/* Calculate size of selected directories ignoring cached sizes. */
static void
cmd_gA(key_info_t key_info, keys_info_t *keys_info)
{
	fops_size_bg(curr_view, 1);
}

/* Calculate size of selected directories taking cached sizes into account. */
static void
cmd_ga(key_info_t key_info, keys_info_t *keys_info)
{
	fops_size_bg(curr_view, 0);
}

/* Changes current directory to symlink's target (follows just one link). */
static void
cmd_gf(key_info_t key_info, keys_info_t *keys_info)
{
	flist_sel_stash(curr_view);
	rn_follow(curr_view, 0);
	redraw_current_view();
}

/* Changes current directory to the ultimate target (follows all links). */
static void
cmd_gF(key_info_t key_info, keys_info_t *keys_info)
{
	flist_sel_stash(curr_view);
	rn_follow(curr_view, 1);
	redraw_current_view();
}

/* Jump to the top of the list or to specified line. */
static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, def_count(key_info.count) - 1);
}

/* Goes to parent directory regardless of ls-like state. */
static void
cmd_gh(key_info_t key_info, keys_info_t *keys_info)
{
	rn_leave(curr_view, def_count(key_info.count));
}

#ifdef _WIN32
static void
cmd_gr(key_info_t key_info, keys_info_t *keys_info)
{
	rn_open(curr_view, FHE_ELEVATE_AND_RUN);
	flist_sel_stash(curr_view);
	redraw_current_view();
}
#endif

/* Restores previous selection of files or selects files listed in a
 * register. */
static void
cmd_gs(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
	{
		flist_sel_restore(curr_view, NULL);
		return;
	}

	regs_sync_from_shared_memory();
	const reg_t *reg = regs_find(tolower(key_info.reg));
	if(reg == NULL || reg->nfiles < 1)
	{
		ui_sb_err(reg == NULL ? "No such register" : "Register is empty");
		curr_stats.save_msg = 1;
		return;
	}

	flist_sel_restore(curr_view, reg);
}

/* Switches either to the next tab or to tab specified by its number [count]. */
static void
cmd_gt(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		tabs_next(1);
	}
	else
	{
		tabs_goto(key_info.count - 1);
	}
}

/* Switches to [count]-th previous tab ([count] is 1 by default). */
static void
cmd_gT(key_info_t key_info, keys_info_t *keys_info)
{
	tabs_previous(def_count(key_info.count));
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

	check_marking(curr_view, keys_info->count, keys_info->indexes);
	curr_stats.save_msg = fops_case(curr_view, upper);
	free_list_of_file_indexes(keys_info);
}

static void
cmd_gv(key_info_t key_info, keys_info_t *keys_info)
{
	modvis_enter(VS_RESTORE);
}

/* Go to the first file in window. */
static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_get_top_pos(curr_view));
}

/* Go to the last file in window. */
static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_get_bottom_pos(curr_view));
}

/* Go to middle line of window. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_get_middle_pos(curr_view));
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
		fpos_set_pos(curr_view, new_pos);
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
	call_put_files(key_info, 1);
}

/* Visual selection of files. */
static void
cmd_V(key_info_t key_info, keys_info_t *keys_info)
{
	modvis_enter(VS_NORMAL);
}

/* Possibly exits the application without saving vifminfo file or closes a
 * tab. */
static void
cmd_ZQ(key_info_t key_info, keys_info_t *keys_info)
{
	ui_quit(0, 0);
}

/* Possibly exits the application saving vifminfo file or closes a tab. */
static void
cmd_ZZ(key_info_t key_info, keys_info_t *keys_info)
{
	ui_quit(1, 0);
}

/* Goto mark. */
static void
cmd_quote(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->selector)
	{
		const int pos = marks_find_in_view(curr_view, key_info.multi);
		if(pos >= 0)
		{
			pick_files(curr_view, pos, keys_info);
		}
	}
	else
	{
		curr_stats.save_msg = marks_goto(curr_view, key_info.multi);
		if(!cfg.auto_ch_pos)
		{
			fpos_set_pos(curr_view, 0);
		}
	}
}

/* Suggests marks located anywhere. */
static void
sug_cmd_quote(vle_keys_list_cb cb)
{
	if(cfg.sug.flags & SF_MARKS)
	{
		marks_suggest(curr_view, cb, 0);
	}
}

/* Suggests only marks that point into current view. */
static void
sug_sel_quote(vle_keys_list_cb cb)
{
	if(cfg.sug.flags & SF_MARKS)
	{
		marks_suggest(curr_view, cb, 1);
	}
}

/* Move cursor to the last column in ls-view sub-mode. */
static void
cmd_dollar(key_info_t key_info, keys_info_t *keys_info)
{
	if(!fpos_at_last_col(curr_view) || keys_info->selector)
	{
		pick_or_move(keys_info, fpos_line_end(curr_view));
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
	modcline_enter(CLS_FILTER, curr_view->local_filter.filter.raw);
}

/* Continues navigation to word which starts with specified character in
 * opposite direction. */
static void
cmd_comma(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward != -1)
	{
		find_goto(last_fast_search_char, def_count(key_info.count),
				!last_fast_search_backward, keys_info);
	}
}

/* Repeat last change. */
static void
cmd_dot(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = cmds_dispatch(curr_stats.last_cmdline_command,
			curr_view, CIT_COMMAND);
}

/* Move cursor to the first column in ls-view sub-mode. */
static void
cmd_zero(key_info_t key_info, keys_info_t *keys_info)
{
	if(!fpos_at_first_col(curr_view) || keys_info->selector)
	{
		pick_or_move(keys_info, fpos_line_start(curr_view));
	}
}

/* Switch to command-line mode. */
static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	char prefix[16];
	prefix[0] = '\0';
	if(key_info.count != NO_COUNT_GIVEN)
	{
		snprintf(prefix, ARRAY_LEN(prefix), ".,.+%d", key_info.count - 1);
	}

	cmds_vars_set_count(key_info.count);
	modcline_enter(CLS_COMMAND, prefix);
}

/* Continues navigation to word which starts with specified character in initial
 * direction. */
static void
cmd_semicolon(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward != -1)
	{
		find_goto(last_fast_search_char, def_count(key_info.count),
				last_fast_search_backward, keys_info);
	}
}

/* Search forward. */
static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 0, 0);
}

/* Search backward. */
static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 1, 0);
}

/* Creates link with absolute path. */
static void
cmd_al(key_info_t key_info, keys_info_t *keys_info)
{
	call_put_links(key_info, 0);
}

/* Enters selection amending submode of visual mode. */
static void
cmd_av(key_info_t key_info, keys_info_t *keys_info)
{
	modvis_enter(VS_AMEND);
}

/* Change word (rename file without extension). */
static void
cmd_cW(key_info_t key_info, keys_info_t *keys_info)
{
	fops_rename_current(curr_view, 1);
}

#ifndef _WIN32
/* Change group. */
static void
cmd_cg(key_info_t key_info, keys_info_t *keys_info)
{
	fops_chgroup();
}
#endif

/* Change symbolic link(s). */
static void
cmd_cl(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->selected_files > 1)
	{
		flist_set_marking(curr_view, 0);
	}

	curr_stats.save_msg = fops_retarget(curr_view);
}

#ifndef _WIN32
/* Change owner. */
static void
cmd_co(key_info_t key_info, keys_info_t *keys_info)
{
	fops_chuser();
}
#endif

/* Change file attributes (permissions or properties). */
static void
cmd_cp(key_info_t key_info, keys_info_t *keys_info)
{
	modnorm_cp(curr_view, key_info);
}

void
modnorm_cp(view_t *view, key_info_t key_info)
{
#ifndef _WIN32
	if(key_info.count == NO_COUNT_GIVEN)
	{
		enter_attr_mode(view);
		return;
	}

	char mode[32];
	int len = snprintf(mode, sizeof(mode), "%04d", key_info.count);
	if(len != 4 || ((int)strspn(mode, "01234567") != len))
	{
		/* Update view because there might be not reflected changes of cursor
		 * position (when called from visual mode). */
		redraw_view(view);

		ui_sb_errf("Incorrect file mode: %s", mode);
		curr_stats.save_msg = 1;
		return;
	}

	flist_set_marking(view, 0);
	files_chmod(view, mode, 0);
#else
	enter_attr_mode(view);
#endif
}

/* Change word (rename file or files). */
static void
cmd_cw(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_view->selected_files > 1)
	{
		flist_set_marking(curr_view, 0);
		fops_rename(curr_view, NULL, 0, 0);
		return;
	}

	fops_rename_current(curr_view, 0);
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

/* Performs file deletion either by moving them to trash or removing
 * permanently. */
static void
delete(key_info_t key_info, int use_trash)
{
	keys_info_t keys_info = {};

	if(!fops_view_can_be_changed(curr_view))
	{
		return;
	}

	if(key_info.count != NO_COUNT_GIVEN)
	{
		const int end_pos = calc_pick_files_end_pos(curr_view, key_info.count);
		pick_files(curr_view, end_pos, &keys_info);
	}

	if(!cfg.selection_is_primary && key_info.count == NO_COUNT_GIVEN)
	{
		pick_files(curr_view, curr_view->list_pos, &keys_info);
	}

	call_delete(key_info, &keys_info, use_trash);
}

/* Permanently removes files defined by selector. */
static void
cmd_D_selector(key_info_t key_info, keys_info_t *keys_info)
{
	if(fops_view_can_be_changed(curr_view))
	{
		call_delete(key_info, keys_info, 0);
	}
}

static void
cmd_d_selector(key_info_t key_info, keys_info_t *keys_info)
{
	call_delete(key_info, keys_info, 1);
}

/* Invokes actual file deletion procedure. */
static void
call_delete(key_info_t key_info, keys_info_t *keys_info, int use_trash)
{
	check_marking(curr_view, keys_info->count, keys_info->indexes);
	curr_stats.save_msg = (fops_delete(curr_view, def_reg(key_info.reg),
			use_trash) != 0);
	free_list_of_file_indexes(keys_info);
}

/* Applies change from other compare to current one. */
static void
cmd_do(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (compare_move(other_view, curr_view) != 0);
}

/* Applies change from current compare to other one. */
static void
cmd_dp(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (compare_move(curr_view, other_view) != 0);
}

static void
cmd_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.preview.on)
	{
		ui_sb_err("Another type of file viewing is activated");
		curr_stats.save_msg = 1;
		return;
	}
	modview_enter(curr_view, 1);
}

/* Navigates to next word which starts with specified character. */
static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;

	find_goto(key_info.multi, def_count(key_info.count), 0, keys_info);
}

/* Updir or one file left in less-like sub-mode. */
static void
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	const dir_entry_t *entry = get_current_entry(curr_view);

	if(!ui_view_displays_columns(curr_view))
	{
		if(fpos_can_move_left(curr_view))
		{
			go_to_prev(key_info, keys_info, fpos_get_hor_step(curr_view));
		}
		return;
	}

	if(entry->child_pos != 0)
	{
		key_info.count = def_count(key_info.count);
		while(key_info.count-- > 0)
		{
			entry -= entry->child_pos;
		}
		pick_or_move(keys_info, entry_to_pos(curr_view, entry));
	}
	else
	{
		cmd_gh(key_info, keys_info);
	}
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	rn_open(curr_view, FHE_NO_RUN);
	flist_sel_stash(curr_view);
	redraw_current_view();
}

/* Move down one line. */
static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_down(curr_view))
	{
		go_to_next(key_info, keys_info, fpos_get_ver_step(curr_view));
	}
}

/* Move up one line. */
static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_up(curr_view))
	{
		go_to_prev(key_info, keys_info, fpos_get_ver_step(curr_view));
	}
}

/* Moves cursor to one of previous files in the list. */
static void
go_to_prev(key_info_t key_info, keys_info_t *keys_info, int step)
{
	const int distance = def_count(key_info.count)*step;
	pick_or_move(keys_info, curr_view->list_pos - distance);
}

static void
cmd_l(key_info_t key_info, keys_info_t *keys_info)
{
	if(ui_view_displays_columns(curr_view))
	{
		cmd_return(key_info, keys_info);
	}
	else if(fpos_can_move_right(curr_view))
	{
		go_to_next(key_info, keys_info, fpos_get_hor_step(curr_view));
	}
}

/* Moves cursor to one of next files in the list. */
static void
go_to_next(key_info_t key_info, keys_info_t *keys_info, int step)
{
	const int distance = def_count(key_info.count)*step;
	pick_or_move(keys_info, curr_view->list_pos + distance);
}

/* Set mark. */
static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	const dir_entry_t *const curr = get_current_entry(curr_view);
	if(!fentry_is_fake(curr))
	{
		curr_stats.save_msg = marks_set_user(curr_view, key_info.multi,
				curr->origin, curr->name);
	}
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, curr_stats.last_search_backward);
}

static void
search(key_info_t key_info, int backward)
{
	curr_stats.save_msg = search_next(curr_view, backward,
			/*stash_selection=*/cfg.hl_search, /*select_matches=*/cfg.hl_search,
			def_count(key_info.count), &set_pos_in_curr_view);
}

/* Put files. */
static void
cmd_p(key_info_t key_info, keys_info_t *keys_info)
{
	call_put_files(key_info, 0);
}

/* Invokes file putting procedure. */
static void
call_put_files(key_info_t key_info, int move)
{
	curr_stats.save_msg = fops_put(curr_view, -1, def_reg(key_info.reg), move);
	ui_views_reload_filelists();
}

/* Creates link with absolute path. */
static void
cmd_rl(key_info_t key_info, keys_info_t *keys_info)
{
	call_put_links(key_info, 1);
}

/* Invokes links putting procedure. */
static void
call_put_links(key_info_t key_info, int relative)
{
	curr_stats.save_msg =
		fops_put_links(curr_view, def_reg(key_info.reg), relative);
	ui_views_reload_filelists();
}

/* Runs external editor to get command-line command and then executes it. */
static void
cmd_q_colon(key_info_t key_info, keys_info_t *keys_info)
{
	cmds_run_ext("", 0U, CIT_COMMAND);
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
	/* TODO: generalize with visual.c:activate_search(). */

	search_repeat = def_count(count);
	curr_stats.last_search_backward = back;
	if(external)
	{
		CmdInputType type = back ? CIT_BSEARCH_PATTERN : CIT_FSEARCH_PATTERN;
		cmds_run_ext("", 0U, type);
	}
	else
	{
		const CmdLineSubmode submode = back ? CLS_BSEARCH : CLS_FSEARCH;
		modcline_enter(submode, "");
	}
}

/* Runs external editor to get local filter pattern and then executes it. */
static void
cmd_q_equals(key_info_t key_info, keys_info_t *keys_info)
{
	cmds_run_ext("", 0U, CIT_FILTER_PATTERN);
}

/* Toggles selection of the current file. */
static void
cmd_t(key_info_t key_info, keys_info_t *keys_info)
{
	dir_entry_t *const curr = get_current_entry(curr_view);
	/* Special purpose kind of entries can't be selected. */
	if(!fentry_is_valid(curr))
	{
		return;
	}

	if(curr->selected == 0)
	{
		curr->selected = 1;
		++curr_view->selected_files;
	}
	else
	{
		curr->selected = 0;
		--curr_view->selected_files;
	}

	fview_cursor_redraw(curr_view);
}

/* Undo last command group. */
static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.confirmed = 0;
	ui_cancellation_push_off();

	ui_sb_msg("Undoing...");

	switch(un_group_undo())
	{
		case UN_ERR_SUCCESS:
			ui_views_reload_visible_filelists();
			ui_sb_msg("Undone one group");
			break;
		case UN_ERR_FAIL:
			ui_views_reload_visible_filelists();
			ui_sb_err("Undone one group with errors");
			break;
		case UN_ERR_NONE:
			ui_sb_msg("Nothing to undo");
			break;
		case UN_ERR_BROKEN:
			ui_sb_err("Can't undo group, it was skipped");
			break;
		case UN_ERR_BALANCE:
			ui_sb_err("Can't undo what wasn't redone");
			break;
		case UN_ERR_NOUNDO:
			ui_sb_err("Operation cannot be undone");
			break;
		case UN_ERR_SKIPPED:
			ui_sb_msg("Group undo skipped by user");
			break;
		case UN_ERR_CANCELLED:
			ui_views_reload_visible_filelists();
			ui_sb_msg("Undoing was cancelled");
			break;
		case UN_ERR_ERRORS:
			ui_sb_err("Undo operation was skipped due to previous errors");
			break;
	}

	curr_stats.save_msg = 1;
	ui_cancellation_pop();
}

/* Yanks files. */
static void
cmd_yy(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
	{
		const int end_pos = calc_pick_files_end_pos(curr_view, key_info.count);
		pick_files(curr_view, end_pos, keys_info);
	}
	else if(!cfg.selection_is_primary)
	{
		pick_files(curr_view, curr_view->list_pos, keys_info);
	}

	check_marking(curr_view, keys_info->count, keys_info->indexes);
	yank(key_info, keys_info);
}

/* Calculates end position for pick_files(...) function using cursor position
 * and count of a command.  Considers possible integer overflow. */
static int
calc_pick_files_end_pos(const view_t *view, int count)
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
	if(keys_info->count != 0)
	{
		mark_files_at(curr_view, keys_info->count, keys_info->indexes);
		yank(key_info, keys_info);
	}
}

static void
yank(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = fops_yank(curr_view, def_reg(key_info.reg));
	free_list_of_file_indexes(keys_info);

	flist_sel_stash(curr_view);
	ui_view_schedule_redraw(curr_view);
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
	name_filters_restore(curr_view);
	local_filter_restore(curr_view);
	dot_filter_set(curr_view, 0);
}

/* Remove filename filter. */
static void
cmd_zO(key_info_t key_info, keys_info_t *keys_info)
{
	name_filters_remove(curr_view);
}

/* Show all hidden files. */
static void
cmd_zR(key_info_t key_info, keys_info_t *keys_info)
{
	name_filters_remove(curr_view);
	local_filter_remove(curr_view);
	dot_filter_set(curr_view, 1);
}

/* Toggle dot files visibility. */
static void
cmd_za(key_info_t key_info, keys_info_t *keys_info)
{
	dot_filter_toggle(curr_view);
}

/* Excludes entries from custom view. */
static void
cmd_zd(key_info_t key_info, keys_info_t *keys_info)
{
	flist_custom_exclude(curr_view, key_info.count == 1);
}

void
modnorm_zb(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_back(curr_view))
	{
		const int bottom = fpos_get_bottom_pos(curr_view);
		fview_scroll_back_by(curr_view, bottom - curr_view->list_pos);
		redraw_current_view();
	}
}

/* Filters active files. */
static void
cmd_zf(key_info_t key_info, keys_info_t *keys_info)
{
	name_filters_add_active(curr_view);
}

/* Hides dot files. */
static void
cmd_zm(key_info_t key_info, keys_info_t *keys_info)
{
	dot_filter_set(curr_view, 0);
}

/* Shows all the dot files. */
static void
cmd_zo(key_info_t key_info, keys_info_t *keys_info)
{
	dot_filter_set(curr_view, 1);
}

/* Resets local filter. */
static void
cmd_zr(key_info_t key_info, keys_info_t *keys_info)
{
	local_filter_remove(curr_view);
}

/* Toggles fold under the cursor if any. */
static void
cmd_zx(key_info_t key_info, keys_info_t *keys_info)
{
	dir_entry_t *curr = get_current_entry(curr_view);
	if(curr->type != FT_DIR && curr->child_pos != 0)
	{
		fpos_set_pos(curr_view, curr_view->list_pos - curr->child_pos);
	}

	flist_toggle_fold(curr_view);
}

/* Moves cursor to the beginning of the previous group of files defined by the
 * primary sorting key. */
static void
cmd_left_paren(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_find_group(curr_view, 0));
}

/* Moves cursor to the beginning of the next group of files defined by the
 * primary sorting key. */
static void
cmd_right_paren(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_find_group(curr_view, 1));
}

/* Go to or pick files until and including previous sibling directory entry or
 * do nothing. */
static void
cmd_z_k(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_prev_dir_sibling(curr_view));
}

/* Go to or pick files until and including next sibling directory entry or do
 * nothing. */
static void
cmd_z_j(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_next_dir_sibling(curr_view));
}

/* Go to or pick files until and including previous mismatched entry or do
 * nothing. */
static void
cmd_lb_c(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_prev_mismatch(curr_view));
}

/* Go to or pick files until and including next mismatched entry or do
 * nothing. */
static void
cmd_rb_c(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_next_mismatch(curr_view));
}

/* Go to or pick files until and including previous directory entry or do
 * nothing. */
static void
cmd_lb_d(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_prev_dir(curr_view));
}

/* Go to or pick files until and including next directory entry or do
 * nothing. */
static void
cmd_rb_d(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_next_dir(curr_view));
}

/* Navigates to previous sibling directory. */
static void
cmd_lb_r(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (go_to_sibling_dir(curr_view,
				-def_count(key_info.count), 0) != 0);
}

/* Navigates to next sibling directory. */
static void
cmd_rb_r(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (go_to_sibling_dir(curr_view,
				def_count(key_info.count), 0) != 0);
}

/* Navigates to previous sibling directory with wrapping. */
static void
cmd_lb_R(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (go_to_sibling_dir(curr_view,
				-def_count(key_info.count), 1) != 0);
}

/* Navigates to next sibling directory with wrapping. */
static void
cmd_rb_R(key_info_t key_info, keys_info_t *keys_info)
{
	curr_stats.save_msg = (go_to_sibling_dir(curr_view,
				def_count(key_info.count), 1) != 0);
}

/* Go to or pick files until and including previous selected entry or do
 * nothing. */
static void
cmd_lb_s(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_prev_selected(curr_view));
}

/* Go to or pick files until and including next selected entry or do nothing. */
static void
cmd_rb_s(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_next_selected(curr_view));
}

/* Go to or pick files until and including first sibling of the current
 * entry. */
static void
cmd_lb_z(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_first_sibling(curr_view));
}

/* Go to or pick files until and including last sibling of the current entry. */
static void
cmd_rb_z(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_last_sibling(curr_view));
}

/* Moves cursor to the beginning of the previous group of files defined by them
 * being directory or files. */
static void
cmd_left_curly_bracket(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_find_dir_group(curr_view, 0));
}

/* Moves cursor to the beginning of the next group of files defined by them
 * being directory or files. */
static void
cmd_right_curly_bracket(key_info_t key_info, keys_info_t *keys_info)
{
	pick_or_move(keys_info, fpos_find_dir_group(curr_view, 1));
}

void
modnorm_zt(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_fwd(curr_view))
	{
		const int top = fpos_get_top_pos(curr_view);
		fview_scroll_fwd_by(curr_view, curr_view->list_pos - top);
		redraw_current_view();
	}
}

void
modnorm_zz(key_info_t key_info, keys_info_t *keys_info)
{
	if(!fpos_are_all_files_visible(curr_view))
	{
		const int middle = fpos_get_middle_pos(curr_view);
		fview_scroll_by(curr_view, curr_view->list_pos - middle);
		redraw_current_view();
	}
}

static void
pick_files(view_t *view, int end, keys_info_t *keys_info)
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
		fpos_set_pos(view, end);
	}
}

static void
selector_S(key_info_t key_info, keys_info_t *keys_info)
{
	int i, x;

	keys_info->count = curr_view->list_rows - curr_view->selected_files;
	keys_info->indexes = reallocarray(NULL, keys_info->count,
			sizeof(keys_info->indexes[0]));
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
	keys_info->indexes = reallocarray(NULL, keys_info->count,
			sizeof(keys_info->indexes[0]));
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

	keys_info->indexes = reallocarray(NULL, keys_info->count,
			sizeof(keys_info->indexes[0]));
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
modnorm_find(view_t *view, const char pattern[], int backward, int print_msg,
		int *found)
{
	return search_find(view, pattern, backward, /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search, search_repeat, &set_pos_in_curr_view,
			print_msg, found);
}

/* Moves cursor to pos, redraws cursor and schedules redraw of curr_view. */
static void
set_pos_in_curr_view(int pos)
{
	fpos_set_pos(curr_view, pos);
}

void
modnorm_set_search_attrs(int count, int last_search_backward)
{
	search_repeat = count;
	curr_stats.last_search_backward = last_search_backward;
}

/* Processes events from the mouse. */
static void
handle_mouse_event(key_info_t key_info, keys_info_t *keys_info)
{
	MEVENT e;
	if(ui_get_mouse(&e) != OK)
	{
		return;
	}

	int on_tab_line = !cfg.pane_tabs && wenclose(tab_line, e.y, e.x);

	if(ui_wenclose(&lwin, lwin.win, e.x, e.y) ||
			ui_wenclose(&lwin, lwin.title, e.x, e.y))
	{
		if(curr_view != &lwin)
		{
			go_to_other_window();
			return;
		}
	}
	else if(ui_wenclose(&rwin, rwin.win, e.x, e.y) ||
			ui_wenclose(&rwin, rwin.title, e.x, e.y))
	{
		if(curr_view != &rwin)
		{
			go_to_other_window();
			return;
		}
	}
	else if(!on_tab_line)
	{
		return;
	}

	if(on_tab_line || ui_wenclose(curr_view, curr_view->title, e.y, e.x))
	{
		if(!on_tab_line && !cfg.pane_tabs)
		{
			/* Do nothing. */
		}
		else if(e.bstate & BUTTON1_PRESSED)
		{
			wmouse_trafo(ui_get_tab_line_win(curr_view), &e.y, &e.x, FALSE);

			int tab_num = ui_map_tab_line(curr_view, e.x);
			if(tab_num >= 0)
			{
				tabs_goto(tab_num);
			}
		}
		else if(e.bstate & BUTTON4_PRESSED)
		{
			tabs_previous(1);
		}
		else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
		{
			tabs_next(1);
		}
	}
	else if(e.bstate & BUTTON1_PRESSED)
	{
		wmouse_trafo(curr_view->win, &e.y, &e.x, FALSE);

		/* Only handle clicks on non-blank lines. */
		int list_pos = fview_map_coordinates(curr_view, e.x, e.y);
		if(list_pos >= 0)
		{
			int old_pos = curr_view->list_pos;
			fpos_set_pos(curr_view, list_pos);

			if(curr_view->list_pos == old_pos)
			{
				cmd_return(key_info, keys_info);
			}
		}
		else if(list_pos == FVM_LEAVE)
		{
			cmd_gh(key_info, keys_info);
		}
		else if(list_pos == FVM_OPEN)
		{
			cmd_i(key_info, keys_info);
		}
	}
	else if(e.bstate & BUTTON3_PRESSED)
	{
		wmouse_trafo(curr_view->win, &e.y, &e.x, FALSE);

		/* Only handle clicks on non-blank lines. */
		int list_pos = fview_map_coordinates(curr_view, e.x, e.y);
		if(list_pos >= 0)
		{
			fpos_set_pos(curr_view, list_pos);
			curr_stats.save_msg = show_file_menu(curr_view, 0);
		}
	}
	else if(e.bstate & BUTTON4_PRESSED)
	{
		cmd_ctrl_y(key_info, keys_info);
	}
	else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
	{
		cmd_ctrl_e(key_info, keys_info);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
