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

#include "main_loop.h"

#include <curses.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <sys/time.h> /* select() */
#include <sys/types.h> /* select() */
#include <signal.h>
#include <unistd.h> /* select() */

#include <assert.h>
#include <string.h> /* strncpy */

#include "cfg/config.h"
#include "engine/keys.h"
#include "modes/modes.h"
#include "modes/normal.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/utils.h"
#include "background.h"
#include "color_scheme.h"
#include "filelist.h"
#include "ipc.h"
#include "status.h"
#include "ui.h"

static wchar_t buf[128];
static int pos;

#ifdef _WIN32
static void
update_win_console(void)
{
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_ECHO_INPUT |
			ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
			ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE);
}
#endif

static int
read_char(WINDOW *win, wint_t *c, int timeout)
{
	static const int T = 150;
	static const int IPC_F = 10;

	int i;
	int result = ERR;

	for(i = 0; i <= timeout/T; i++)
	{
		int j;

		if(is_redraw_scheduled())
		{
			modes_redraw();
		}

		if(!is_status_bar_multiline() && !is_in_menu_like_mode() &&
				get_mode() != CMDLINE_MODE)
		{
			check_if_filelists_have_changed(curr_view);
			if(curr_stats.number_of_windows != 1 && !curr_stats.view)
				check_if_filelists_have_changed(other_view);
		}

		check_background_jobs();

		for(j = 0; j < IPC_F; j++)
		{
			ipc_check();
			wtimeout(win, MIN(T, timeout)/IPC_F);

			if((result = wget_wch(win, c)) != ERR)
				break;

			if(is_redraw_scheduled())
			{
				modes_redraw();
			}
		}
		if(result != ERR)
			break;

		timeout -= T;
	}
	return result;
}

/*
 * Main Loop
 * Everything is driven from this function with the exception of
 * signals which are handled in signals.c
 */
void
main_loop(void)
{
	LOG_FUNC_ENTER;

	int last_result = 0;
	int wait_enter = 0;
	int timeout = cfg.timeout_len;

	buf[0] = L'\0';
	while(1)
	{
		wchar_t c;
		size_t counter;
		int ret;

		is_term_working();

#ifdef _WIN32
		update_win_console();
#endif

		lwin.user_selection = 1;
		rwin.user_selection = 1;

		if(curr_stats.too_small_term > 0)
		{
			touchwin(stdscr);
			wrefresh(stdscr);

			mvwin(status_bar, 0, 0);
			wresize(status_bar, getmaxy(stdscr), getmaxx(stdscr));
			werase(status_bar);
			waddstr(status_bar, "Terminal is too small for vifm");
			touchwin(status_bar);
			wrefresh(status_bar);

#ifndef _WIN32
			pause();
#endif
			continue;
		}
		else if(curr_stats.too_small_term < 0)
		{
			wtimeout(status_bar, 0);
			while(wget_wch(status_bar, (wint_t*)&c) != ERR);
			curr_stats.too_small_term = 0;
			modes_redraw();
			wtimeout(status_bar, cfg.timeout_len);

			wait_enter = 0;
			curr_stats.save_msg = 0;
			status_bar_message("");
		}

		modes_pre();

		/* This waits for timeout then skips if no keypress. */
		ret = read_char(status_bar, (wint_t*)&c, timeout);

		/* Ensure that current working directory is set correctly (some pieces of
		 * code rely on this). */
		(void)my_chdir(curr_view->curr_dir);

		if(ret != ERR && pos != ARRAY_LEN(buf) - 2)
		{
			if(c == L'\x1a') /* Ctrl-Z */
			{
				def_prog_mode();
				endwin();
#ifndef _WIN32
				{
					void (*saved_stp_sig_handler)(int) = signal(SIGTSTP, SIG_DFL);
					kill(0, SIGTSTP);
					signal(SIGTSTP, saved_stp_sig_handler);
				}
#endif
				continue;
			}

			if(wait_enter)
			{
				wait_enter = 0;
				curr_stats.save_msg = 0;
				clean_status_bar();
				if(c == L'\x0d')
					continue;
			}

			buf[pos++] = c;
			buf[pos] = L'\0';
		}

		if(wait_enter && ret == ERR)
			continue;

		counter = get_key_counter();
		if(ret == ERR && last_result == KEYS_WAIT_SHORT)
		{
			last_result = execute_keys_timed_out(buf);
			counter = get_key_counter() - counter;
			assert(counter <= pos);
			if(counter > 0)
			{
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
				pos -= counter;
				memmove(buf, buf + counter,
						(wcslen(buf) - counter + 1)*sizeof(wchar_t));
			}
			if(last_result == KEYS_WAIT || last_result == KEYS_WAIT_SHORT)
			{
				if(ret != ERR)
					modupd_input_bar(buf);
				if(last_result == KEYS_WAIT_SHORT && wcscmp(buf, L"\033") == 0)
					timeout = 1;
				if(counter > 0)
					clear_input_bar();

				if(!curr_stats.save_msg && curr_view->selected_files &&
						get_mode() != CMDLINE_MODE)
					print_selected_msg();
				continue;
			}
		}

		timeout = cfg.timeout_len;

		if(is_redraw_scheduled())
		{
			modes_redraw();
		}

		pos = 0;
		buf[0] = L'\0';
		clear_input_bar();

		if(is_status_bar_multiline())
		{
			wait_enter = 1;
			update_all_windows();
			continue;
		}

		/* Ensure that current working directory is set correctly (some pieces of
		 * code rely on this).  PWD could be changed during command execution, but
		 * it should be correct for modes_post() in case of preview modes. */
		(void)my_chdir(curr_view->curr_dir);
		modes_post();
	}
}

void
update_input_buf(void)
{
	wprintw(input_win, "%ls", buf);
	wrefresh(input_win);
}

int
is_input_buf_empty(void)
{
	return pos == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
