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
#include <string.h>

#include "../config.h"

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
#include "permissions_dialog.h"
#include "status.h"

#include "visual.h"

static int *mode;
static FileView *view;
static int start_pos;
static int last_fast_search_char;
static int last_fast_search_backward = -1;
static int upwards_range;

static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_d(struct key_info, struct keys_info *);
static void cmd_ctrl_e(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_ctrl_u(struct key_info, struct keys_info *);
static void cmd_ctrl_y(struct key_info, struct keys_info *);
static void cmd_quote(struct key_info, struct keys_info *);
static void cmd_percent(struct key_info, struct keys_info *);
static void cmd_comma(struct key_info, struct keys_info *);
static void cmd_colon(struct key_info, struct keys_info *);
static void cmd_semicolon(struct key_info, struct keys_info *);
static void cmd_D(struct key_info, struct keys_info *);
static void cmd_F(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_H(struct key_info, struct keys_info *);
static void cmd_L(struct key_info, struct keys_info *);
static void cmd_M(struct key_info, struct keys_info *);
static void cmd_O(struct key_info, struct keys_info *);
static void cmd_d(struct key_info, struct keys_info *);
static void delete(struct key_info key_info, int use_trash);
static void cmd_cp(struct key_info, struct keys_info *);
static void cmd_f(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void goto_pos(int pos);
static void cmd_gv(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_y(struct key_info, struct keys_info *);
static void cmd_zf(struct key_info, struct keys_info *);
static void find_goto(int ch, int backward);
static void select_up_one(FileView *view, int start_pos);
static void select_down_one(FileView *view, int start_pos);
static void update_marks(FileView *view);
static void update(void);

static struct keys_add_info builtin_cmds[] = {
	{L"\x02", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x0d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x15", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x19", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
	/* escape */
	{L"\x1b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"'", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_quote}}},
	{L"%", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_percent}}},
	{L",", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_comma}}},
	{L":", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L";", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_semicolon}}},
	{L"D", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_D}}},
	{L"F", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_F}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"O", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_O}}},
	{L"V", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_d}}},
	{L"f", {BUILDIN_WAIT_POINT, FOLLOWED_BY_MULTIKEY, {.handler = cmd_f}}},
	{L"cp", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_cp}}},
	{L"cw", {BUILDIN_CMD, FOLLOWED_BY_NONE, {.cmd = L":rename\r"}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"gv", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gv}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"o", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_O}}},
	{L"v", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"y", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_y}}},
	{L"zb", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zb}}},
	{L"zf", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zf}}},
	{L"zt", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zt}}},
	{L"zz", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = normal_cmd_zz}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_visual_mode(int *key_mode)
{
	assert(key_mode != NULL);

	mode = key_mode;

	assert(add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), VISUAL_MODE) == 0);
}

void
enter_visual_mode(int restore_selection)
{
	int x;
	int ub = check_mark_directory(view, '<');
	int lb = check_mark_directory(view, '>');

	if(restore_selection && (ub < 0 || lb < 0))
		return;

	view = curr_view;
	start_pos = view->list_pos;
	*mode = VISUAL_MODE;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;

	if(restore_selection)
	{
		struct key_info ki;
		cmd_gv(ki, NULL);
	}
	else if(strcmp(view->dir_entry[view->list_pos].name, "../"))
	{
		/* Don't allow the ../ dir to be selected */
		view->selected_files = 1;
		view->dir_entry[view->list_pos].selected = 1;
	}

	draw_dir_list(view, view->top_line);
	moveto_list_pos(view, view->list_pos);
}

void
leave_visual_mode(int save_msg)
{
	clean_selected_files(view);
	draw_dir_list(view, view->top_line);
	moveto_list_pos(view, view->list_pos);

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
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(view->top_line - view->window_rows - 1);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	update_marks(view);
	leave_visual_mode(0);
}

static void
cmd_ctrl_d(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(curr_view->list_pos + curr_view->window_rows/2);
}

static void
cmd_ctrl_e(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1)
		return;
	if(curr_view->top_line == curr_view->list_rows - curr_view->window_rows - 1)
		return;
	if(curr_view->list_pos == curr_view->top_line)
		goto_pos(curr_view->list_pos + 1);
	curr_view->top_line++;
	scroll_view(curr_view);
}

static void
cmd_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(view->top_line + 2*view->window_rows + 1);
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	update_marks(view);
	if(*mode == VISUAL_MODE)
		*mode = NORMAL_MODE;
}

static void
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(curr_view->list_pos - curr_view->window_rows/2);
}

static void
cmd_ctrl_y(struct key_info key_info, struct keys_info *keys_info)
{
	if(curr_view->list_rows <= curr_view->window_rows + 1
			|| curr_view->top_line == 0)
		return;
	if(curr_view->list_pos == curr_view->top_line + curr_view->window_rows)
		goto_pos(curr_view->list_pos - 1);
	curr_view->top_line--;
	scroll_view(curr_view);
}

static void
cmd_D(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 0);
}

static void
cmd_F(struct key_info key_info, struct keys_info *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;
	find_goto(key_info.multi, 1);
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = curr_view->list_rows;
	goto_pos(key_info.count - 1);
}

static void
cmd_H(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(view->top_line);
}

/* move to last line of window, selecting as we go */
static void
cmd_L(struct key_info key_info, struct keys_info *keys_info)
{
	goto_pos(view->top_line + view->window_rows);
}

/* move to middle of window, selecting from start position to there */
static void
cmd_M(struct key_info key_info, struct keys_info *keys_info)
{
	int pos1 = view->list_rows/2;
	int pos2 = view->top_line + view->window_rows/2;
	goto_pos(MIN(pos1, pos2));
}

static void
cmd_O(struct key_info key_info, struct keys_info *keys_info)
{
	int t = start_pos;
	start_pos = view->list_pos;
	view->list_pos = t;
	update();
}

static void
cmd_quote(struct key_info key_info, struct keys_info *keys_info)
{
	int pos;
	pos = check_mark_directory(curr_view, key_info.multi);
	if(pos < 0)
		return;
	goto_pos(pos);
}

static void
cmd_percent(struct key_info key_info, struct keys_info *keys_info)
{
	int line;
	if(key_info.count == NO_COUNT_GIVEN)
		return;
	if(key_info.count > 100)
		return;
	line = (key_info.count * curr_view->list_rows)/100;
	goto_pos(line - 1);
}

static void
cmd_comma(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;
	find_goto(last_fast_search_char, !last_fast_search_backward);
}

static void
cmd_colon(struct key_info key_info, struct keys_info *keys_info)
{
	update_marks(view);
	enter_cmdline_mode(CMD_SUBMODE, L"'<,'>", NULL);
}

static void
cmd_semicolon(struct key_info key_info, struct keys_info *keys_info)
{
	if(last_fast_search_backward == -1)
		return;
	find_goto(last_fast_search_char, last_fast_search_backward);
}

static void
cmd_d(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 1);
}

static void
delete(struct key_info key_info, int use_trash)
{
	int save_msg;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;

	if(!use_trash && cfg.confirm)
	{
		if(!query_user_menu("Permanent deletion",
				"Are you sure you want to delete files permanently?"))
			return;
	}

	save_msg = delete_file(view, key_info.reg, 0, NULL, use_trash);
	leave_visual_mode(save_msg);
}

static void
cmd_f(struct key_info key_info, struct keys_info *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;
	find_goto(key_info.multi, 0);
}

/* Change permissions. */
static void
cmd_cp(struct key_info key_info, struct keys_info *keys_info)
{
	enter_permissions_mode(curr_view);
}

static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
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
	if(pos > curr_view->list_rows - 1)
		pos = curr_view->list_rows - 1;

	while(view->list_pos < pos)
		select_down_one(view, start_pos);
	while(view->list_pos > pos)
		select_up_one(view, start_pos);

	update();
}

static void
cmd_gv(struct key_info key_info, struct keys_info *keys_info)
{
	int x;
	int ub = check_mark_directory(view, '<');
	int lb = check_mark_directory(view, '>');

	if(ub < 0 || lb < 0)
		return;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;

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
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		select_down_one(view, start_pos);
	update();
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		select_up_one(view, start_pos);
	update();
}

static void
cmd_y(struct key_info key_info, struct keys_info *keys_info)
{
	char status_buf[64] = "";

	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	get_all_selected_files(view);
	yank_selected_files(view, key_info.reg);

	snprintf(status_buf, sizeof(status_buf), "%d %s Yanked", view->selected_files,
			view->selected_files == 1 ? "File" : "Files");
	status_bar_message(status_buf);
	curr_stats.save_msg = 1;

	free_selected_file_array(view);

	leave_visual_mode(1);
}

static void
cmd_zf(struct key_info key_info, struct keys_info *keys_info)
{
	filter_selected_files(curr_view);
	leave_visual_mode(0);
}

static void
find_goto(int ch, int backward)
{
	int pos;
	pos = ffind(ch, backward, 1);
	if(pos < 0 || pos == curr_view->list_pos)
		return;

	goto_pos(pos);
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
		view->dir_entry[view->list_pos +1].selected = 0;
		view->selected_files = 1;
	}
	else
	{
		view->dir_entry[view->list_pos +1].selected = 0;
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
		if (start_pos == 0)
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
		view->dir_entry[view->list_pos -1].selected = 0;
		view->selected_files = 1;
	}
	else
	{
		view->dir_entry[view->list_pos -1].selected = 0;
		view->selected_files--;
	}
}

static void
update(void)
{
	draw_dir_list(view, view->top_line);
	moveto_list_pos(view, view->list_pos);
	update_pos_window(view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
