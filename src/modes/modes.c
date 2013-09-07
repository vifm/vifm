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

#include "modes.h"

#include <assert.h> /* assert() */
#include <stdlib.h>

#include "../engine/keys.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../background.h"
#include "../filelist.h"
#include "../main_loop.h"
#include "../status.h"
#include "../ui.h"
#include "dialogs/attr_dialog.h"
#include "dialogs/change_dialog.h"
#include "dialogs/sort_dialog.h"
#include "cmdline.h"
#include "file_info.h"
#include "menu.h"
#include "normal.h"
#include "view.h"
#include "visual.h"

static int mode = NORMAL_MODE;

static int mode_flags[] = {
	MF_USES_COUNT | MF_USES_REGS, /* NORMAL_MODE */
	MF_USES_INPUT,                /* CMDLINE_MODE */
	MF_USES_COUNT | MF_USES_REGS, /* VISUAL_MODE */
	MF_USES_COUNT,                /* MENU_MODE */
	MF_USES_COUNT,                /* SORT_MODE */
	MF_USES_COUNT,                /* ATTR_MODE */
	MF_USES_COUNT,                /* CHANGE_MODE */
	MF_USES_COUNT,                /* VIEW_MODE */
	0,                            /* FILE_INFO_MODE */
};
ARRAY_GUARD(mode_flags, MODES_COUNT);

static char uses_input_bar[] = {
	1, /* NORMAL_MODE */
	0, /* CMDLINE_MODE */
	1, /* VISUAL_MODE */
	1, /* MENU_MODE */
	1, /* SORT_MODE */
	1, /* ATTR_MODE */
	1, /* CHANGE_MODE */
	1, /* VIEW_MODE */
	1, /* FILE_INFO_MODE */
};
ARRAY_GUARD(uses_input_bar, MODES_COUNT);

typedef void (*mode_init_func)(int *mode);
static mode_init_func mode_init_funcs[] = {
	&init_normal_mode,        /* NORMAL_MODE */
	&init_cmdline_mode,       /* CMDLINE_MODE */
	&init_visual_mode,        /* VISUAL_MODE */
	&init_menu_mode,          /* MENU_MODE */
	&init_sort_dialog_mode,   /* SORT_MODE */
	&init_attr_dialog_mode,   /* ATTR_MODE */
	&init_change_dialog_mode, /* CHANGE_MODE */
	&init_view_mode,          /* VIEW_MODE */
	&init_file_info_mode,     /* FILE_INFO_MODE */
};
ARRAY_GUARD(mode_init_funcs, MODES_COUNT);

static void modes_statusbar_update(void);
static void update_vmode_input(void);

void
init_modes(void)
{
	LOG_FUNC_ENTER;

	int i;

	init_keys(MODES_COUNT, &mode, (int*)&mode_flags);

	for(i = 0; i < MODES_COUNT; i++)
		mode_init_funcs[i](&mode);
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
	else if(mode == ATTR_MODE)
	{
		return;
	}
	else if(mode == VIEW_MODE)
	{
		view_pre();
		return;
	}
	else if(is_in_menu_like_mode())
	{
		menu_pre();
		return;
	}

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
	else if(mode == ATTR_MODE)
		return;
	else if(mode == VIEW_MODE)
	{
		view_post();
		return;
	}
	else if(is_in_menu_like_mode())
	{
		menu_post();
		return;
	}

	update_screen(curr_stats.need_update);

	if(curr_stats.save_msg)
		status_bar_message(NULL);

	if(mode != FILE_INFO_MODE && curr_view->list_rows > 0)
	{
		if(!is_status_bar_multiline())
		{
			update_stat_window(curr_view);
			update_pos_window(curr_view);
		}
	}

	modes_statusbar_update();
}

void
modes_statusbar_update(void)
{
	if(curr_stats.save_msg)
	{
		if(mode == VISUAL_MODE)
		{
			update_vmode_input();
		}
	}
	else if(curr_view->selected_files || mode == VISUAL_MODE)
		print_selected_msg();
	else
		clean_status_bar();
}

void
modes_redraw(void)
{
	LOG_FUNC_ENTER;

	static int in_here;

	if(curr_stats.load_stage < 2)
		return;

	if(in_here++ > 0)
		return;

	if(curr_stats.too_small_term)
	{
		update_screen(UT_REDRAW);
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
	else if(mode == FILE_INFO_MODE)
	{
		redraw_file_info_dialog();
		if(--in_here > 0)
			modes_redraw();
		return;
	}

	update_screen(UT_REDRAW);

	if(curr_stats.save_msg)
		status_bar_message(NULL);

	if(mode == SORT_MODE)
		redraw_sort_dialog();
	else if(mode == CHANGE_MODE)
		redraw_change_dialog();
	else if(mode == ATTR_MODE)
		redraw_attr_dialog();
	else if(mode == VIEW_MODE)
		view_redraw();

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
	else if(mode == FILE_INFO_MODE)
	{
		redraw_file_info_dialog();
		return;
	}

	touchwin(stdscr);
	update_all_windows();

	if(mode == SORT_MODE)
		redraw_sort_dialog();
	else if(mode == CHANGE_MODE)
		redraw_change_dialog();
	else if(mode == ATTR_MODE)
		redraw_attr_dialog();
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

int
is_in_menu_like_mode(void)
{
	return mode == MENU_MODE || mode == FILE_INFO_MODE;
}

void
print_selected_msg(void)
{
	if(mode == VISUAL_MODE)
	{
		status_bar_message("-- VISUAL -- ");
		update_vmode_input();
	}
	else
	{
		status_bar_messagef("%d %s selected", curr_view->selected_files,
				curr_view->selected_files == 1 ? "file" : "files");
	}
	curr_stats.save_msg = 2;
}

static void
update_vmode_input(void)
{
	if(is_input_buf_empty())
	{
		werase(input_win);
		checked_wmove(input_win, 0, 0);
		wprintw(input_win, "%d", curr_view->selected_files);
		wrefresh(input_win);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
