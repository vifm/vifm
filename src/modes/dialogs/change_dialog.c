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

#include "change_dialog.h"

#include <curses.h>

#include <assert.h>
#include <string.h>

#include "../../cfg/config.h"
#include "../../compat/curses.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../ui/colors.h"
#include "../../ui/ui.h"
#include "../../utils/macros.h"
#include "../../utils/utils.h"
#include "../../fops_misc.h"
#include "../../fops_rename.h"
#include "../../status.h"
#include "../modes.h"
#include "../wk.h"
#include "attr_dialog.h"

static void leave_change_mode(int clear_selection);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void goto_line(int line);
static void print_at_pos(void);
static void clear_at_pos(void);

static view_t *view;
static int top, bottom, step, curr, col;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_c,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_CR,     {{&cmd_return}, .descr = "perform selected action"}},
	{WK_C_n,    {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_p,    {{&cmd_k},      .descr = "go to item above"}},
	{WK_ESC,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_G,      {{&cmd_G},      .descr = "go to the last item"}},
	{WK_Z WK_Q, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_Z WK_Z, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_g WK_g, {{&cmd_gg},     .descr = "go to the first item"}},
	{WK_j,      {{&cmd_j},      .descr = "go to item below"}},
	{WK_k,      {{&cmd_k},      .descr = "go to item above"}},
	{WK_l,      {{&cmd_return}, .descr = "perform selected action"}},
	{WK_q,      {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_n,      {{&cmd_n},      .descr = "rename file"}},
	{WK_o,      {{&cmd_o},      .descr = "change owner"}},
	{WK_g,      {{&cmd_g},      .descr = "change group"}},
	{WK_p,      {{&cmd_p},      .descr = "change file permissions/attributes"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_UP)},    {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_DOWN)},  {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_RIGHT)}, {{&cmd_return}, .descr = "perform selected action"}},
	{{K(KEY_HOME)},  {{&cmd_gg},     .descr = "go to the first item"}},
	{{K(KEY_END)},   {{&cmd_G},      .descr = "go to the last item"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_change_dialog_mode(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), CHANGE_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
enter_change_mode(view_t *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	ui_hide_graphics();

	view = active_view;
	vle_mode_set(CHANGE_MODE, VMT_SECONDARY);

	ui_set_cursor(/*visibility=*/0);
	update_all_windows();

	top = 2;
#ifndef _WIN32
	bottom = 8;
#else
	bottom = 4;
#endif
	curr = 2;
	col = 5;
	step = 2;

	redraw_change_dialog();
}

void
redraw_change_dialog(void)
{
	int x, y;

	wresize(change_win, bottom + 3, 25);

	getmaxyx(change_win, y, x);
	werase(change_win);
	box(change_win, 0, 0);

	mvwaddstr(change_win, 0, (x - 20)/2, " Change Current File ");
	mvwaddstr(change_win, 2, 3, " [ ] n Name");
#ifndef _WIN32
	mvwaddstr(change_win, 4, 3, " [ ] o Owner");
	mvwaddstr(change_win, 6, 3, " [ ] g Group");
	mvwaddstr(change_win, 8, 3, " [ ] p Permissions");
#else
	mvwaddstr(change_win, 4, 3, " [ ] p Properties");
#endif
	print_at_pos();

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - getmaxy(change_win))/2, (x - getmaxx(change_win))/2);
	ui_refresh_win(change_win);
}

static void
leave_change_mode(int clear_selection)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	if(clear_selection)
	{
		ui_view_reset_selection_and_reload(view);
	}

	stats_redraw_later();
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_change_mode(1);
}

static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	leave_change_mode(0);

	if(curr == 2)
		fops_rename_current(view, 0);
#ifndef _WIN32
	else if(curr == 4)
		fops_chuser();
	else if(curr == 6)
		fops_chgroup();
	else if(curr == 8)
		enter_attr_mode(view);
#else
	else if(curr == 4)
		enter_attr_mode(view);
#endif
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(bottom);
	else
		goto_line(key_info.count);
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(def_count(key_info.count));
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	clear_at_pos();
	curr += def_count(key_info.count)*step;
	if(curr > bottom)
		curr = bottom;

	print_at_pos();
	ui_refresh_win(change_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	clear_at_pos();
	curr -= def_count(key_info.count)*step;
	if(curr < top)
		curr = top;

	print_at_pos();
	ui_refresh_win(change_win);
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(1);
	cmd_return(key_info, keys_info);
}

static void
cmd_o(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(2);
	cmd_return(key_info, keys_info);
}

static void
cmd_g(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(3);
	cmd_return(key_info, keys_info);
}

static void
cmd_p(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(4);
	cmd_return(key_info, keys_info);
}

/* Moves cursor to the specified line and updates the dialog. */
static void
goto_line(int line)
{
	line = top + (line - 1)*step;
	if(line > bottom)
	{
		line = bottom;
	}
	if(curr == line)
	{
		return;
	}

	clear_at_pos();
	curr = line;
	print_at_pos();
	ui_refresh_win(change_win);
}

static void
print_at_pos(void)
{
	mvwaddstr(change_win, curr, col, "*");
	checked_wmove(change_win, curr, col);
}

static void
clear_at_pos(void)
{
	mvwaddstr(change_win, curr, col, " ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
