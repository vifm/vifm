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

#include <curses.h>

#include <assert.h>
#include <string.h>

#include "../config.h"

#include "attr_dialog.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "menus.h"
#include "modes.h"
#include "normal.h"
#include "search.h"
#include "status.h"
#include "utils.h"

#include "visual.h"

static void cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_quote(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_comma(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_semicolon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_question(key_info_t key_info, keys_info_t *keys_info);
static void cmd_C(key_info_t key_info, keys_info_t *keys_info);
static void cmd_D(key_info_t key_info, keys_info_t *keys_info);
static void cmd_F(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_H(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_O(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void delete(key_info_t key_info, int use_trash);
static void cmd_cp(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void goto_pos(int pos);
static void cmd_gU(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gu(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(key_info_t key_info, int backward);
static void cmd_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_paren(key_info_t key_info, keys_info_t *keys_info);
static void find_goto(int ch, int count, int backward);
static void select_up_one(FileView *view, int start_pos);
static void select_down_one(FileView *view, int start_pos);
static void update_marks(FileView *view);
static void update(void);
static int find_update(FileView *view, int backward);

static int *mode;
static FileView *view;
static int start_pos;
static int last_fast_search_char;
static int last_fast_search_backward = -1;
static int upwards_range;
static int search_repeat;

static keys_add_info_t builtin_cmds[] = {
	{L"\x01", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_a}}},
	{L"\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x15", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x18", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_x}}},
	{L"\x19", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"'", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L"%", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L":", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L";", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"/", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L"?", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_question}}},
	{L"C", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_C}}},
	{L"D", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_D}}},
	{L"F", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"N", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"O", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_O}}},
	{L"U", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"V", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"f", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"cp", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cp}}},
	{L"cw", {BUILTIN_CMD, FOLLOWED_BY_NONE, {.cmd = L":rename\r"}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"gU", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gU}}},
	{L"gu", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"gv", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gv}}},
	{L"i", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_i}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
	{L"m", {BUILTIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_m}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"o", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_O}}},
	{L"u", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gu}}},
	{L"v", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"y", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_y}}},
	{L"zb", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zb}}},
	{L"zf", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zf}}},
	{L"zt", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zt}}},
	{L"zz", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zz}}},
	{L"(", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left_paren}}},
	{L")", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right_paren}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_l}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_visual_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), VISUAL_MODE);
	assert(ret_code == 0);
}

void
enter_visual_mode(int restore_selection)
{
	int ub = check_mark_directory(view, '<');
	int lb = check_mark_directory(view, '>');

	if(restore_selection && (ub < 0 || lb < 0))
		return;

	view = curr_view;
	start_pos = view->list_pos;
	*mode = VISUAL_MODE;

	clean_selected_files(view);

	if(restore_selection)
	{
		key_info_t ki;
		cmd_gv(ki, NULL);
	}
	else if(pathcmp(view->dir_entry[view->list_pos].name, "../"))
	{
		/* Don't allow the ../ dir to be selected */
		view->selected_files = 1;
		view->dir_entry[view->list_pos].selected = 1;
	}

	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
}

void
leave_visual_mode(int save_msg, int goto_top, int clean_selection)
{
	if(goto_top)
	{
		int ub = check_mark_directory(view, '<');
		if(ub != -1)
			view->list_pos = ub;
	}

	if(clean_selection)
	{
		int i;
		for(i = 0; i < view->list_rows; i++)
			view->dir_entry[i].search_match = 0;

		for(i = 0; i < view->list_rows; i++)
			view->dir_entry[i].selected = 0;
		view->selected_files = 0;

		draw_dir_list(view, view->top_line);
		move_to_list_pos(view, view->list_pos);
	}

	curr_stats.save_msg = save_msg;
	if(*mode == VISUAL_MODE)
		*mode = NORMAL_MODE;
}

static void
update_marks(FileView *view)
{
	if(start_pos >= view->list_rows)
		start_pos = view->list_rows - 1;
	upwards_range = view->list_pos < start_pos;
	if(upwards_range)
	{
		set_specmark('<', view->curr_dir, get_current_file_name(view));
		set_specmark('>', view->curr_dir, view->dir_entry[start_pos].name);
	}
	else
	{
		set_specmark('<', view->curr_dir, view->dir_entry[start_pos].name);
		set_specmark('>', view->curr_dir, get_current_file_name(view));
	}
}

static void
cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = incdec_names(view, key_info.count);
	leave_visual_mode(curr_stats.save_msg, 1, 1);
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	int s;
	int l = view->window_rows - 1;
	int pos;

	if(view->top_line == 0)
		return;

	pos = view->top_line + 1;
	view->top_line -= l;
	if(view->top_line < 0)
		view->top_line = 0;
	s = MIN((view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 &&
			view->top_line + view->window_rows - pos < s)
		pos -= s - (view->top_line + view->window_rows - pos);

	goto_pos(pos);
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	leave_visual_mode(0, 0, 1);
}

static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = view->list_pos;
	int s = MIN((view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 && view->list_pos - view->top_line < s)
		pos += s - (view->list_pos - view->top_line);
	view->top_line += (view->window_rows + 1)/2;
	goto_pos(pos + (view->window_rows + 1)/2);
}

static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	int off;

	if(view->list_rows <= view->window_rows + 1)
		return;
	if(view->top_line == view->list_rows - view->window_rows - 1)
		return;

	off = MAX(cfg.scroll_off, 0);
	if(view->list_pos <= view->top_line + off)
		goto_pos(view->top_line + 1 + off);
	view->top_line++;
	scroll_view(view);
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	int s;
	int l = view->window_rows - 1;
	int pos;

	if(view->top_line + 1 == view->list_rows - (l + 1))
		return;

	pos = view->top_line + l;
	view->top_line += l;
	if(view->top_line > view->list_rows)
		view->top_line = view->list_rows - l;
	s = MIN((view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 && pos - view->top_line < s)
		pos += s - (pos - view->top_line);

	goto_pos(pos);
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	redraw_window();
	curs_set(FALSE);
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	if(*mode == VISUAL_MODE)
		*mode = NORMAL_MODE;
}

static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = view->list_pos;
	int s = MIN((view->window_rows + 1)/2 - 1, cfg.scroll_off);
	if(cfg.scroll_off > 0 &&
			view->top_line + view->window_rows - view->list_pos < s)
		pos -= s - (view->top_line + view->window_rows - view->list_pos);

	view->top_line -= (view->window_rows + 1)/2;
	if(view->top_line < 0)
		view->top_line = 0;
	goto_pos(pos - (view->window_rows + 1)/2);
}

static void
cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = incdec_names(view, -key_info.count);
	leave_visual_mode(curr_stats.save_msg, 1, 1);
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	int off;

	if(view->list_rows <= view->window_rows + 1 || view->top_line == 0)
		return;

	off = MAX(cfg.scroll_off, 0);
	if(view->list_pos >= view->top_line + view->window_rows - off)
		goto_pos(view->top_line - 1 + view->window_rows - off);
	view->top_line--;
	scroll_view(view);
}

static void
cmd_C(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	curr_stats.save_msg = clone_files(view, NULL, 0, 0, key_info.count);
	leave_visual_mode(curr_stats.save_msg, 1, 1);
}

static void
cmd_D(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 0);
}

static void
cmd_F(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(key_info.multi, key_info.count, 1);
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = view->list_rows;
	goto_pos(key_info.count - 1);
}

static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	int off = MAX(cfg.scroll_off, 0);
	if(off > view->window_rows/2)
		return;
	if(view->top_line == 0)
		goto_pos(0);
	else
		goto_pos(view->top_line + off);
}

/* move to last line of window, selecting as we go */
static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	int off = MAX(cfg.scroll_off, 0);
	if(off > view->window_rows/2)
		return;
	if(view->top_line + view->window_rows < view->list_rows - 1)
		goto_pos(view->top_line + view->window_rows - off);
	else
		goto_pos(view->top_line + view->window_rows);
}

/* move to middle of window, selecting from start position to there */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	int pos1 = view->list_rows/2;
	int pos2 = view->top_line + view->window_rows/2;
	goto_pos(MIN(pos1, pos2));
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, !curr_stats.last_search_backward);
}

static void
cmd_O(key_info_t key_info, keys_info_t *keys_info)
{
	int t = start_pos;
	start_pos = view->list_pos;
	view->list_pos = t;
	update();
}

static void
cmd_quote(key_info_t key_info, keys_info_t *keys_info)
{
	int pos;
	pos = check_mark_directory(view, key_info.multi);
	if(pos < 0)
		return;
	goto_pos(pos);
}

static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
{
	int line;
	if(key_info.count == NO_COUNT_GIVEN)
		return;
	if(key_info.count > 100)
		return;
	line = (key_info.count * view->list_rows)/100;
	goto_pos(line - 1);
}

static void
cmd_comma(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward == -1)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(last_fast_search_char, key_info.count, !last_fast_search_backward);
}

static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	enter_cmdline_mode(CMD_SUBMODE, L"", NULL);
}

static void
cmd_semicolon(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward == -1)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(last_fast_search_char, key_info.count, last_fast_search_backward);
}

/* Search forward. */
static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	curr_stats.last_search_backward = 0;
	enter_cmdline_mode(VSEARCH_FORWARD_SUBMODE, L"", NULL);
}

/* Search backward. */
static void
cmd_question(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	curr_stats.last_search_backward = 1;
	enter_cmdline_mode(VSEARCH_BACKWARD_SUBMODE, L"", NULL);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 1);
}

static void
delete(key_info_t key_info, int use_trash)
{
	int save_msg;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

	curr_stats.confirmed = 0;
	if(!use_trash && cfg.confirm)
	{
		if(!query_user_menu("Permanent deletion",
				"Are you sure you want to delete files permanently?"))
			return;
		curr_stats.confirmed = 1;
	}

	save_msg = delete_file(view, key_info.reg, 0, NULL, use_trash);
	update_marks(view);
	leave_visual_mode(save_msg, 1, 1);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	find_goto(key_info.multi, key_info.count, 0);
}

/* Change file attributes (permissions or properties). */
static void
cmd_cp(key_info_t key_info, keys_info_t *keys_info)
{
	int ub;

	*mode = NORMAL_MODE;
	update_marks(view);
	ub = check_mark_directory(view, '<');
	if(ub != -1)
		view->list_pos = ub;

	enter_attr_mode(view);
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	goto_pos(key_info.count - 1);
}

static void
goto_pos(int pos)
{
	if(pos < 0)
		pos = 0;
	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(view->list_pos == pos)
		return;

	while(view->list_pos < pos)
		select_down_one(view, start_pos);
	while(view->list_pos > pos)
		select_up_one(view, start_pos);

	update();
}

static void
cmd_gU(key_info_t key_info, keys_info_t *keys_info)
{
	int save_msg;
	save_msg = change_case(view, 1, 0, NULL);
	update_marks(view);
	leave_visual_mode(save_msg, 1, 1);
}

static void
cmd_gu(key_info_t key_info, keys_info_t *keys_info)
{
	int save_msg;
	save_msg = change_case(view, 0, 0, NULL);
	update_marks(view);
	leave_visual_mode(save_msg, 1, 1);
}

static void
cmd_gv(key_info_t key_info, keys_info_t *keys_info)
{
	int x;
	int ub = check_mark_directory(view, '<');
	int lb = check_mark_directory(view, '>');

	if(ub < 0 || lb < 0)
		return;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;
	view->selected_files = 0;

	start_pos = ub;
	view->list_pos = ub;

	view->selected_files = 1;
	view->dir_entry[view->list_pos].selected = 1;

	while(view->list_pos != lb)
		select_down_one(view, start_pos);

	if(upwards_range)
	{
		int t = start_pos;
		start_pos = view->list_pos;
		view->list_pos = t;
	}

	update();
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	handle_file(curr_view, 1, 0);
	leave_visual_mode(curr_stats.save_msg, 1, 1);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	goto_pos(view->list_pos + key_info.count);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	goto_pos(view->list_pos - key_info.count);
}

static void
cmd_l(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	leave_visual_mode(curr_stats.save_msg, 1, 0);
	handle_file(view, 0, 0);
	clean_selected_files(view);
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
}

static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	add_bookmark(key_info.multi, view->curr_dir,
			get_current_file_name(view));
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, curr_stats.last_search_backward);
}

static void
search(key_info_t key_info, int backward)
{
	int found;
	if(cfg.search_history_num < 0)
		return;

	if(view->matches == 0)
	{
		const char *pattern = (view->regexp[0] == '\0') ?
				cfg.search_history[0] : view->regexp;
		curr_stats.save_msg = find_vpattern(view, pattern, backward);
	}

	if(view->matches == 0)
		return;

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	found = 0;
	while(key_info.count-- > 0)
		found += find_update(view, backward) != 0;

	if(!found)
	{
		if(backward)
			status_bar_errorf("Search hit TOP without match for: %s", view->regexp);
		else
			status_bar_errorf("Search hit BOTTOM without match for: %s",
					view->regexp);
		curr_stats.save_msg = 1;
		return;
	}

	status_bar_messagef("%c%s", backward ? '?' : '/', view->regexp);
	curr_stats.save_msg = 1;
}

static void
cmd_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	get_all_selected_files(view);
	yank_selected_files(view, key_info.reg);

	status_bar_messagef("%d file%s yanked", view->selected_files,
			(view->selected_files == 1) ? "" : "s");
	curr_stats.save_msg = 1;

	free_selected_file_array(view);

	update_marks(view);
	leave_visual_mode(1, 1, 1);
}

static void
cmd_zf(key_info_t key_info, keys_info_t *keys_info)
{
	filter_selected_files(view);
	leave_visual_mode(0, 1, 1);
}

static void
cmd_left_paren(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = cmd_paren(0, view->list_rows, -1);
	goto_pos(pos);
}

static void
cmd_right_paren(key_info_t key_info, keys_info_t *keys_info)
{
	int pos = cmd_paren(-1, view->list_rows - 1, +1);
	goto_pos(pos);
}

static void
find_goto(int ch, int count, int backward)
{
	int pos;
	pos = ffind(ch, backward, 1);
	if(pos < 0 || pos == view->list_pos)
		return;
	while(--count > 0)
	{
		goto_pos(pos);
		pos = ffind(ch, backward, 1);
	}
}

/* move up one position in the window, adding to the selection list */
static void
select_up_one(FileView *view, int start_pos)
{
	view->list_pos--;
	if(view->list_pos < 0)
	{
		if(start_pos == 0)
			view->selected_files = 0;
		view->list_pos = 0;
	}
	else if(view->list_pos == 0)
	{
		if(start_pos == 0)
		{
			view->dir_entry[view->list_pos + 1].selected = 0;
			view->selected_files = 0;
		}
	}
	else if(view->list_pos < start_pos)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files++;
	}
	else if(view->list_pos == start_pos)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->dir_entry[view->list_pos + 1].selected = 0;
		view->selected_files = 1;
	}
	else
	{
		view->dir_entry[view->list_pos + 1].selected = 0;
		view->selected_files--;
	}
}

/* move down one position in the window, adding to the selection list */
static void
select_down_one(FileView *view, int start_pos)
{
	view->list_pos++;

	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}
	else if(view->list_pos == 0)
	{
		if(start_pos == 0)
			view->selected_files = 0;
	}
	else if (view->list_pos == 1 && start_pos != 0)
	{
		return;
	}
	else if(view->list_pos > start_pos)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files++;
	}
	else if(view->list_pos == start_pos)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->dir_entry[view->list_pos - 1].selected = 0;
		view->selected_files = 1;
	}
	else
	{
		view->dir_entry[view->list_pos - 1].selected = 0;
		view->selected_files--;
	}
}

static void
update(void)
{
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
	update_pos_window(view);
}

int
find_vpattern(FileView *view, const char *pattern, int backward)
{
	int i;
	int result;
	int hls = cfg.hl_search;

	for(i = 0; i < view->list_rows; i++)
		view->dir_entry[i].selected = 0;

	cfg.hl_search = 0;
	result = find_pattern(view, pattern, backward, 0);
	cfg.hl_search = hls;
	for(i = 0; i < search_repeat; i++)
		find_update(view, backward);
	return result;
}

/* returns non-zero when it finds something */
static int
find_update(FileView *view, int backward)
{
	int found;
	int old_pos, new_pos;
	old_pos = view->list_pos;
	if(backward)
		found = find_previous_pattern(view, cfg.wrap_scan);
	else
		found = find_next_pattern(view, cfg.wrap_scan);
	new_pos = view->list_pos;
	view->list_pos = old_pos;
	goto_pos(new_pos);
	return found;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
