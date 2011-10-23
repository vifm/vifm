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

#include <stdlib.h>

#include "background.h"
#include "change_dialog.h"
#include "cmdline.h"
#include "file_info.h"
#include "filelist.h"
#include "keys.h"
#include "macros.h"
#include "main_loop.h"
#include "menu.h"
#include "normal.h"
#include "permissions_dialog.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "view.h"
#include "visual.h"

#include "modes.h"

static int mode = NORMAL_MODE;

static int mode_flags[] = {
	MF_USES_COUNT | MF_USES_REGS, /* NORMAL_MODE */
	MF_USES_INPUT,                /* CMDLINE_MODE */
	MF_USES_COUNT | MF_USES_REGS, /* VISUAL_MODE */
	MF_USES_COUNT,                /* MENU_MODE */
	MF_USES_COUNT,                /* SORT_MODE */
	MF_USES_COUNT,                /* PERMISSIONS_MODE */
	MF_USES_COUNT,                /* CHANGE_MODE */
	MF_USES_COUNT,                /* VIEW_MODE */
};

static char _gnuc_unused mode_flags_size_guard[
	(ARRAY_LEN(mode_flags) == MODES_COUNT) ? 1 : -1
];

static char uses_input_bar[] = {
	1, /* NORMAL_MODE */
	0, /* CMDLINE_MODE */
	1, /* VISUAL_MODE */
	1, /* MENU_MODE */
	1, /* SORT_MODE */
	1, /* PERMISSIONS_MODE */
	1, /* CHANGE_MODE */
	1, /* VIEW_MODE */
};

static char _gnuc_unused uses_input_bar_size_guard[
	(ARRAY_LEN(uses_input_bar) == MODES_COUNT) ? 1 : -1
];

void
init_modes(void)
{
	init_keys(MODES_COUNT, &mode, (int*)&mode_flags);
	init_cmdline_mode(&mode);
	init_menu_mode(&mode);
	init_normal_mode(&mode);
	init_permissions_dialog_mode(&mode);
	init_sort_dialog_mode(&mode);
	init_visual_mode(&mode);
	init_change_dialog_mode(&mode);
	init_view_mode(&mode);
}

void
modes_pre(void)
{
	if(mode == CMDLINE_MODE)
	{
		touchwin(status_bar);
		wrefresh(status_bar);
		return;
	}
	else if(mode == SORT_MODE)
	{
		return;
	}
	else if(mode == CHANGE_MODE)
	{
		return;
	}
	else if(mode == PERMISSIONS_MODE)
	{
		return;
	}
	else if(mode == VIEW_MODE)
	{
		view_pre();
		return;
	}
	else if(mode == MENU_MODE)
	{
		menu_pre();
		return;
	}

	check_if_filelists_have_changed(curr_view);
	if(curr_stats.number_of_windows != 1 && !curr_stats.view)
		check_if_filelists_have_changed(other_view);

	check_background_jobs();

	if(!curr_stats.save_msg)
	{
		clean_status_bar();
		wrefresh(status_bar);
	}
}

void
modes_post(void)
{
	if(mode == CMDLINE_MODE)
		return;
	else if(mode == SORT_MODE)
		return;
	else if(mode == CHANGE_MODE)
		return;
	else if(mode == PERMISSIONS_MODE)
		return;
	else if(mode == VIEW_MODE)
	{
		view_post();
		return;
	}
	else if(mode == MENU_MODE)
	{
		menu_post();
		return;
	}

	if(curr_stats.need_redraw)
		redraw_window();

	if(curr_stats.save_msg)
		status_bar_message(NULL);

	if(curr_stats.show_full)
	{
		show_full_file_properties(curr_view);
	}
	else if(curr_view->list_rows > 0)
	{
		if(!is_status_bar_multiline())
		{
			update_stat_window(curr_view);
			update_pos_window(curr_view);
		}
	}

	if(curr_stats.save_msg)
		;
	else if(curr_view->selected_files || mode == VISUAL_MODE)
		print_selected_msg();
	else
		clean_status_bar();
}

void
modes_redraw(void)
{
	static int in_here;

	if(curr_stats.vifm_started < 2)
		return;

	if(in_here++ > 0)
		return;

	if(curr_stats.too_small_term)
	{
		redraw_window();
		if(--in_here > 0)
			modes_redraw();
		return;
	}

	if(mode == CMDLINE_MODE)
	{
		redraw_cmdline();
		if(--in_here > 0)
			modes_redraw();
		return;
	}
	else if(mode == MENU_MODE)
	{
		menu_redraw();
		if(--in_here > 0)
			modes_redraw();
		return;
	}

	redraw_window();

	if(curr_stats.save_msg)
		status_bar_message(NULL);

	if(mode == SORT_MODE)
		redraw_sort_dialog();
	else if(mode == CHANGE_MODE)
		redraw_change_dialog();
	else if(mode == PERMISSIONS_MODE)
		redraw_permissions_dialog();
	else if(mode == VIEW_MODE)
		view_redraw();

	curr_stats.vifm_started = 3;

	if(--in_here > 0)
		modes_redraw();
}

void
modes_update(void)
{
	if(mode == CMDLINE_MODE)
	{
		redraw_cmdline();
		return;
	}
	else if(mode == MENU_MODE)
	{
		menu_redraw();
		return;
	}

	touchwin(stdscr);
	update_all_windows();

	if(mode == SORT_MODE)
		redraw_sort_dialog();
	else if(mode == PERMISSIONS_MODE)
		redraw_permissions_dialog();
}

void
modupd_input_bar(wchar_t *str)
{
	if(mode == VISUAL_MODE)
		clear_input_bar();

	if(uses_input_bar[mode])
		update_input_bar(str);
}

void
clear_input_bar(void)
{
	if(uses_input_bar[mode] && mode != VISUAL_MODE)
		clear_num_window();
}

int
get_mode(void)
{
	return mode;
}

void
print_selected_msg(void)
{
	if(mode == VISUAL_MODE)
	{
		status_bar_message("-- VISUAL -- ");
		if(is_input_buf_empty())
		{
			werase(input_win);
			wmove(input_win, 0, 0);
			wprintw(input_win, "%d", curr_view->selected_files);
			wrefresh(input_win);
		}
	}
	else
	{
		status_bar_messagef("%d %s selected", curr_view->selected_files,
				curr_view->selected_files == 1 ? "file" : "files");
	}
	curr_stats.save_msg = 2;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
