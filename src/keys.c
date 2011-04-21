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
#include "color_scheme.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "keys_buildin_n.h"
#include "keys_eng.h"
#include "modes.h"
#include "status.h"
#include "ui.h"

#include "keys.h"

/*
 * Main Loop
 * Everything is driven from this function with the exception of
 * signals which are handled in signals.c
 */
void
main_loop(void)
{
	char status_buf[64] = "";
	char buf[128];
	int pos = 0;
	int c;
	int last_result = 0;

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

		if(c == ERR && last_result == KEYS_WAIT_SHORT)
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
