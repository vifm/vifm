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
#include <stdlib.h> /* abs() */
#include <string.h>

#include "../../cfg/config.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../ui/colors.h"
#include "../../ui/ui.h"
#include "../../utils/macros.h"
#include "../../filelist.h"
#include "../../status.h"
#include "../modes.h"

static FileView *view;
static int top, bottom, curr, col;
static int descending;

static const char * caps[] = { "a-z", "z-a" };

#ifndef _WIN32
#define CORRECTION 0
#else
#define CORRECTION -7
#endif

/* This maps actual sorting keys onto position of corresponding lines of the
 * dialog.  See comment for SortingKey enumeration for why we can't change order
 * of sorting keys. */
static int indexes[] = {
	/* SK_* start with 1. */
	[0] = -1,

	[SK_BY_EXTENSION]     = 0,
	[SK_BY_FILEEXT]       = 1,
	[SK_BY_NAME]          = 2,
	[SK_BY_INAME]         = 3,
	[SK_BY_TYPE]          = 4,
	[SK_BY_DIR]           = 5,
#ifndef _WIN32
	[SK_BY_GROUP_ID]      = 6,
	[SK_BY_GROUP_NAME]    = 7,
	[SK_BY_MODE]          = 8,
	[SK_BY_PERMISSIONS]   = 9,
	[SK_BY_OWNER_ID]      = 10,
	[SK_BY_OWNER_NAME]    = 11,
	[SK_BY_NLINKS]        = 12,
#endif
	[SK_BY_SIZE]          = 13 + CORRECTION,
	[SK_BY_NITEMS]        = 14 + CORRECTION,
	[SK_BY_GROUPS]        = 15 + CORRECTION,
	[SK_BY_TIME_ACCESSED] = 16 + CORRECTION,
	[SK_BY_TIME_CHANGED]  = 17 + CORRECTION,
	[SK_BY_TIME_MODIFIED] = 18 + CORRECTION,
};
ARRAY_GUARD(indexes, 1 + SK_COUNT);

static void leave_sort_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void goto_line(int line);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_R(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void cmd_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_O(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void print_at_pos(void);
static void clear_at_pos(void);

static keys_add_info_t builtin_cmds[] = {
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
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
	{L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_e}}},
	{L"f", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_f}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"N", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_t}}},
	{L"d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"r", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_r}}},
	{L"R", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_R}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"p", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_p}}},
	{L"o", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_o}}},
	{L"O", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_O}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"s", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_s}}},
	{L"i", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_i}}},
	{L"u", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_u}}},
	{L"a", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_a}}},
	{L"c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_c}}},
	{L"m", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_m}}},
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
init_sort_dialog_mode(void)
{
	int ret_code;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), SORT_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
enter_sort_mode(FileView *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	descending = (view->sort[0] < 0);
	vle_mode_set(SORT_MODE, VMT_SECONDARY);

	wattroff(view->win, COLOR_PAIR(cfg.cs.pair[CURR_LINE_COLOR]) | A_BOLD);
	curs_set(FALSE);
	update_all_windows();

	top = 4;
	bottom = top + SK_COUNT - 1;
	curr = top + indexes[abs(view->sort[0])];
	col = 4;

	redraw_sort_dialog();
}

void
redraw_sort_dialog(void)
{
	int x, y, cy;

	y = (getmaxy(stdscr) - ((top + SK_COUNT) - 2 + 3))/2;
	x = (getmaxx(stdscr) - SORT_WIN_WIDTH)/2;
	wresize(sort_win, MIN(getmaxy(stdscr), SK_COUNT + 6), SORT_WIN_WIDTH);
	mvwin(sort_win, MAX(0, y), x);

	werase(sort_win);
	box(sort_win, 0, 0);

	mvwaddstr(sort_win, 0, (getmaxx(sort_win) - 6)/2, " Sort ");
	mvwaddstr(sort_win, top - 2, 2, " Sort files by:");
	cy = top;
	mvwaddstr(sort_win, cy++, 2, " [   ] e Extension");
	mvwaddstr(sort_win, cy++, 2, " [   ] f File Extension");
	mvwaddstr(sort_win, cy++, 2, " [   ] n Name");
	mvwaddstr(sort_win, cy++, 2, " [   ] N Name (ignore case)");
	mvwaddstr(sort_win, cy++, 2, " [   ] t Type");
	mvwaddstr(sort_win, cy++, 2, " [   ] d Dir");
#ifndef _WIN32
	mvwaddstr(sort_win, cy++, 2, " [   ] r Group ID");
	mvwaddstr(sort_win, cy++, 2, " [   ] R Group Name");
	mvwaddstr(sort_win, cy++, 2, " [   ] M Mode");
	mvwaddstr(sort_win, cy++, 2, " [   ] p Permissions");
	mvwaddstr(sort_win, cy++, 2, " [   ] o Owner ID");
	mvwaddstr(sort_win, cy++, 2, " [   ] O Owner Name");
	mvwaddstr(sort_win, cy++, 2, " [   ] L Links Count");
#endif
	mvwaddstr(sort_win, cy++, 2, " [   ] s Size");
	mvwaddstr(sort_win, cy++, 2, " [   ] i Item Count");
	mvwaddstr(sort_win, cy++, 2, " [   ] u Groups");
	mvwaddstr(sort_win, cy++, 2, " [   ] a Time Accessed");
#ifndef _WIN32
	mvwaddstr(sort_win, cy++, 2, " [   ] c Time Changed");
#else
	mvwaddstr(sort_win, cy++, 2, " [   ] c Time Created");
#endif
	mvwaddstr(sort_win, cy++, 2, " [   ] m Time Modified");
	assert(cy - top == SK_COUNT &&
			"Sort dialog and sort options should not diverge");
	mvwaddstr(sort_win, curr, 4, caps[descending]);

	wrefresh(sort_win);
}

static void
leave_sort_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	ui_view_reset_selection_and_reload(view);

	update_all_windows();
}

/* Redraws the dialog. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	redraw_sort_dialog();
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_sort_mode();
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	size_t i;

	leave_sort_mode();

	for(i = 0U; i < ARRAY_LEN(indexes); ++i)
	{
		if(indexes[i] == curr - top)
		{
			break;
		}
	}
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
cmd_e(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 0);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 1);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 2);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 3);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_t(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 4);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 5);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_r(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 6);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_R(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 7);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 8);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_p(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 9);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_o(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 10);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_O(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 11);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 12);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_s(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 13 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 14 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 15 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_a(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 16 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_c(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 17 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
}

static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + 18 + CORRECTION);
	cmd_ctrl_m(key_info, keys_info);
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
/* vim: set cinoptions+=t0 filetype=c : */
