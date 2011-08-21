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
#include "cmdline.h"
#include "file_info.h"
#include "filelist.h"
#include "keys.h"
#include "menu.h"
#include "normal.h"
#include "permissions_dialog.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "visual.h"

#include "modes.h"

static int mode = NORMAL_MODE;

static int mode_flags[] = {
	MF_USES_COUNT | MF_USES_REGS, // NORMAL_MODE
	MF_USES_INPUT,                // CMDLINE_MODE
	MF_USES_COUNT | MF_USES_REGS, // VISUAL_MODE
	MF_USES_COUNT,                // MENU_MODE
	MF_USES_COUNT,                // SORT_MODE
	MF_USES_COUNT                 // PERMISSIONS_MODE
};

static char uses_input_bar[] = {
	1, // NORMAL_MODE
	0, // CMDLINE_MODE
	1, // VISUAL_MODE
	1, // MENU_MODE
	1, // SORT_MODE
	1  // PERMISSIONS_MODE
};

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
		return;
	else if(mode == PERMISSIONS_MODE)
		return;
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

	update_all_windows();
}

void
modes_post(void)
{
	if(mode == CMDLINE_MODE)
		return;
	else if(mode == SORT_MODE)
		return;
	else if(mode == PERMISSIONS_MODE)
		return;
	else if(mode == MENU_MODE)
	{
		menu_post();
		return;
	}

	if(curr_stats.need_redraw)
		redraw_window();

	if(curr_stats.save_msg)
	{
		status_bar_message(NULL);
		wrefresh(status_bar);
	}

	if(curr_stats.show_full)
	{
		show_full_file_properties(curr_view);
	}
	else if(curr_view->list_rows > 0)
	{
		update_stat_window(curr_view);
		if(!is_status_bar_multiline())
			update_pos_window(curr_view);
	}

	if(curr_stats.save_msg)
		;
	else if(curr_view->selected_files || mode == VISUAL_MODE)
	{
		status_bar_messagef("%s%d %s selected",
				(mode == VISUAL_MODE) ? "-- VISUAL -- " : "", curr_view->selected_files,
				curr_view->selected_files == 1 ? "file" : "files");
		curr_stats.save_msg = 1;
	}
	else
		clean_status_bar();
}

void
modes_redraw(void)
{
	static int in_here;

	if(in_here++ > 0)
		return;

	if(curr_stats.too_small_term)
	{
		redraw_window();
		in_here--;
		return;
	}

	if(mode == CMDLINE_MODE)
	{
		redraw_cmdline();
		in_here--;
		return;
	}
	else if(mode == MENU_MODE)
	{
		menu_redraw();
		in_here--;
		return;
	}

	redraw_window();

	if(mode == SORT_MODE)
		redraw_sort_dialog();
	else if(mode == PERMISSIONS_MODE)
		redraw_permissions_dialog();

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
add_to_input_bar(wchar_t c)
{
	if(uses_input_bar[mode])
		update_input_bar(c);
}

void
clear_input_bar(void)
{
	if(uses_input_bar[mode])
		clear_num_window();
}

int
get_mode(void)
{
	return mode;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
