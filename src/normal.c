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

#include <assert.h>
#include <string.h>

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
#include "menus.h"
#include "modes.h"
#include "permissions_dialog.h"
#include "registers.h"
#include "search.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

#include "normal.h"

static int *mode;
static int last_search_backward;
static int last_fast_search_char;
static int last_fast_search_backward = -1;

static void init_selectors(void);
static void init_extended_keys(void);
static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_d(struct key_info, struct keys_info *);
static void cmd_ctrl_e(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_ctrl_g(struct key_info, struct keys_info *);
static void cmd_space(struct key_info, struct keys_info *);
static void cmd_ctrl_i(struct key_info, struct keys_info *);
static void cmd_ctrl_l(struct key_info, struct keys_info *);
static void cmd_return(struct key_info, struct keys_info *);
static void cmd_ctrl_o(struct key_info, struct keys_info *);
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
static void find_F(int ch);
static void cmd_G(struct key_info, struct keys_info *);
static void selector_G(struct key_info, struct keys_info *);
static void cmd_H(struct key_info, struct keys_info *);
static void selector_H(struct key_info, struct keys_info *);
static void cmd_L(struct key_info, struct keys_info *);
static void selector_L(struct key_info, struct keys_info *);
static void cmd_M(struct key_info, struct keys_info *);
static void selector_M(struct key_info, struct keys_info *);
static void cmd_N(struct key_info, struct keys_info *);
static void cmd_P(struct key_info, struct keys_info *);
static void cmd_V(struct key_info, struct keys_info *);
static void cmd_ZQ(struct key_info, struct keys_info *);
static void cmd_ZZ(struct key_info, struct keys_info *);
static void selector_a(struct key_info, struct keys_info *);
static void cmd_cW(struct key_info, struct keys_info *);
static void cmd_cg(struct key_info, struct keys_info *);
static void cmd_co(struct key_info, struct keys_info *);
static void cmd_cp(struct key_info, struct keys_info *);
static void cmd_cw(struct key_info, struct keys_info *);
static void cmd_DD(struct key_info, struct keys_info *);
static void cmd_dd(struct key_info, struct keys_info *);
static void delete(struct key_info, int use_trash);
static void cmd_D_selector(struct key_info, struct keys_info *);
static void cmd_d_selector(struct key_info, struct keys_info *);
static void delete_with_selector(struct key_info, struct keys_info *,
		int use_trash);
static void selector_S(struct key_info, struct keys_info *);
static void cmd_f(struct key_info, struct keys_info *);
static void find_f(int ch);
static void cmd_ga(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void selector_gg(struct key_info, struct keys_info *);
static void cmd_gv(struct key_info, struct keys_info *);
static void cmd_h(struct key_info, struct keys_info *);
static void cmd_i(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void selector_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void selector_k(struct key_info, struct keys_info *);
static void cmd_l(struct key_info, struct keys_info *);
static void cmd_n(struct key_info, struct keys_info *);
static void cmd_p(struct key_info, struct keys_info *);
static void cmd_s(struct key_info, struct keys_info *);
static void cmd_m(struct key_info, struct keys_info *);
static void cmd_t(struct key_info, struct keys_info *);
static void cmd_yy(struct key_info, struct keys_info *);
static void cmd_y_selector(struct key_info, struct keys_info *);
static void cmd_zM(struct key_info, struct keys_info *);
static void cmd_zO(struct key_info, struct keys_info *);
static void cmd_zR(struct key_info, struct keys_info *);
static void cmd_za(struct key_info, struct keys_info *);
static void cmd_zb(struct key_info, struct keys_info *);
static void cmd_zf(struct key_info, struct keys_info *);
static void cmd_zm(struct key_info, struct keys_info *);
static void cmd_zo(struct key_info, struct keys_info *);
static void cmd_zt(struct key_info, struct keys_info *);
static void cmd_zz(struct key_info, struct keys_info *);
static void pick_files(FileView *view, int end, struct keys_info *keys_info);

void
init_normal_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_cmd(L"\x02", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_b;

	curr = add_cmd(L"\x03", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"\x04", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_d;

	curr = add_cmd(L"\x05", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_e;

	curr = add_cmd(L"\x06", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_f;

	curr = add_cmd(L"\x07", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_g;

	curr = add_cmd(L"\x09", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_i;

	curr = add_cmd(L"\x0c", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_l;

	curr = add_cmd(L"\x0d", NORMAL_MODE);
	curr->data.handler = cmd_return;

	curr = add_cmd(L"\x0f", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_o;

	curr = add_cmd(L"\x15", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_u;

	curr = add_cmd(L"\x17\x08", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wh;

	curr = add_cmd(L"\x17h", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wh;

	curr = add_cmd(L"\x17\x0f", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wo;

	curr = add_cmd(L"\x17o", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wo;

	curr = add_cmd(L"\x17\x0c", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wl;

	curr = add_cmd(L"\x17l", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wl;

	curr = add_cmd(L"\x17\x16", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wv;

	curr = add_cmd(L"\x17v", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wv;

	curr = add_cmd(L"\x17\x17", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_ww;

	curr = add_cmd(L"\x17w", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_ww;

	curr = add_cmd(L"\x17\x18", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wx;

	curr = add_cmd(L"\x17x", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_wx;

	curr = add_cmd(L"\x19", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_y;

	/* escape */
	curr = add_cmd(L"\x1b", NORMAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"'", NORMAL_MODE);
	curr->type = BUILDIN_WAIT_POINT;
	curr->data.handler = cmd_quote;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L" ", NORMAL_MODE);
	curr->data.handler = cmd_space;

	curr = add_cmd(L"%", NORMAL_MODE);
	curr->data.handler = cmd_percent;

	curr = add_cmd(L",", NORMAL_MODE);
	curr->data.handler = cmd_comma;

	curr = add_cmd(L".", NORMAL_MODE);
	curr->data.handler = cmd_dot;

	curr = add_cmd(L":", NORMAL_MODE);
	curr->data.handler = cmd_colon;

	curr = add_cmd(L";", NORMAL_MODE);
	curr->data.handler = cmd_semicolon;

	curr = add_cmd(L"/", NORMAL_MODE);
	curr->data.handler = cmd_slash;

	curr = add_cmd(L"?", NORMAL_MODE);
	curr->data.handler = cmd_question;

	curr = add_cmd(L"C", NORMAL_MODE);
	curr->data.handler = cmd_C;

	curr = add_cmd(L"F", NORMAL_MODE);
	curr->type = BUILDIN_WAIT_POINT;
	curr->data.handler = cmd_F;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L"G", NORMAL_MODE);
	curr->data.handler = cmd_G;

	curr = add_cmd(L"H", NORMAL_MODE);
	curr->data.handler = cmd_H;

	curr = add_cmd(L"L", NORMAL_MODE);
	curr->data.handler = cmd_L;

	curr = add_cmd(L"M", NORMAL_MODE);
	curr->data.handler = cmd_M;

	curr = add_cmd(L"N", NORMAL_MODE);
	curr->data.handler = cmd_N;

	curr = add_cmd(L"P", NORMAL_MODE);
	curr->data.handler = cmd_P;

	curr = add_cmd(L"V", NORMAL_MODE);
	curr->data.handler = cmd_V;

	curr = add_cmd(L"Y", NORMAL_MODE);
	curr->data.handler = cmd_yy;

	curr = add_cmd(L"ZQ", NORMAL_MODE);
	curr->data.handler = cmd_ZQ;

	curr = add_cmd(L"ZZ", NORMAL_MODE);
	curr->data.handler = cmd_ZZ;

	curr = add_cmd(L"cW", NORMAL_MODE);
	curr->data.handler = cmd_cW;

	curr = add_cmd(L"cg", NORMAL_MODE);
	curr->data.handler = cmd_cg;

	curr = add_cmd(L"co", NORMAL_MODE);
	curr->data.handler = cmd_co;

	curr = add_cmd(L"cp", NORMAL_MODE);
	curr->data.handler = cmd_cp;

	curr = add_cmd(L"cw", NORMAL_MODE);
	curr->data.handler = cmd_cw;

	curr = add_cmd(L"DD", NORMAL_MODE);
	curr->data.handler = cmd_DD;

	curr = add_cmd(L"dd", NORMAL_MODE);
	curr->data.handler = cmd_dd;

	curr = add_cmd(L"D", NORMAL_MODE);
	curr->data.handler = cmd_D_selector;
	curr->type = BUILDIN_WAIT_POINT;
	curr->followed = FOLLOWED_BY_SELECTOR;

	curr = add_cmd(L"d", NORMAL_MODE);
	curr->data.handler = cmd_d_selector;
	curr->type = BUILDIN_WAIT_POINT;
	curr->followed = FOLLOWED_BY_SELECTOR;

	curr = add_cmd(L"f", NORMAL_MODE);
	curr->type = BUILDIN_WAIT_POINT;
	curr->data.handler = cmd_f;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L"ga", NORMAL_MODE);
	curr->data.handler = cmd_ga;

	curr = add_cmd(L"gg", NORMAL_MODE);
	curr->data.handler = cmd_gg;

	curr = add_cmd(L"gv", NORMAL_MODE);
	curr->data.handler = cmd_gv;

	curr = add_cmd(L"h", NORMAL_MODE);
	curr->data.handler = cmd_h;

	curr = add_cmd(L"i", NORMAL_MODE);
	curr->data.handler = cmd_i;

	curr = add_cmd(L"j", NORMAL_MODE);
	curr->data.handler = cmd_j;

	curr = add_cmd(L"k", NORMAL_MODE);
	curr->data.handler = cmd_k;

	curr = add_cmd(L"l", NORMAL_MODE);
	curr->data.handler = cmd_l;

	curr = add_cmd(L"m", NORMAL_MODE);
	curr->type = BUILDIN_WAIT_POINT;
	curr->data.handler = cmd_m;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L"n", NORMAL_MODE);
	curr->data.handler = cmd_n;

	curr = add_cmd(L"p", NORMAL_MODE);
	curr->data.handler = cmd_p;

	curr = add_cmd(L"t", NORMAL_MODE);
	curr->data.handler = cmd_t;

	curr = add_cmd(L"yy", NORMAL_MODE);
	curr->data.handler = cmd_yy;

	curr = add_cmd(L"y", NORMAL_MODE);
	curr->data.handler = cmd_y_selector;
	curr->type = BUILDIN_WAIT_POINT;
	curr->followed = FOLLOWED_BY_SELECTOR;

	curr = add_cmd(L"v", NORMAL_MODE);
	curr->data.handler = cmd_V;

	curr = add_cmd(L"zM", NORMAL_MODE);
	curr->data.handler = cmd_zM;

	curr = add_cmd(L"zO", NORMAL_MODE);
	curr->data.handler = cmd_zO;

	curr = add_cmd(L"zR", NORMAL_MODE);
	curr->data.handler = cmd_zR;

	curr = add_cmd(L"za", NORMAL_MODE);
	curr->data.handler = cmd_za;

	curr = add_cmd(L"zb", NORMAL_MODE);
	curr->data.handler = cmd_zb;

	curr = add_cmd(L"zf", NORMAL_MODE);
	curr->data.handler = cmd_zf;

	curr = add_cmd(L"zm", NORMAL_MODE);
	curr->data.handler = cmd_zm;

	curr = add_cmd(L"zo", NORMAL_MODE);
	curr->data.handler = cmd_zo;

	curr = add_cmd(L"zt", NORMAL_MODE);
	curr->data.handler = cmd_zt;

	curr = add_cmd(L"zz", NORMAL_MODE);
	curr->data.handler = cmd_zz;

	init_selectors();
	init_extended_keys();
}

static void
init_selectors(void)
{
	struct key_t *curr;

	curr = add_selector(L"G", NORMAL_MODE);
	curr->data.handler = selector_G;

	curr = add_selector(L"H", NORMAL_MODE);
	curr->data.handler = selector_H;

	curr = add_selector(L"L", NORMAL_MODE);
	curr->data.handler = selector_L;

	curr = add_selector(L"M", NORMAL_MODE);
	curr->data.handler = selector_M;

	curr = add_selector(L"a", NORMAL_MODE);
	curr->data.handler = selector_a;

	curr = add_selector(L"S", NORMAL_MODE);
	curr->data.handler = selector_S;

	curr = add_selector(L"gg", NORMAL_MODE);
	curr->data.handler = selector_gg;

	curr = add_selector(L"j", NORMAL_MODE);
	curr->data.handler = selector_j;

	curr = add_selector(L"k", NORMAL_MODE);
	curr->data.handler = selector_k;

	curr = add_selector(L"s", NORMAL_MODE);
	curr->data.handler = cmd_s;
}

static void
init_extended_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_PPAGE;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_ctrl_b;

	buf[0] = KEY_NPAGE;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_ctrl_f;

	buf[0] = KEY_LEFT;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_h;

	buf[0] = KEY_DOWN;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_j;

	buf[0] = KEY_UP;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_k;

	buf[0] = KEY_RIGHT;
	curr = add_cmd(buf, NORMAL_MODE);
	curr->data.handler = cmd_l;
#endif /* ENABLE_EXTENDED_KEYS */
}

static void
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	curr_view->list_pos -= curr_view->window_rows;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line, curr_view->list_pos);
	moveto_list_pos(curr_view, curr_view->list_pos);
	curs_set(0);
}

static void
cmd_ctrl_d(struct key_info key_info, struct keys_info *keys_info)
{
	curr_view->list_pos += curr_view->window_rows/2;
	moveto_list_pos(curr_view, curr_view->list_pos);
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
	curr_view->list_pos += curr_view->window_rows;
	moveto_list_pos(curr_view, curr_view->list_pos);
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
cmd_ctrl_i(struct key_info key_info, struct keys_info *keys_info)
{
#ifdef ENABLE_COMPATIBILITY_MODE
	change_window();
#else /* ENABLE_COMPATIBILITY_MODE */
	if(curr_view->history_pos == curr_view->history_num - 1)
		return;

	goto_history_pos(curr_view, curr_view->history_pos + 1);
#endif /* ENABLE_COMPATIBILITY_MODE */
}

/* Clear screen and redraw. */
static void
cmd_ctrl_l(struct key_info key_info, struct keys_info *keys_info)
{
	redraw_window();
	curs_set(0);
}

static void
cmd_return(struct key_info key_info, struct keys_info *keys_info)
{
	handle_file(curr_view, 0);
}

static void
cmd_ctrl_o(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->history_pos == 0)
		return;

	goto_history_pos(curr_view, curr_view->history_pos - 1);
}

static void
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	curr_view->list_pos -= curr_view->window_rows/2;
	moveto_list_pos(curr_view, curr_view->list_pos);
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
	moveto_list_pos(curr_view, curr_view->list_pos);

	if(curr_stats.view)
		quick_view_file(curr_view);
	else
		load_dir_list(other_view, 1);
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

static void
selector_S(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = curr_view->list_rows - curr_view->selected_files;
	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
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

/* Clone file. */
static void
cmd_C(struct key_info key_info, struct keys_info *keys_info)
{
	clone_file(curr_view);
}

static void
cmd_F(struct key_info key_info, struct keys_info *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;
	find_F(key_info.multi);
}

static void
find_F(int ch)
{
	int x;

	x = curr_view->list_pos;
	do
	{
		x--;
		if(x == 0)
			x = curr_view->list_rows - 1;
		if(curr_view->dir_entry[x].name[0] == ch)
			break;
	}while(x != curr_view->list_pos);

	if(x == curr_view->list_pos)
		return;

	moveto_list_pos(curr_view, x);
}

/* Jump to bottom of list or to specified line. */
static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;

	moveto_list_pos(curr_view, key_info.count - 1);
}

static void
selector_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;

	pick_files(curr_view, key_info.count - 1, keys_info);
}

static void
cmd_ga(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].type != DIRECTORY)
		return;

	calc_dirsize(curr_view->dir_entry[curr_view->list_pos].name);

	moveto_list_pos(curr_view, curr_view->list_pos);
}

/* Jump to top of the list or to specified line. */
static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	moveto_list_pos(curr_view, key_info.count - 1);
}

static void
selector_gg(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	pick_files(curr_view, key_info.count - 1, keys_info);
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
	curr_view->list_pos = curr_view->top_line;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
selector_H(struct key_info key_info, struct keys_info *keys_info)
{
	pick_files(curr_view, curr_view->top_line, keys_info);
}

/* Go to last file in window. */
static void
cmd_L(struct key_info key_info, struct keys_info *keys_info)
{
	curr_view->list_pos = curr_view->top_line + curr_view->window_rows;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
selector_L(struct key_info key_info, struct keys_info *keys_info)
{
	pick_files(curr_view, curr_view->top_line + curr_view->window_rows,
			keys_info);
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

	curr_view->list_pos = new_pos;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
selector_M(struct key_info key_info, struct keys_info *keys_info)
{
	int new_pos;
	if(curr_view->list_rows < curr_view->window_rows)
		new_pos = curr_view->list_rows/2;
	else
		new_pos = curr_view->top_line + (curr_view->window_rows/2);

	pick_files(curr_view, new_pos, keys_info);
}

static void
cmd_N(struct key_info key_info, struct keys_info *keys_info)
{
	if(cfg.search_history_num < 0)
		return;
	if(curr_view->selected_files == 0)
		find_pattern(curr_view, cfg.search_history[0], last_search_backward);

	if(last_search_backward)
		find_next_pattern(curr_view);
	else
		find_previous_pattern(curr_view);
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
	curr_stats.setting_change = 0;
	comm_quit();
}

static void
cmd_ZZ(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.setting_change = 1;
	comm_quit();
}

/* Mark. */
static void
cmd_quote(struct key_info key_info, struct keys_info *keys_info)
{
	curr_stats.save_msg = get_bookmark(curr_view, key_info.multi);
}

/* Jump to percent of file. */
static void
cmd_percent(struct key_info key_info, struct keys_info *keys_info)
{
	int line = (key_info.count * (curr_view->list_rows)/100);
	moveto_list_pos(curr_view, line - 1);
}

static void
cmd_comma(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;
	if(last_fast_search_backward)
		find_f(last_fast_search_char);
	else
		find_F(last_fast_search_char);
}

/* Repeat last change. */
static void
cmd_dot(struct key_info key_info, struct keys_info *keys_info)
{
	if (0 > cfg.cmd_history_num)
		show_error_msg(" Command Error ", "Command history list is empty.");
	else
		execute_command(curr_view, cfg.cmd_history[0]);
}

/* Command. */
static void
cmd_colon(struct key_info key_info, struct keys_info *keys_info)
{
	enter_cmdline_mode(CMD_SUBMODE, L"", NULL);
}

static void
cmd_semicolon(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;
	if(last_fast_search_backward)
		find_F(last_fast_search_char);
	else
		find_f(last_fast_search_char);
}

/* Search forward. */
static void
cmd_slash(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 0;
	enter_cmdline_mode(SEARCH_FORWARD_SUBMODE, L"", NULL);
}

/* Search backward. */
static void
cmd_question(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 1;
	enter_cmdline_mode(SEARCH_BACKWARD_SUBMODE, L"", NULL);
}

static void
selector_a(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = curr_view->list_rows;
	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
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

/* Change word (rename file without extension). */
static void
cmd_cW(struct key_info key_info, struct keys_info *keys_info)
{
	rename_file(curr_view, 1);
}

/* Change group. */
static void
cmd_cg(struct key_info key_info, struct keys_info *keys_info)
{
	change_group(curr_view);
}

/* Change owner. */
static void
cmd_co(struct key_info key_info, struct keys_info *keys_info)
{
	change_owner(curr_view);
}

/* Change permissions. */
static void
cmd_cp(struct key_info key_info, struct keys_info *keys_info)
{
	enter_permissions_mode(curr_view);
}

/* Change word (rename file). */
static void
cmd_cw(struct key_info key_info, struct keys_info *keys_info)
{
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
	int i;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	i = curr_view->list_pos;
	curr_stats.save_msg = delete_file(curr_view, key_info.reg, 1, &i, use_trash);
#endif /* ENABLE_COMPATIBILITY_MODE */
}

static void
cmd_D_selector(struct key_info key_info, struct keys_info *keys_info)
{
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
	find_f(key_info.multi);
}

static void
find_f(int ch)
{
	int x;

	x = curr_view->list_pos;
	do
	{
		x++;
		if(x == curr_view->list_rows)
			x = 0;
		if(curr_view->dir_entry[x].name[0] == ch)
			break;
	}while(x != curr_view->list_pos);

	if(x == curr_view->list_pos)
		return;

	moveto_list_pos(curr_view, x);
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
	handle_file(curr_view, 1);
}

/* Move down one line. */
static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	curr_view->list_pos += key_info.count;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
selector_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	pick_files(curr_view, curr_view->list_pos + key_info.count, keys_info);
}

/* Move up one line. */
static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	curr_view->list_pos -= key_info.count;
	moveto_list_pos(curr_view, curr_view->list_pos);
}

static void
selector_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	pick_files(curr_view, curr_view->list_pos - key_info.count, keys_info);
}

static void
cmd_l(struct key_info key_info, struct keys_info *keys_info)
{
	handle_file(curr_view, 0);
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
	if(curr_view->selected_files == 0)
		find_pattern(curr_view, cfg.search_history[0], last_search_backward);

	if(last_search_backward)
		find_previous_pattern(curr_view);
	else
		find_next_pattern(curr_view);
}

/* Put files. */
static void
cmd_p(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = put_files_from_register(curr_view, key_info.reg, 0);
}

static void
cmd_s(struct key_info key_info, struct keys_info *keys_info)
{
	int i, x;

	keys_info->count = curr_view->selected_files;
	keys_info->indexes = malloc(keys_info->count*sizeof(keys_info->indexes[0]));
	if(keys_info->indexes == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	for(x = 0; x < curr_view->list_rows; x++)
	{
		if(curr_view->dir_entry[x].selected)
			keys_info->indexes[i++] = x;
	}
}

/* Tag file. */
static void
cmd_t(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->dir_entry[curr_view->list_pos].selected == 0)
	{
		/* The ../ dir cannot be selected */
		if (!strcmp(curr_view->dir_entry[curr_view->list_pos].name, "../"))
				return;

		curr_view->dir_entry[curr_view->list_pos].selected = 1;
		curr_view->selected_files++;
	}
	else
	{
		curr_view->dir_entry[curr_view->list_pos].selected = 0;
		curr_view->selected_files--;
	}

	draw_dir_list(curr_view, curr_view->top_line, curr_view->list_pos);
	wattron(curr_view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	mvwaddstr(curr_view->win, curr_view->curr_line, 0, " ");
	wattroff(curr_view->win, COLOR_PAIR(CURR_LINE_COLOR));
	wmove(curr_view->win, curr_view->curr_line, 0);
}

/* Yank file. */
static void
cmd_yy(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
		pick_files(curr_view, curr_view->list_pos + key_info.count - 1, keys_info);
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

#ifdef ENABLE_COMPATIBILITY_MODE
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);
#else /* ENABLE_COMPATIBILITY_MODE */
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, 1,
			&curr_view->list_pos);
#endif /* ENABLE_COMPATIBILITY_MODE */

	if(key_info.count != NO_COUNT_GIVEN)
		free(keys_info->indexes);
}

static void
cmd_y_selector(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	curr_stats.save_msg = yank_files(curr_view, key_info.reg, keys_info->count,
			keys_info->indexes);

	free(keys_info->indexes);
	keys_info->indexes = NULL;
	keys_info->count = 0;
}

static void
cmd_zM(struct key_info key_info, struct keys_info *keys_info)
{
	restore_filename_filter(curr_view);
	hide_dot_files(curr_view);
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
	show_dot_files(curr_view);
}

static void
cmd_za(struct key_info key_info, struct keys_info *keys_info)
{
	toggle_dot_files(curr_view);
}

/* Redraw with file in bottom of list. */
static void
cmd_zb(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;

	if(curr_view->list_pos < curr_view->window_rows)
		return;

	curr_view->top_line = curr_view->list_pos - curr_view->window_rows;
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
	hide_dot_files(curr_view);
}

static void
cmd_zo(struct key_info key_info, struct keys_info *keys_info)
{
	show_dot_files(curr_view);
}

/* Redraw with file in top of list. */
static void
cmd_zt(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;

	if(curr_view->list_rows - curr_view->list_pos >= curr_view->window_rows)
		curr_view->top_line = curr_view->list_pos;
	else
		curr_view->top_line = curr_view->list_rows - curr_view->window_rows;
	scroll_view(curr_view);
}

/* Redraw with file in center of list. */
static void
cmd_zz(struct key_info key_info, struct keys_info *keys_info)
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
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
		return;
	}

	delta = (view->list_pos > end) ? -1 : +1;
	i = 0;
	x = view->list_pos - delta;
	do {
		x += delta;
		keys_info->indexes[i++] = x;
	} while(x != end);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
