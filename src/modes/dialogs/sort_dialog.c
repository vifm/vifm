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

#include "../../compat/curses.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../ui/colors.h"
#include "../../ui/ui.h"
#include "../../utils/macros.h"
#include "../../utils/utils.h"
#include "../../filelist.h"
#include "../../status.h"
#include "../modes.h"
#include "../wk.h"
#include "msg_dialog.h"

static view_t *view;
static int top, bottom, curr, col;
static int descending;

static const char * caps[] = { "a-z", "z-a" };

#ifndef _WIN32
#define CORRECTION 0
#else
#define CORRECTION -8
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
	[SK_BY_INODE]         = 13,
#endif
	[SK_BY_SIZE]          = 14 + CORRECTION,
	[SK_BY_NITEMS]        = 15 + CORRECTION,
	[SK_BY_GROUPS]        = 16 + CORRECTION,
	[SK_BY_TARGET]        = 17 + CORRECTION,
	[SK_BY_TIME_ACCESSED] = 18 + CORRECTION,
	[SK_BY_TIME_CHANGED]  = 19 + CORRECTION,
	[SK_BY_TIME_MODIFIED] = 20 + CORRECTION,
};
ARRAY_GUARD(indexes, 1 + SK_COUNT);

static void leave_sort_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
#ifndef _WIN32
static void cmd_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_R(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_p(key_info_t key_info, keys_info_t *keys_info);
static void cmd_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_O(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_I(key_info_t key_info, keys_info_t *keys_info);
#endif
static void cmd_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_T(key_info_t key_info, keys_info_t *keys_info);
static void cmd_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void goto_line(int line);
static void print_at_pos(void);
static void clear_at_pos(void);

static keys_add_info_t builtin_cmds[] = {
	{WK_C_c,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_C_l,    {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_CR,     {{&cmd_return}, .descr = "apply selected sorting"}},
	{WK_C_n,    {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_p,    {{&cmd_k},      .descr = "go to item above"}},
	{WK_ESC,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_G,      {{&cmd_G},      .descr = "go to the last item"}},
	{WK_Z WK_Q, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_Z WK_Z, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_g WK_g, {{&cmd_gg},     .descr = "go to the first item"}},
	{WK_h,      {{&cmd_h},      .descr = "toggle ordering"}},
	{WK_SPACE,  {{&cmd_h},      .descr = "toggle ordering"}},
	{WK_j,      {{&cmd_j},      .descr = "go to item below"}},
	{WK_k,      {{&cmd_k},      .descr = "go to item above"}},
	{WK_l,      {{&cmd_return}, .descr = "apply selected sorting"}},
	{WK_q,      {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_e,      {{&cmd_e},      .descr = "sort by extension"}},
	{WK_f,      {{&cmd_f},      .descr = "sort by file extension"}},
	{WK_n,      {{&cmd_n},      .descr = "sort by name"}},
	{WK_N,      {{&cmd_N},      .descr = "sort by name (ignore case)"}},
	{WK_t,      {{&cmd_t},      .descr = "sort by file type"}},
	{WK_d,      {{&cmd_d},      .descr = "sort by file/directory trait"}},
#ifndef _WIN32
	{WK_r,      {{&cmd_r},      .descr = "sort by group id"}},
	{WK_R,      {{&cmd_R},      .descr = "sort by group name"}},
	{WK_M,      {{&cmd_M},      .descr = "sort by file mode"}},
	{WK_p,      {{&cmd_p},      .descr = "sort by file permissions"}},
	{WK_o,      {{&cmd_o},      .descr = "sort by owner id"}},
	{WK_O,      {{&cmd_O},      .descr = "sort by owner name"}},
	{WK_L,      {{&cmd_L},      .descr = "sort by number of hard-links"}},
	{WK_I,      {{&cmd_I},      .descr = "sort by inode number"}},
#endif
	{WK_s,      {{&cmd_s},      .descr = "sort by size"}},
	{WK_i,      {{&cmd_i},      .descr = "sort by number of files in directory"}},
	{WK_u,      {{&cmd_u},      .descr = "sort by 'sortgroups' match"}},
	{WK_T,      {{&cmd_T},      .descr = "sort by symbolic link target"}},
	{WK_a,      {{&cmd_a},      .descr = "sort by access time"}},
	{WK_m,      {{&cmd_m},      .descr = "by modification time"}},
#ifndef _WIN32
	{WK_c,      {{&cmd_c},      .descr = "by change time"}},
#else
	{WK_c,      {{&cmd_c},      .descr = "by creation time"}},
#endif
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_UP)},    {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_DOWN)},  {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_LEFT)},  {{&cmd_h},      .descr = "toggle ordering"}},
	{{K(KEY_RIGHT)}, {{&cmd_return}, .descr = "apply selected sorting"}},
	{{K(KEY_HOME)},  {{&cmd_gg},     .descr = "go to the first item"}},
	{{K(KEY_END)},   {{&cmd_G},      .descr = "go to the last item"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_sort_dialog_mode(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), SORT_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
enter_sort_mode(view_t *active_view)
{
	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(cv_compare(active_view->custom.type))
	{
		show_error_msg("Sorting", "Sorting of comparison view can't be changed.");
		return;
	}

	ui_hide_graphics();

	view = active_view;
	descending = (view->sort[0] < 0);
	vle_mode_set(SORT_MODE, VMT_SECONDARY);

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
	mvwaddstr(sort_win, cy++, 2, " [   ] I Inode");
#endif
	mvwaddstr(sort_win, cy++, 2, " [   ] s Size");
	mvwaddstr(sort_win, cy++, 2, " [   ] i Item Count");
	mvwaddstr(sort_win, cy++, 2, " [   ] u Groups");
	mvwaddstr(sort_win, cy++, 2, " [   ] T Link Target");
	mvwaddstr(sort_win, cy++, 2, " [   ] a Time Accessed");
#ifndef _WIN32
	mvwaddstr(sort_win, cy++, 2, " [   ] c Time Changed");
#else
	mvwaddstr(sort_win, cy++, 2, " [   ] c Time Created");
#endif
	mvwaddstr(sort_win, cy++, 2, " [   ] m Time Modified");
	assert(cy - top == SK_COUNT &&
			"Sort dialog and sort options should not diverge");
	print_at_pos();

	ui_refresh_win(sort_win);
}

static void
leave_sort_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	ui_view_reset_selection_and_reload(view);

	stats_redraw_later();
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
cmd_return(key_info_t key_info, keys_info_t *keys_info)
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
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	descending = !descending;
	clear_at_pos();
	print_at_pos();
	ui_refresh_win(sort_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	clear_at_pos();
	curr += def_count(key_info.count);
	if(curr > bottom)
		curr = bottom;

	print_at_pos();
	ui_refresh_win(sort_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	clear_at_pos();
	curr -= def_count(key_info.count);
	if(curr < top)
		curr = top;

	print_at_pos();
	ui_refresh_win(sort_win);
}

static void
cmd_e(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_EXTENSION]);
	cmd_return(key_info, keys_info);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_FILEEXT]);
	cmd_return(key_info, keys_info);
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_NAME]);
	cmd_return(key_info, keys_info);
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_INAME]);
	cmd_return(key_info, keys_info);
}

static void
cmd_t(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_TYPE]);
	cmd_return(key_info, keys_info);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_DIR]);
	cmd_return(key_info, keys_info);
}

#ifndef _WIN32

static void
cmd_r(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_GROUP_ID]);
	cmd_return(key_info, keys_info);
}

static void
cmd_R(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_GROUP_NAME]);
	cmd_return(key_info, keys_info);
}

static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_MODE]);
	cmd_return(key_info, keys_info);
}

static void
cmd_p(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_PERMISSIONS]);
	cmd_return(key_info, keys_info);
}

static void
cmd_o(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_OWNER_ID]);
	cmd_return(key_info, keys_info);
}

static void
cmd_O(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_OWNER_NAME]);
	cmd_return(key_info, keys_info);
}

static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_NLINKS]);
	cmd_return(key_info, keys_info);
}

static void
cmd_I(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_INODE]);
	cmd_return(key_info, keys_info);
}

#endif

static void
cmd_s(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_SIZE]);
	cmd_return(key_info, keys_info);
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_NITEMS]);
	cmd_return(key_info, keys_info);
}

static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_GROUPS]);
	cmd_return(key_info, keys_info);
}

static void
cmd_T(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_TARGET]);
	cmd_return(key_info, keys_info);
}

static void
cmd_a(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_TIME_ACCESSED]);
	cmd_return(key_info, keys_info);
}

static void
cmd_c(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_TIME_CHANGED]);
	cmd_return(key_info, keys_info);
}

static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	goto_line(top + indexes[SK_BY_TIME_MODIFIED]);
	cmd_return(key_info, keys_info);
}

/* Moves cursor to the specified line and updates the dialog. */
static void
goto_line(int line)
{
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
	ui_refresh_win(sort_win);
}

static void
print_at_pos(void)
{
	mvwaddstr(sort_win, curr, col, caps[descending]);
	checked_wmove(sort_win, curr, col);
}

static void
clear_at_pos(void)
{
	mvwaddstr(sort_win, curr, col, "   ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
