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


#include<ncurses.h>
#include<unistd.h> /* for chdir */
#include<string.h> /* strncpy */
#include<sys/time.h> /* select() */
#include<sys/types.h> /* select() */
#include<unistd.h> /* select() */

#include "background.h"
#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "keys_buildin_n.h"
#include "keys_eng.h"
#include "menus.h"
#include "modes.h"
#include "registers.h"
#include "search.h"
#include "signals.h"
#include "sort.h"
#include "status.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

void
switch_views(void)
{
	FileView *tmp = curr_view;
	curr_view = other_view;
	other_view = tmp;
}

static void
update_num_window(char *text)
{
	werase(num_win);
	mvwaddstr(num_win, 0, 0, text);
	wrefresh(num_win);
}

static void
clear_num_window(void)
{
	werase(num_win);
	wrefresh(num_win);
}

static void
reload_window(FileView *view)
{
	struct stat s;

	stat(view->curr_dir, &s);
	if(view != curr_view)
		change_directory(view, view->curr_dir);

	load_dir_list(view, 1);
	view->dir_mtime = s.st_mtime;

	if(view != curr_view)
	{
		change_directory(curr_view, curr_view->curr_dir);
		mvwaddstr(view->win, view->curr_line, 0, "*");
		wrefresh(view->win);
	}
	else
		moveto_list_pos(view, view->list_pos);

}

/*
 * This checks the modified times of the directories.
 */
static void
check_if_filelists_have_changed(FileView *view)
{
	struct stat s;

	stat(view->curr_dir, &s);
	if(s.st_mtime  != view->dir_mtime)
		reload_window(view);

	if (curr_stats.number_of_windows != 1 && curr_stats.view != 1)
	{
		stat(other_view->curr_dir, &s);
		if(s.st_mtime != other_view->dir_mtime)
			reload_window(other_view);
	}

}

static void
update_view(FileView *win)
{
	touchwin(win->title);
	touchwin(win->win);

	redrawwin(win->title);
	redrawwin(win->win);

	wnoutrefresh(win->title);
	wnoutrefresh(win->win);
}

void
update_all_windows(void)
{
	touchwin(lborder);
	touchwin(stat_win);
	touchwin(status_bar);
	touchwin(pos_win);
	touchwin(num_win);
	touchwin(rborder);

	/*
	 * redrawwin() shouldn't be needed.  But without it there is a
	 * lot of flickering when redrawing the windows?
	 */

	redrawwin(lborder);
	redrawwin(stat_win);
	redrawwin(status_bar);
	redrawwin(pos_win);
	redrawwin(num_win);
	redrawwin(rborder);

	/* In One window view */
	if (curr_stats.number_of_windows == 1)
	{
		update_view(curr_view);
	}
	/* Two Pane View */
	else
	{
		touchwin(mborder);
		redrawwin(mborder);
		wnoutrefresh(mborder);

		update_view(&lwin);
		update_view(&rwin);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(stat_win);
	wnoutrefresh(status_bar);
	wnoutrefresh(pos_win);
	wnoutrefresh(num_win);
	wnoutrefresh(rborder);

	doupdate();
}

static void
update_input_bar(int c)
{
	if(getcurx(num_win) == getmaxx(num_win) - 1)
	{
		mvwdelch(num_win, 0, 0);
		wmove(num_win, 0, getmaxx(num_win) - 2);
	}

	waddch(num_win, c);
	wrefresh(num_win);
}

/*
 *	Main Loop
 *	Everything is driven from this function with the exception of
 *	signals which are handled in signals.c
 */
void
main_key_press_cb()
{
	char status_buf[64] = "";
	int mode = NORMAL_MODE;
	int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		0,
		MF_USES_COUNT
	};
	char buf[128];
	int pos = 0;
	int c;
	int last_result = 0;

	/* TODO move this code where initialization occurs */
	init_keys(MODES_COUNT, &mode, (int*)&mode_flags, NULL);
	init_buildin_n_keys(&mode);
	init_registers();

	curs_set(0);

	wattroff(curr_view->win, COLOR_PAIR(CURR_LINE_COLOR));

	wtimeout(curr_view->win, KEYPRESS_TIMEOUT);
	wtimeout(other_view->win, KEYPRESS_TIMEOUT);

	update_stat_window(curr_view);

	if (curr_view->selected_files)
	{
		snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
				curr_view->selected_files, curr_view->selected_files == 1 ? "File" :
				"Files");
		status_bar_message(status_buf);
	}

	buf[0] = '\0';

	while (1) {
		int need_clear;

		check_if_filelists_have_changed(curr_view);
		check_background_jobs();

		if (!curr_stats.save_msg)
		{
			clean_status_bar();
			wrefresh(status_bar);
		}

		/* This waits for 1 second then skips if no keypress. */
		c = wgetch(curr_view->win);

		need_clear = (pos >= sizeof (buf) - 2);
		if (c != ERR && pos != sizeof (buf) - 2)
		{
			if (c > 255)
				buf[pos++] = c >> 8;
			buf[pos++] = c;
			buf[pos] = '\0';
		}

		/* escape or ctrl c */
		if(c == 27 || c == 3)
		{
			clean_selected_files(curr_view);
			redraw_window();
			curs_set(0);
		}
		else if(c == ERR && last_result == KEYS_WAIT_SHORT)
			last_result = 0;
		else
		{
			if(c != ERR)
				curr_stats.save_msg = 0;
			last_result = execute_keys(buf);
			if(last_result == KEYS_WAIT || last_result == KEYS_WAIT_SHORT)
			{
				if(c != ERR)
					update_input_bar(c);
				continue;
			}
		}
		need_clear = 1;

		if (need_clear == 1) {
			clear_num_window();
			pos = 0;
			buf[0] = '\0';
		}

		if(curr_stats.show_full)
			show_full_file_properties(curr_view);
		else
			update_stat_window(curr_view);

		if(curr_view->selected_files)
		{
			static int number = 0;
			if(number != curr_view->selected_files)
			{
				snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
						curr_view->selected_files, curr_view->selected_files == 1 ? "File" :
						"Files");
				status_bar_message(status_buf);
				curr_stats.save_msg = 1;
			}
		}
		else if(!curr_stats.save_msg)
			clean_status_bar();

		if(curr_stats.need_redraw)
			redraw_window();

		update_all_windows();
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
