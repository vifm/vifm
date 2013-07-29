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

#include "sort_dialog.h"

#include <curses.h>

#include <assert.h> /* assert() */
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

static int *mode;
static FileView *view;
static int top, bottom, curr, col;
static int descending;

static const char * caps[] = { "a-z", "z-a" };

#ifndef _WIN32
#define CORRECTION 0
#else
#define CORRECTION -6
#endif

static int indexes[] = {
	-1,
	0,               /* SORT_BY_EXTENSION */
	1,               /* SORT_BY_NAME */
#ifndef _WIN32
	3,               /* SORT_BY_GROUP_ID */
	4,               /* SORT_BY_GROUP_NAME */
	5,               /* SORT_BY_MODE */
	7,               /* SORT_BY_OWNER_ID */
	8,               /* SORT_BY_OWNER_NAME */
#endif
	9 + CORRECTION,  /* SORT_BY_SIZE */
	10 + CORRECTION, /* SORT_BY_TIME_ACCESSED */
	11 + CORRECTION, /* SORT_BY_TIME_CHANGED */
	12 + CORRECTION, /* SORT_BY_TIME_MODIFIED */
	2,               /* SORT_BY_INAME */
#ifndef _WIN32
	6,               /* SORT_BY_PERMISSIONS */
#endif
};
ARRAY_GUARD(indexes, 1 + SORT_OPTION_COUNT);

static void leave_sort_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void goto_line(int line);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void print_at_pos(void);
static void clear_at_pos(void);

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
	{L"h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_LEFT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_sort_dialog_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), SORT_MODE);
	assert(ret_code == 0);
}

void
enter_sort_mode(FileView *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	descending = (view->sort[0] < 0);
	*mode = SORT_MODE;

	wattroff(view->win, COLOR_PAIR(DCOLOR_BASE + CURR_LINE_COLOR) | A_BOLD);
	curs_set(FALSE);
	update_all_windows();

	top = 3;
	bottom = top + SORT_OPTION_COUNT - 1;
	curr = top + indexes[abs(view->sort[0])];
	col = 6;

	redraw_sort_dialog();
}

void
redraw_sort_dialog(void)
{
	int x, y, cy;

	wresize(sort_win, SORT_OPTION_COUNT + 5, SORT_WIN_WIDTH);

	werase(sort_win);
	box(sort_win, ACS_VLINE, ACS_HLINE);

	getmaxyx(sort_win, y, x);
	mvwaddstr(sort_win, 0, (x - 6)/2, " Sort ");
	mvwaddstr(sort_win, top - 1, 2, " Sort files by:");
	cy = top;
	mvwaddstr(sort_win, cy++, 4, " [   ] File Extenstion");
	mvwaddstr(sort_win, cy++, 4, " [   ] Name");
	mvwaddstr(sort_win, cy++, 4, " [   ] Name (ignore case)");
#ifndef _WIN32
	mvwaddstr(sort_win, cy++, 4, " [   ] Group ID");
	mvwaddstr(sort_win, cy++, 4, " [   ] Group Name");
	mvwaddstr(sort_win, cy++, 4, " [   ] Mode");
	mvwaddstr(sort_win, cy++, 4, " [   ] Permissions");
	mvwaddstr(sort_win, cy++, 4, " [   ] Owner ID");
	mvwaddstr(sort_win, cy++, 4, " [   ] Owner Name");
#endif
	mvwaddstr(sort_win, cy++, 4, " [   ] Size");
	mvwaddstr(sort_win, cy++, 4, " [   ] Time Accessed");
#ifndef _WIN32
	mvwaddstr(sort_win, cy++, 4, " [   ] Time Changed");
#else
	mvwaddstr(sort_win, cy++, 4, " [   ] Time Created");
#endif
	mvwaddstr(sort_win, cy++, 4, " [   ] Time Modified");
	assert(cy - top == SORT_OPTION_COUNT &&
			"Sort dialog and sort options should not diverge");
	mvwaddstr(sort_win, curr, 6, caps[descending]);

	getmaxyx(stdscr, y, x);
	mvwin(sort_win, (y - (cy - 2 + 3))/2, (x - SORT_WIN_WIDTH)/2);
	wrefresh(sort_win);
}

static void
leave_sort_mode(void)
{
	*mode = NORMAL_MODE;

	clean_selected_files(view);
	load_saving_pos(view, 1);

	update_all_windows();
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_sort_mode();
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	int i;

	leave_sort_mode();

	for(i = 0; i < ARRAY_LEN(indexes); i++)
		if(indexes[i] == curr - top)
			break;
	change_sort_type(view, i, descending);
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(bottom);
	else
		goto_line(key_info.count + top - 1);
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(top);
	else
		goto_line(key_info.count + top - 1);
}

static void
goto_line(int line)
{
	if(line > bottom)
		line = bottom;
	if(curr == line)
		return;

	clear_at_pos();
	curr = line;
	print_at_pos();
	wrefresh(sort_win);
}

static void
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	descending = !descending;
	clear_at_pos();
	print_at_pos();
	wrefresh(sort_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clear_at_pos();
	curr += key_info.count;
	if(curr > bottom)
		curr = bottom;

	print_at_pos();
	wrefresh(sort_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clear_at_pos();
	curr -= key_info.count;
	if(curr < top)
		curr = top;

	print_at_pos();
	wrefresh(sort_win);
}

static void
print_at_pos(void)
{
	mvwaddstr(sort_win, curr, col, caps[descending]);
}

static void
clear_at_pos(void)
{
	mvwaddstr(sort_win, curr, col, "   ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
