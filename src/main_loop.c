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

#include <curses.h>

#include <sys/time.h> /* select() */
#include <sys/types.h> /* select() */
#include <signal.h>
#include <unistd.h> /* chdir(), select() */

#include <assert.h>
#include <string.h> /* strncpy */

#include "color_scheme.h"
#include "config.h"
#include "keys.h"
#include "macros.h"
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
	int wait_enter = 0;

	wtimeout(status_bar, cfg.timeout_len);

	buf[0] = L'\0';
	while(1)
	{
		wchar_t c;
		size_t counter;
		int ret;

		is_term_working();

		if(curr_stats.too_small_term > 0)
		{
			touchwin(stdscr);
			wrefresh(stdscr);

			mvwin(status_bar, 0, 0);
			wresize(status_bar, getmaxy(stdscr), getmaxx(stdscr));
			wclear(status_bar);
			waddstr(status_bar, "Terminal is too small for vifm");
			touchwin(status_bar);
			wrefresh(status_bar);

			pause();
			continue;
		}
		else if(curr_stats.too_small_term < 0)
		{
			wtimeout(status_bar, 0);
			while(wget_wch(status_bar, (wint_t*)&c) != ERR);
			curr_stats.too_small_term = 0;
			modes_redraw();
			wtimeout(status_bar, cfg.timeout_len);
		}

		modes_pre();

		/* just in case... */
		(void)chdir(curr_view->curr_dir);

		/* This waits for timeout then skips if no keypress. */
		ret = wget_wch(status_bar, (wint_t*)&c);

		if(ret != ERR && pos != ARRAY_LEN(buf) - 2)
		{
			if(c == L'\x1a') /* Ctrl-Z */
			{
				def_prog_mode();
				endwin();
				kill(0, SIGSTOP);
				continue;
			}

			if(wait_enter)
			{
				clean_status_bar();
				curr_stats.save_msg = 0;
				wait_enter = 0;
				if(c == L'\x0d')
					continue;
			}

			buf[pos++] = c;
			buf[pos] = L'\0';
		}

		counter = get_key_counter();
		if(ret == ERR && last_result == KEYS_WAIT_SHORT)
		{
			last_result = execute_keys_timed_out(buf);
			counter = get_key_counter() - counter;
			assert(counter <= pos);
			if(counter > 0)
			{
				pos -= counter;
				memmove(buf, buf + counter,
						(wcslen(buf) - counter + 1)*sizeof(wchar_t));
			}
		}
		else
		{
			if(ret != ERR)
				curr_stats.save_msg = 0;
			last_result = execute_keys(buf);
			counter = get_key_counter() - counter;
			assert(counter <= pos);
			if(counter > 0)
			{
				clear_input_bar();
				pos -= counter;
				memmove(buf, buf + counter,
						(wcslen(buf) - counter + 1)*sizeof(wchar_t));
			}
			if(last_result == KEYS_WAIT || last_result == KEYS_WAIT_SHORT)
			{
				if(ret != ERR)
					add_to_input_bar(c);
				if(last_result == KEYS_WAIT_SHORT && wcscmp(buf, L"\033") == 0)
					wtimeout(status_bar, 0);
				continue;
			}
		}

		wtimeout(status_bar, cfg.timeout_len);

		clear_input_bar();
		pos = 0;
		buf[0] = L'\0';

		if(is_status_bar_multiline())
			wait_enter = 1;

		modes_post();
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
