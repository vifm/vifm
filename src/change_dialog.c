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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <curses.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "../config.h"

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
#include "status.h"
#include "ui.h"

#include "change_dialog.h"

static int *mode;
static FileView *view;
static int top, bottom, step, curr, col;

static void leave_change_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void goto_line(int line);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void print_at_pos(void);
static void clear_at_pos(void);

static struct keys_add_info builtin_cmds[] = {
	{L"\x03", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* return */
	{L"\x0d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	/* escape */
	{L"\x1b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
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
	if(curr_stats.vifm_started < 2)
		return;

	view = active_view;
	*mode = CHANGE_MODE;

	wattroff(view->win, COLOR_PAIR(DCOLOR_BASE + CURR_LINE_COLOR) | A_BOLD);
	curs_set(0);
	update_all_windows();

	top = 2;
	bottom = 8;
	curr = 2;
	col = 6;
	step = 2;

	redraw_change_dialog();
}

void
redraw_change_dialog(void)
{
	int x, y;

	wresize(change_win, 11, 25);

	getmaxyx(change_win, y, x);
	werase(change_win);
	box(change_win, ACS_VLINE, ACS_HLINE);

	mvwaddstr(change_win, 0, (x - 20)/2, " Change Current File ");
	mvwaddstr(change_win, 2, 4, " [ ] Name");
	mvwaddstr(change_win, 4, 4, " [ ] Owner");
	mvwaddstr(change_win, 6, 4, " [ ] Group");
	mvwaddstr(change_win, 8, 4, " [ ] Permissions");
	mvwaddch(change_win, 2, 6, '*');

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - getmaxy(change_win))/2, (x - getmaxx(change_win))/2);
	wrefresh(change_win);
}

static void
leave_change_mode(void)
{
	*mode = NORMAL_MODE;

	clean_selected_files(view);

	update_all_windows();
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_change_mode();
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	leave_change_mode();

	if(curr == 2)
		rename_file(view, 0);
	else if(curr == 4)
		change_owner();
	else if(curr == 6)
		change_group();
	else if(curr == 8)
		enter_permissions_mode(view);
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(bottom);
	else
		goto_line(key_info.count);
}

static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
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
cmd_j(struct key_info key_info, struct keys_info *keys_info)
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
cmd_k(struct key_info key_info, struct keys_info *keys_info)
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
