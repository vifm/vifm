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
#include <ctype.h>
#include <string.h>

#include "../../cfg/config.h"
#include "../../engine/keys.h"
#include "../../menus/menus.h"
#include "../../bookmarks.h"
#include "../../color_scheme.h"
#include "../../commands.h"
#include "../../filelist.h"
#include "../../fileops.h"
#include "../../status.h"
#include "../../ui.h"
#include "../cmdline.h"
#include "../modes.h"
#include "attr_dialog.h"

static void leave_change_mode(int clean_selection);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void goto_line(int line);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void print_at_pos(void);
static void clear_at_pos(void);

static int *mode;
static FileView *view;
static int top, bottom, step, curr, col;

static keys_add_info_t builtin_cmds[] = {
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* return */
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_change_dialog_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), CHANGE_MODE);
	assert(ret_code == 0);
}

void
enter_change_mode(FileView *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	*mode = CHANGE_MODE;

	wattroff(view->win, COLOR_PAIR(DCOLOR_BASE + CURR_LINE_COLOR) | A_BOLD);
	curs_set(FALSE);
	update_all_windows();

	top = 2;
#ifndef _WIN32
	bottom = 8;
#else
	bottom = 4;
#endif
	curr = 2;
	col = 6;
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
	box(change_win, ACS_VLINE, ACS_HLINE);

	mvwaddstr(change_win, 0, (x - 20)/2, " Change Current File ");
	mvwaddstr(change_win, 2, 4, " [ ] Name");
#ifndef _WIN32
	mvwaddstr(change_win, 4, 4, " [ ] Owner");
	mvwaddstr(change_win, 6, 4, " [ ] Group");
	mvwaddstr(change_win, 8, 4, " [ ] Permissions");
#else
	mvwaddstr(change_win, 4, 4, " [ ] Properties");
#endif
	mvwaddch(change_win, 2, 6, '*');

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - getmaxy(change_win))/2, (x - getmaxx(change_win))/2);
	wrefresh(change_win);
}

static void
leave_change_mode(int clean_selection)
{
	*mode = NORMAL_MODE;

	if(clean_selection)
	{
		clean_selected_files(view);
		load_saving_pos(view, 1);
	}

	update_all_windows();
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_change_mode(1);
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	leave_change_mode(0);

	if(curr == 2)
		rename_current_file(view, 0);
#ifndef _WIN32
	else if(curr == 4)
		change_owner();
	else if(curr == 6)
		change_group();
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
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(1);
	else
		goto_line(key_info.count);
}

static void
goto_line(int line)
{
	line = top + (line - 1)*step;
	if(line > bottom)
		line = bottom;
	if(curr == line)
		return;

	clear_at_pos();
	curr = line;
	print_at_pos();
	wrefresh(change_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clear_at_pos();
	curr += key_info.count*step;
	if(curr > bottom)
		curr = bottom;

	print_at_pos();
	wrefresh(change_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clear_at_pos();
	curr -= key_info.count*step;
	if(curr < top)
		curr = top;

	print_at_pos();
	wrefresh(change_win);
}

static void
print_at_pos(void)
{
	mvwaddstr(change_win, curr, col, "*");
}

static void
clear_at_pos(void)
{
	mvwaddstr(change_win, curr, col, " ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
