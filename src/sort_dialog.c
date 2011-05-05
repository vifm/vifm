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

static void init_extended_keys(void);
static void leave_sort_mode(void);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_m(struct key_info, struct keys_info *);
static void keys_G(struct key_info, struct keys_info *);
static void keys_gg(struct key_info, struct keys_info *);
static void goto_line(int line);
static void keys_h(struct key_info, struct keys_info *);
static void keys_j(struct key_info, struct keys_info *);
static void keys_k(struct key_info, struct keys_info *);
static void print_at_pos(void);
static void clear_at_pos(void);

void
init_sort_dialog_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_keys(L"\x03", SORT_MODE);
	curr->data.handler = keys_ctrl_c;

	/* return */
	curr = add_keys(L"\x0d", SORT_MODE);
	curr->data.handler = keys_ctrl_m;

	/* escape */
	curr = add_keys(L"\x1b", SORT_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L"G", SORT_MODE);
	curr->data.handler = keys_G;

	curr = add_keys(L"gg", SORT_MODE);
	curr->data.handler = keys_gg;

	curr = add_keys(L"h", SORT_MODE);
	curr->data.handler = keys_h;

	curr = add_keys(L"j", SORT_MODE);
	curr->data.handler = keys_j;

	curr = add_keys(L"k", SORT_MODE);
	curr->data.handler = keys_k;

	curr = add_keys(L"l", SORT_MODE);
	curr->data.handler = keys_ctrl_m;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_UP;
	curr = add_keys(buf, SORT_MODE);
	curr->data.handler = keys_k;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, SORT_MODE);
	curr->data.handler = keys_j;
#endif /* ENABLE_EXTENDED_KEYS */
}

void
enter_sort_mode(FileView *active_view)
{
	view = active_view;
	descending = view->sort_descending;
	*mode = SORT_MODE;

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	curs_set(0);
	update_all_windows();

	top = 2;
	bottom = top + NUM_SORT_OPTIONS - 1;
	curr = view->sort_type + 2;
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
	mvwaddstr(sort_win, 3, 4, " [   ] File Name");
	mvwaddstr(sort_win, 4, 4, " [   ] Group ID");
	mvwaddstr(sort_win, 5, 4, " [   ] Group Name");
	mvwaddstr(sort_win, 6, 4, " [   ] Mode");
	mvwaddstr(sort_win, 7, 4, " [   ] Owner ID");
	mvwaddstr(sort_win, 8, 4, " [   ] Owner Name");
	mvwaddstr(sort_win, 9, 4, " [   ] Size");
	mvwaddstr(sort_win, 10, 4, " [   ] Time Accessed");
	mvwaddstr(sort_win, 11, 4, " [   ] Time Changed");
	mvwaddstr(sort_win, 12, 4, " [   ] Time Modified");
	mvwaddstr(sort_win, curr, 6, caps[descending]);

	getmaxyx(stdscr, y, x);
	mvwin(sort_win, (y - (NUM_SORT_OPTIONS + 3))/2, (x - 30)/2);
	wrefresh(sort_win);
}

static void
leave_sort_mode(void)
{
	*mode = NORMAL_MODE;
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_sort_mode();
}

static void
keys_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char filename[NAME_MAX];

	leave_sort_mode();

	view->sort_type = curr - 2;
	view->sort_descending = descending;
	curr_stats.setting_change = 1;

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	load_dir_list(view, 1);
	moveto_list_pos(view, find_file_pos_in_list(view, filename));
}

static void
keys_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		goto_line(bottom);
	else
		goto_line(key_info.count + top - 1);
}

static void
keys_gg(struct key_info key_info, struct keys_info *keys_info)
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
keys_h(struct key_info key_info, struct keys_info *keys_info)
{
	descending = !descending;
	clear_at_pos();
	print_at_pos();
	wrefresh(sort_win);
}

static void
keys_j(struct key_info key_info, struct keys_info *keys_info)
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
keys_k(struct key_info key_info, struct keys_info *keys_info)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
