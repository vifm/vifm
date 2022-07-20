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

#include <curses.h>

#include <stddef.h> /* wchar_t */

#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../ui/statusbar.h"
#include "../ui/statusline.h"
#include "../ui/ui.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../event_loop.h"
#include "../status.h"
#include "dialogs/attr_dialog.h"
#include "dialogs/change_dialog.h"
#include "dialogs/msg_dialog.h"
#include "dialogs/sort_dialog.h"
#include "cmdline.h"
#include "file_info.h"
#include "menu.h"
#include "more.h"
#include "normal.h"
#include "view.h"
#include "visual.h"

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
	0,                            /* MSG_MODE */
	0,                            /* MORE_MODE */
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
	0, /* MSG_MODE */
	0, /* MORE_MODE */
};
ARRAY_GUARD(uses_input_bar, MODES_COUNT);

typedef void (*mode_init_func)(void);
static mode_init_func mode_init_funcs[] = {
	&modnorm_init,            /* NORMAL_MODE */
	&modcline_init,           /* CMDLINE_MODE */
	&modvis_init,             /* VISUAL_MODE */
	&modmenu_init,            /* MENU_MODE */
	&init_sort_dialog_mode,   /* SORT_MODE */
	&init_attr_dialog_mode,   /* ATTR_MODE */
	&init_change_dialog_mode, /* CHANGE_MODE */
	&modview_init,            /* VIEW_MODE */
	&modfinfo_init,           /* FILE_INFO_MODE */
	&init_msg_dialog_mode,    /* MSG_MODE */
	&modmore_init,            /* MORE_MODE */
};
ARRAY_GUARD(mode_init_funcs, MODES_COUNT);

static void modes_statusbar_update(void);
static void update_vmode_input(void);

void
init_modes(void)
{
	LOG_FUNC_ENTER;

	vle_keys_init(MODES_COUNT, mode_flags, &stats_silence_ui);

	int i;
	for(i = 0; i < MODES_COUNT; ++i)
	{
		mode_init_funcs[i]();
	}
}

void
modes_pre(void)
{
	if(ANY(vle_mode_is, SORT_MODE, CHANGE_MODE, ATTR_MODE, MORE_MODE))
	{
		/* Do nothing for these modes. */
	}
	else if(vle_mode_is(CMDLINE_MODE))
	{
		touchwin(status_bar);
		ui_refresh_win(status_bar);
	}
	else if(vle_mode_is(VIEW_MODE))
	{
		modview_pre();
	}
	else if(is_in_menu_like_mode())
	{
		modmenu_pre();
	}
	else if(!curr_stats.save_msg)
	{
		ui_sb_clear();
		ui_refresh_win(status_bar);
	}
}

void
modes_periodic(void)
{
	/* Trigger possible view updates. */
	modview_check_for_updates();
}

void
modes_post(void)
{
	if(ANY(vle_mode_is,
				CMDLINE_MODE, SORT_MODE, CHANGE_MODE, ATTR_MODE, MORE_MODE))
	{
		/* Do nothing for these modes. */
		return;
	}
	else if(vle_mode_is(VIEW_MODE))
	{
		modview_post();
		return;
	}
	else if(is_in_menu_like_mode())
	{
		modmenu_post();
		return;
	}

	update_screen(stats_update_fetch());

	if(curr_stats.save_msg)
	{
		ui_sb_msg(NULL);
	}

	if(!vle_mode_is(FILE_INFO_MODE) && curr_view->list_rows > 0)
	{
		if(!ui_sb_multiline())
		{
			ui_stat_update(curr_view, 0);
			ui_ruler_update(curr_view, 1);
		}
	}

	modes_statusbar_update();
}

void
modes_statusbar_update(void)
{
	if(vle_mode_is(MORE_MODE))
	{
		/* Status bar is used for special purposes. */
	}
	else if(!curr_stats.save_msg &&
			(curr_view->selected_files || vle_mode_is(VISUAL_MODE)))
	{
		print_selected_msg();
	}
	else if(!curr_stats.save_msg)
	{
		ui_sb_clear();
	}
}

void
modes_redraw(void)
{
	LOG_FUNC_ENTER;

	static int in_here;

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(in_here++ > 0)
	{
		/* TODO: is this still needed?  Update scheduling might have solved issues
		 * caused by asynchronous execution of this function in the past. */
		return;
	}

	if(curr_stats.term_state != TS_NORMAL)
	{
		update_screen(UT_REDRAW);
		goto finish;
	}

	if(vle_mode_is(CMDLINE_MODE))
	{
		modcline_redraw();
		goto finish;
	}
	else if(vle_primary_mode_is(MENU_MODE))
	{
		modmenu_full_redraw();
		if(vle_mode_is(MSG_MODE))
		{
			redraw_msg_dialog(0);
		}
		goto finish;
	}
	else if(vle_mode_is(FILE_INFO_MODE))
	{
		modfinfo_redraw();
		goto finish;
	}
	else if(vle_mode_is(MORE_MODE))
	{
		modmore_redraw();
		goto finish;
	}

	UpdateType update = stats_update_fetch();
	if(update == UT_NONE)
	{
		update_screen(UT_REDRAW);
	}
	else
	{
		update_screen(update);
	}

	if(curr_stats.save_msg)
	{
		ui_sb_msg(NULL);
	}

	if(vle_mode_is(SORT_MODE))
	{
		redraw_sort_dialog();
	}
	else if(vle_mode_is(CHANGE_MODE))
	{
		redraw_change_dialog();
	}
	else if(vle_mode_is(ATTR_MODE))
	{
		redraw_attr_dialog();
	}
	else if(vle_mode_is(VIEW_MODE))
	{
		modview_redraw();
	}
	else if(vle_mode_is(MSG_MODE))
	{
		redraw_msg_dialog(0);
	}

finish:
	if(--in_here > 0)
	{
		modes_redraw();
	}
}

void
modes_update(void)
{
	if(vle_mode_is(CMDLINE_MODE))
	{
		modcline_redraw();
		return;
	}
	else if(vle_mode_is(MENU_MODE))
	{
		modmenu_full_redraw();
		return;
	}
	else if(vle_mode_is(FILE_INFO_MODE))
	{
		modfinfo_redraw();
		return;
	}

	touchwin(stdscr);
	update_all_windows();

	if(vle_mode_is(SORT_MODE))
	{
		redraw_sort_dialog();
	}
	else if(vle_mode_is(CHANGE_MODE))
	{
		redraw_change_dialog();
	}
	else if(vle_mode_is(ATTR_MODE))
	{
		redraw_attr_dialog();
	}
}

void
modupd_input_bar(wchar_t *str)
{
	if(vle_mode_is(VISUAL_MODE))
	{
		clear_input_bar();
	}

	if(uses_input_bar[vle_mode_get()])
	{
		update_input_bar(str);
	}
}

void
clear_input_bar(void)
{
	if(uses_input_bar[vle_mode_get()] && !vle_mode_is(VISUAL_MODE))
	{
		clear_num_window();
	}
}

int
is_in_menu_like_mode(void)
{
	return ANY(vle_primary_mode_is, MENU_MODE, FILE_INFO_MODE, MORE_MODE);
}

int
modes_is_dialog_like(void)
{
	return ANY(vle_mode_is, SORT_MODE, ATTR_MODE, CHANGE_MODE, MSG_MODE);
}

void
abort_menu_like_mode(void)
{
	if(vle_primary_mode_is(MENU_MODE))
	{
		modmenu_abort();
	}
	else if(vle_primary_mode_is(FILE_INFO_MODE))
	{
		modfinfo_abort();
	}
	else if(vle_primary_mode_is(MORE_MODE))
	{
		modmore_abort();
	}
}

void
print_selected_msg(void)
{
	if(vle_mode_is(VISUAL_MODE))
	{
		ui_sb_msgf("-- %s -- ", modvis_describe());
		update_vmode_input();
	}
	else
	{
		ui_sb_msgf("%d %s selected", curr_view->selected_files,
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
		ui_refresh_win(input_win);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
