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
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "modes.h"
#include "permissions_dialog.h"
#include "status.h"

#include "visual.h"

static int *mode;
static FileView *view;
static int start_pos;

static void init_extended_keys(void);
static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_colon(struct key_info, struct keys_info *);
static void cmd_D(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_H(struct key_info, struct keys_info *);
static void cmd_L(struct key_info, struct keys_info *);
static void cmd_M(struct key_info, struct keys_info *);
static void cmd_O(struct key_info, struct keys_info *);
static void cmd_d(struct key_info, struct keys_info *);
static void delete(struct key_info key_info, int use_trash);
static void cmd_cp(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_y(struct key_info, struct keys_info *);
static void select_up_one(FileView *view, int start_pos);
static void select_down_one(FileView *view, int start_pos);
static void update_marks(FileView *view);
static void update(void);

void
init_visual_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_cmd(L"\x02", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_b;

	curr = add_cmd(L"\x03", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"\x06", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_f;

	curr = add_cmd(L"\x1b", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L":", VISUAL_MODE);
	curr->data.handler = cmd_colon;

	curr = add_cmd(L"D", VISUAL_MODE);
	curr->data.handler = cmd_D;

	curr = add_cmd(L"G", VISUAL_MODE);
	curr->data.handler = cmd_G;

	curr = add_cmd(L"H", VISUAL_MODE);
	curr->data.handler = cmd_H;

	curr = add_cmd(L"L", VISUAL_MODE);
	curr->data.handler = cmd_L;

	curr = add_cmd(L"M", VISUAL_MODE);
	curr->data.handler = cmd_M;

	curr = add_cmd(L"O", VISUAL_MODE);
	curr->data.handler = cmd_O;

	curr = add_cmd(L"V", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"d", VISUAL_MODE);
	curr->data.handler = cmd_d;

	curr = add_cmd(L"cp", VISUAL_MODE);
	curr->data.handler = cmd_cp;

	curr = add_cmd(L"gg", VISUAL_MODE);
	curr->data.handler = cmd_gg;

	curr = add_cmd(L"j", VISUAL_MODE);
	curr->data.handler = cmd_j;

	curr = add_cmd(L"k", VISUAL_MODE);
	curr->data.handler = cmd_k;

	curr = add_cmd(L"o", VISUAL_MODE);
	curr->data.handler = cmd_O;

	curr = add_cmd(L"v", VISUAL_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"y", VISUAL_MODE);
	curr->data.handler = cmd_y;

	init_extended_keys();
}

void
enter_visual_mode(void)
{
	int x;

	view = curr_view;
	start_pos = view->list_pos;
	*mode = VISUAL_MODE;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;

	/* Don't allow the ../ dir to be selected */
	if(strcmp(view->dir_entry[view->list_pos].name, "../"))
	{
		view->selected_files = 1;
		view->dir_entry[view->list_pos].selected = 1;
	}

	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);

	if(view->list_pos != 0)
		status_bar_message("-- VISUAL --\t1 File Selected");
	else
		status_bar_message("-- VISUAL --");
	curr_stats.save_msg = 1;

	update_marks(view);
}

void
leave_visual_mode(int save_msg)
{
	clean_selected_files(view);
	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);

	curr_stats.save_msg = save_msg;
	if(*mode == VISUAL_MODE)
		*mode = NORMAL_MODE;
}

static void
init_extended_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_PPAGE;
	curr = add_cmd(buf, VISUAL_MODE);
	curr->data.handler = cmd_ctrl_b;

	buf[0] = KEY_NPAGE;
	curr = add_cmd(buf, VISUAL_MODE);
	curr->data.handler = cmd_ctrl_f;

	buf[0] = KEY_DOWN;
	curr = add_cmd(buf, VISUAL_MODE);
	curr->data.handler = cmd_j;

	buf[0] = KEY_UP;
	curr = add_cmd(buf, VISUAL_MODE);
	curr->data.handler = cmd_k;
#endif /* ENABLE_EXTENDED_KEYS */
}

static void
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	int bound = view->top_line - view->window_rows - 1;
	while(view->list_pos > bound && view->list_pos > 0)
	{
		select_up_one(view, start_pos);
	}
	update();
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_visual_mode(0);
}

static void
cmd_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	int bound = view->top_line + 2*view->window_rows + 1;
	while(view->list_pos < bound)
	{
		select_down_one(view, start_pos);
	}
	update();
}

static void
cmd_D(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 0);
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos < view->list_rows)
	{
		select_down_one(view, start_pos);
	}
	update();
}

static void
cmd_H(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos > view->top_line)
	{
		select_up_one(view, start_pos);
	}
	update();
}

/* move to last line of window, selecting as we go */
static void
cmd_L(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos < view->top_line + view->window_rows)
	{
		select_down_one(view, start_pos);
	}
	update();
}

/* move to middle of window, selecting from start position to there */
static void
cmd_M(struct key_info key_info, struct keys_info *keys_info)
{
	/*window smaller than file list */
	if(view->list_rows < view->window_rows)
	{
		/*in upper portion */
		if(view->list_pos < view->list_rows/2)
		{
			while(view->list_pos < view->list_rows/2)
			{
				select_down_one(view,start_pos);
			}
		}
		/* lower portion */
		else if(view->list_pos > view->list_rows/2)
		{
			while(view->list_pos > view->list_rows/2)
			{
				select_up_one(view,start_pos);
			}
		}
	}
	/* window larger than file list */
	else
	{
		/* top half */
		if(view->list_pos < view->top_line + view->window_rows/2)
		{
			while(view->list_pos < view->top_line + view->window_rows/2)
			{
				select_down_one(view,start_pos);
			}
		}
		/* bottom half */
		else if(view->list_pos > view->top_line + view->window_rows/2)
		{
			while(view->list_pos > view->top_line + view->window_rows/2)
			{
				select_up_one(view, start_pos);
			}
		}
	}
	update();
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
cmd_colon(struct key_info key_info, struct keys_info *keys_info)
{
	enter_cmdline_mode(CMD_SUBMODE, L"'<,'>", NULL);
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

	save_msg = delete_file(view, key_info.reg, 0, NULL, use_trash);
	leave_visual_mode(save_msg);
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
	while(view->list_pos > 0)
	{
		select_up_one(view, start_pos);
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

/* move up one position in the window, adding to the selection list */
static void
select_up_one(FileView *view, int start_pos)
{
	view->list_pos--;
	if(view->list_pos < 0)
	{
		if (start_pos == 0)
			view->selected_files = 0;
		view->list_pos = 0;
	}
	else if(view->list_pos == 0)
	{
		if(start_pos == 0)
		{
			view->dir_entry[view->list_pos +1].selected = 0;
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

	update_marks(view);
}

/* move down one position in the window, adding to the selection list */
static void
select_down_one(FileView *view, int start_pos)
{
	view->list_pos++;

	if(view->list_pos >= view->list_rows)
		;
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

	update_marks(view);
}

static void
update_marks(FileView *view)
{
	if(view->list_pos < start_pos)
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
update(void)
{
	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);
	update_pos_window(view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
