/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2015 xaizek.
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

#include "msg_dialog.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <stdarg.h> /* va_list va_start() va_end() vsnprintf() */
#include <stddef.h> /* NULL */
#include <string.h> /* strdup() strchr() strlen() */

#include "../../cfg/config.h"
#include "../../modes/modes.h"
#include "../../ui/ui.h"
#include "../../utils/str.h"
#include "../../status.h"

static int prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa);
static int prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip);
static void redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip);

void
redraw_msg_dialog(void)
{
	redraw_error_msg(NULL, NULL, 0);
}

void
show_error_msg(const char title[], const char message[])
{
	(void)prompt_error_msg_internal(title, message, 0);
}

void
show_error_msgf(const char title[], const char format[], ...)
{
	va_list pa;

	va_start(pa, format);
	(void)prompt_error_msg_internalv(title, format, 0, pa);
	va_end(pa);
}

int
prompt_error_msg(const char title[], const char message[])
{
	return prompt_error_msg_internal(title, message, 1);
}

int
prompt_error_msgf(const char title[], const char format[], ...)
{
	int result;
	va_list pa;

	va_start(pa, format);
	result = prompt_error_msg_internalv(title, format, 1, pa);
	va_end(pa);

	return result;
}

/* Just a varargs wrapper over prompt_error_msg_internal(). */
static int
prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa)
{
	char buf[2048];
	vsnprintf(buf, sizeof(buf), format, pa);
	return prompt_error_msg_internal(title, buf, prompt_skip);
}

/* Internal function for displaying error messages to a user.  Automatically
 * skips whitespace in front of the message and does nothing for empty messages
 * (due to skipping whitespace-only are counted as empty). When the prompt_skip
 * isn't zero, asks user about successive messages.  Returns non-zero if all
 * successive messages should be skipped, otherwise zero is returned. */
static int
prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip)
{
	static int skip_until_started;
	int key;

	if(curr_stats.load_stage == 0)
		return 1;
	if(curr_stats.load_stage < 2 && skip_until_started)
		return 1;

	message = skip_whitespace(message);
	if(*message == '\0')
	{
		return 0;
	}

	curr_stats.errmsg_shown = 1;

	redraw_error_msg(title, message, prompt_skip);

	do
		key = wgetch(error_win);
	while(key != 13 && (!prompt_skip || key != 3)); /* ascii Return, Ctrl-c */

	if(curr_stats.load_stage < 2)
		skip_until_started = key == 3;

	werase(error_win);
	wrefresh(error_win);

	curr_stats.errmsg_shown = 0;

	modes_update();
	if(curr_stats.need_update != UT_NONE)
		modes_redraw();

	return key == 3;
}


int
query_user_menu(const char title[], const char message[])
{
	int key;
	char *dup = strdup(message);

	curr_stats.errmsg_shown = 2;

	redraw_error_msg(title, message, 0);

	do
	{
		key = wgetch(error_win);
	}
	while(key != 'y' && key != 'n' && key != ERR);

	free(dup);

	curr_stats.errmsg_shown = 0;

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_update != UT_NONE)
		update_screen(UT_FULL);

	return key == 'y';
}


/* Draws error message on the screen or redraws the last message when both
 * title_arg and message_arg are NULL. */
static void
redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip)
{
	/* TODO: refactor this function redraw_error_msg() */

	static const char *title;
	static const char *message;
	static int ctrl_c;

	int sx, sy;
	int x, y;
	int z;
	const char *text;

	if(title_arg != NULL && message_arg != NULL)
	{
		title = title_arg;
		message = message_arg;
		ctrl_c = prompt_skip;
	}

	assert(message != NULL);

	curs_set(FALSE);
	werase(error_win);

	getmaxyx(stdscr, sy, sx);

	y = sy - 3 + !cfg.display_statusline;
	x = sx - 2;
	wresize(error_win, y, x);

	z = strlen(message);
	if(z <= x - 2 && strchr(message, '\n') == NULL)
	{
		y = 6;
		wresize(error_win, y, x);
		mvwin(error_win, (sy - y)/2, (sx - x)/2);
		checked_wmove(error_win, 2, (x - z)/2);
		wprint(error_win, message);
	}
	else
	{
		int i;
		int cy = 2;
		i = 0;
		while(i < z)
		{
			int j;
			char buf[x - 2 + 1];

			snprintf(buf, sizeof(buf), "%s", message + i);

			for(j = 0; buf[j] != '\0'; j++)
				if(buf[j] == '\n')
					break;

			if(buf[j] != '\0')
				i++;
			buf[j] = '\0';
			i += j;

			if(buf[0] == '\0')
				continue;

			y = cy + 4;
			mvwin(error_win, (sy - y)/2, (sx - x)/2);
			wresize(error_win, y, x);

			checked_wmove(error_win, cy++, 1);
			wprint(error_win, buf);
		}
	}

	box(error_win, 0, 0);
	if(title[0] != '\0')
		mvwprintw(error_win, 0, (x - strlen(title) - 2)/2, " %s ", title);

	if(curr_stats.errmsg_shown == 1)
	{
		if(ctrl_c)
		{
			text = "Press Return to continue or Ctrl-C to skip other error messages";
		}
		else
		{
			text = "Press Return to continue";
		}
	}
	else
	{
		text = "Enter [y]es or [n]o";
	}
	mvwaddstr(error_win, y - 2, (x - strlen(text))/2, text);
}

int
confirm_deletion(int use_trash)
{
	curr_stats.confirmed = 0;
	if(!use_trash && cfg.confirm)
	{
		const int proceed = query_user_menu("Permanent deletion",
				"Are you sure you want to delete files permanently?");

		if(!proceed)
		{
			return 0;
		}

		curr_stats.confirmed = 1;
	}
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
