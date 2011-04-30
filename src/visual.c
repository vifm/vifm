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

#include "cmdline.h"
#include "commands.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "modes.h"
#include "status.h"

#include "visual.h"

static int *mode;
static FileView *view;
static int start_pos;

void leave_visual_mode(int save_msg);
static void init_extended_keys(void);
static void keys_ctrl_b(struct key_info, struct keys_info *);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_f(struct key_info, struct keys_info *);
static void keys_colon(struct key_info, struct keys_info *);
static void keys_D(struct key_info, struct keys_info *);
static void keys_G(struct key_info, struct keys_info *);
static void keys_H(struct key_info, struct keys_info *);
static void keys_L(struct key_info, struct keys_info *);
static void keys_M(struct key_info, struct keys_info *);
static void keys_d(struct key_info, struct keys_info *);
static void delete(struct key_info key_info, int use_trash);
static void keys_gg(struct key_info, struct keys_info *);
static void keys_j(struct key_info, struct keys_info *);
static void keys_k(struct key_info, struct keys_info *);
static void keys_y(struct key_info, struct keys_info *);
static void select_up_one(FileView *view, int start_pos);
static void select_down_one(FileView *view, int start_pos);
static void update(const char *operation);

void
init_visual_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_keys(L"\x02", VISUAL_MODE);
	curr->data.handler = keys_ctrl_b;

	curr = add_keys(L"\x03", VISUAL_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L"\x06", VISUAL_MODE);
	curr->data.handler = keys_ctrl_f;

	curr = add_keys(L"\x1b", VISUAL_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L":", VISUAL_MODE);
	curr->data.handler = keys_colon;

	curr = add_keys(L"D", VISUAL_MODE);
	curr->data.handler = keys_D;

	curr = add_keys(L"G", VISUAL_MODE);
	curr->data.handler = keys_G;
	curr->selector = KS_SELECTOR_AND_CMD;

	curr = add_keys(L"H", VISUAL_MODE);
	curr->data.handler = keys_H;
	curr->selector = KS_SELECTOR_AND_CMD;

	curr = add_keys(L"L", VISUAL_MODE);
	curr->data.handler = keys_L;
	curr->selector = KS_SELECTOR_AND_CMD;

	curr = add_keys(L"M", VISUAL_MODE);
	curr->data.handler = keys_M;
	curr->selector = KS_SELECTOR_AND_CMD;

	curr = add_keys(L"d", VISUAL_MODE);
	curr->data.handler = keys_d;

	curr = add_keys(L"gg", VISUAL_MODE);
	curr->data.handler = keys_gg;

	curr = add_keys(L"j", VISUAL_MODE);
	curr->data.handler = keys_j;

	curr = add_keys(L"k", VISUAL_MODE);
	curr->data.handler = keys_k;

	curr = add_keys(L"y", VISUAL_MODE);
	curr->data.handler = keys_y;

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
		status_bar_message(" 1 File Selected");
	else
		status_bar_message("");

	write_stat_win("  --VISUAL--");
	curr_stats.save_msg = 1;
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
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_PPAGE;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_ctrl_b;

	buf[0] = KEY_NPAGE;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_ctrl_f;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_j;

	buf[0] = KEY_UP;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_k;
}

static void
keys_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	int bound = view->top_line - view->window_rows - 1;
	while(view->list_pos > bound && view->list_pos > 0)
	{
		select_up_one(view, start_pos);
	}
	update("Selected");
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_visual_mode(0);
}

static void
keys_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	int bound = view->top_line + 2*view->window_rows + 1;
	while(view->list_pos < bound)
	{
		select_down_one(view, start_pos);
	}
	update("Selected");
}

static void
keys_D(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 0);
}

static void
keys_G(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos < view->list_rows)
	{
		select_down_one(view, start_pos);
	}
	update("Selected");
}

static void
keys_H(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos > view->top_line)
	{
		select_up_one(view, start_pos);
	}
	update("Selected");
}

/* move to last line of window, selecting as we go */
static void
keys_L(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos < view->top_line + view->window_rows)
	{
		select_down_one(view, start_pos);
	}
	update("Selected");
}

/* move to middle of window, selecting from start position to there */
static void
keys_M(struct key_info key_info, struct keys_info *keys_info)
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
				select_up_one(view,start_pos);
			}
		}
	}
	update("Selected");
}

static void
keys_colon(struct key_info key_info, struct keys_info *keys_info)
{
	enter_cmdline_mode(CMD_SUBMODE, L"'<,'>", NULL);
}

static void
keys_d(struct key_info key_info, struct keys_info *keys_info)
{
	delete(key_info, 1);
}

static void
delete(struct key_info key_info, int use_trash)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	delete_file(view, key_info.reg, 0, NULL, use_trash);
	leave_visual_mode(0);
}

static void
keys_gg(struct key_info key_info, struct keys_info *keys_info)
{
	while(view->list_pos > 0)
	{
		select_up_one(view, start_pos);
	}
	update("Selected");
}

static void
keys_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		select_down_one(view, start_pos);
	update("Selected");
}

static void
keys_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		select_up_one(view, start_pos);
	update("Selected");
}

static void
keys_y(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
		key_info.reg = DEFAULT_REG_NAME;
	get_all_selected_files(view);
	yank_selected_files(view, key_info.reg);
	update("Yanked");
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
}

static void
update(const char *operation)
{
	char status_buf[64] = "";

	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);
	update_pos_window(view);

	snprintf(status_buf, sizeof(status_buf), " %d %s %s", view->selected_files,
			view->selected_files == 1 ? "File" : "Files", operation);
	status_bar_message(status_buf);

	write_stat_win("  --VISUAL--");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
