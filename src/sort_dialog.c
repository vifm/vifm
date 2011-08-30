/* vifm
 * Copyright (C) 2001 Ken Steen.
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
#include "status.h"
#include "ui.h"

#include "sort_dialog.h"

static int *mode;
static FileView *view;
static int top, bottom, curr, col;
static int descending;

static const char * caps[] = { "a-z", "z-a" };

static int indexes[] = {
	-1,
	0,  /* SORT_BY_EXTENSION */
	1,  /* SORT_BY_NAME */
	3,  /* SORT_BY_GROUP_ID */
	4,  /* SORT_BY_GROUP_NAME */
	5,  /* SORT_BY_MODE */
	6,  /* SORT_BY_OWNER_ID */
	7,  /* SORT_BY_OWNER_NAME */
	8,  /* SORT_BY_SIZE */
	9,  /* SORT_BY_TIME_ACCESSED */
	10, /* SORT_BY_TIME_CHANGED */
	11, /* SORT_BY_TIME_MODIFIED */
	2,  /* SORT_BY_INAME */
};

static int _gnuc_unused indexes_size_guard[
	(ARRAY_LEN(indexes) == NUM_SORT_OPTIONS + 1) ? 1 : -1
];

static void leave_sort_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void goto_line(int line);
static void cmd_h(struct key_info, struct keys_info *);
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
	{L"h", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L" ", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_LEFT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_h}}},
	{{KEY_RIGHT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
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
	if(curr_stats.vifm_started < 2)
		return;

	view = active_view;
	descending = (view->sort[0] < 0);
	*mode = SORT_MODE;

	wattroff(view->win, COLOR_PAIR(cfg.color_scheme + CURR_LINE_COLOR) | A_BOLD);
	curs_set(0);
	update_all_windows();

	top = 2;
	bottom = top + NUM_SORT_OPTIONS - 1;
	curr = indexes[abs(view->sort[0])] + 2;
	col = 6;

	redraw_sort_dialog();
}

void
redraw_sort_dialog(void)
{
	int x, y;

	werase(sort_win);
	box(sort_win, ACS_VLINE, ACS_HLINE);

	getmaxyx(sort_win, y, x);
	mvwaddstr(sort_win, 0, (x - 6)/2, " Sort ");
	mvwaddstr(sort_win, 1, 2, " Sort files by:");
	mvwaddstr(sort_win, 2, 4, " [   ] File Extenstion");
	mvwaddstr(sort_win, 3, 4, " [   ] Name");
	mvwaddstr(sort_win, 4, 4, " [   ] Name (ignore case)");
	mvwaddstr(sort_win, 5, 4, " [   ] Group ID");
	mvwaddstr(sort_win, 6, 4, " [   ] Group Name");
	mvwaddstr(sort_win, 7, 4, " [   ] Mode");
	mvwaddstr(sort_win, 8, 4, " [   ] Owner ID");
	mvwaddstr(sort_win, 9, 4, " [   ] Owner Name");
	mvwaddstr(sort_win, 10, 4, " [   ] Size");
	mvwaddstr(sort_win, 11, 4, " [   ] Time Accessed");
	mvwaddstr(sort_win, 12, 4, " [   ] Time Changed");
	mvwaddstr(sort_win, 13, 4, " [   ] Time Modified");
	mvwaddstr(sort_win, curr, 6, caps[descending]);

	getmaxyx(stdscr, y, x);
	mvwin(sort_win, (y - (NUM_SORT_OPTIONS + 3))/2, (x - 30)/2);
	wrefresh(sort_win);
}

static void
leave_sort_mode(void)
{
	*mode = NORMAL_MODE;

	clean_selected_files(view);

	update_all_windows();
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_sort_mode();
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	int i;

	leave_sort_mode();

	for(i = 0; i < sizeof(indexes); i++)
		if(indexes[i] == curr - 2)
			break;
	change_sort_type(view, i, descending);
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(bottom);
	else
		goto_line(key_info.count + top - 1);
}

static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
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
cmd_h(struct key_info key_info, struct keys_info *keys_info)
{
	descending = !descending;
	clear_at_pos();
	print_at_pos();
	wrefresh(sort_win);
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
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
cmd_k(struct key_info key_info, struct keys_info *keys_info)
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
