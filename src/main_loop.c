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

#define _GNU_SOURCE /* I don't know how portable this is but it is
					   needed in Linux for the ncurses wide char
					   functions
					   */

#include <ncurses.h>

#include <sys/time.h> /* select() */
#include <sys/types.h> /* select() */
#include <unistd.h> /* chdir(), select() */

#include <string.h> /* strncpy */

#include "color_scheme.h"
#include "config.h"
#include "keys.h"
#include "modes.h"
#include "normal.h"
#include "status.h"
#include "ui.h"

#include "main_loop.h"

/*
 * Main Loop
 * Everything is driven from this function with the exception of
 * signals which are handled in signals.c
 */
void
main_loop(void)
{
	wchar_t buf[128];
	int pos = 0;
	int last_result = 0;

	curs_set(0);

	wattroff(curr_view->win, COLOR_PAIR(CURR_LINE_COLOR));

	wtimeout(status_bar, KEYPRESS_TIMEOUT);

	update_stat_window(curr_view);

	if(curr_view->selected_files)
	{
		char status_buf[64] = "";
		snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
				curr_view->selected_files, curr_view->selected_files == 1 ? "File" :
				"Files");
		status_bar_message(status_buf);
	}

	buf[0] = L'\0';
	while(1)
	{
		wchar_t c;
		int ret;

		modes_pre();
		chdir(curr_view->curr_dir);

		/* This waits for timeout then skips if no keypress. */
		ret = wget_wch(status_bar, (wint_t*)&c);

		if(ret != ERR && pos != sizeof(buf)/sizeof(buf[0]) - 2)
		{
			buf[pos++] = c;
			buf[pos] = L'\0';
		}

		if(ret == ERR && last_result == KEYS_WAIT_SHORT)
		{
			last_result = execute_keys_timed_out(buf);
		}
		else
		{
			if(ret != ERR)
				curr_stats.save_msg = 0;
			last_result = execute_keys(buf);
			if(last_result == KEYS_WAIT || last_result == KEYS_WAIT_SHORT)
			{
				if(ret != ERR)
					add_to_input_bar(c);
				if(last_result == KEYS_WAIT_SHORT)
					wtimeout(status_bar, 0);
				continue;
			}
		}
		wtimeout(status_bar, KEYPRESS_TIMEOUT);

		clear_input_bar();
		pos = 0;
		buf[0] = L'\0';

		modes_post();
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
