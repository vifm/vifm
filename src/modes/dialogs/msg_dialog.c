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
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../modes/modes.h"
#include "../../ui/ui.h"
#include "../../utils/macros.h"
#include "../../utils/str.h"
#include "../../event_loop.h"
#include "../../status.h"

/* Kinds of dialogs. */
typedef enum
{
	D_ERROR, /* Error message. */
	D_QUERY, /* User query. */
}
Dialog;

/* Kinds of dialog results. */
typedef enum
{
	R_OK,     /* Agreed. */
	R_CANCEL, /* Cancelled. */
	R_YES,    /* Confirmed. */
	R_NO,     /* Denied. */
}
Result;

static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_y(key_info_t key_info, keys_info_t *keys_info);
static void handle_response(Result r);
static void leave(Result r);
static int prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa);
static int prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip);
static void enter(int result_mask);
static void redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip);
static const char * get_control_msg(Dialog msg_kind, int global_skip);

/* List of builtin key bindings. */
static keys_add_info_t builtin_cmds[] = {
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"y", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_y}}},
};

/* Mode that was active before the dialog. */
static vle_mode_t prev_mode;
/* Main loop quit flag. */
static int quit;
/* Dialog result. */
static Result result;
/* Previous value of curr_stats.use_input_bar. */
static int prev_use_input_bar;
/* Bit mask of R_* kinds of results that are allowed. */
static int accept_mask;
/* Type of active dialog message. */
static Dialog msg_kind;

void
init_msg_dialog_mode(void)
{
	int ret_code;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), MSG_MODE);
	assert(ret_code == 0 && "Failed to register msg dialog keys.");

	(void)ret_code;
}

/* Cancels the query. */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(R_CANCEL);
}

/* Redraws the screen. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	schedule_redraw();
}

/* Agrees to the query. */
static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(R_OK);
}

/* Denies the query. */
static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(R_NO);
}

/* Confirms the query. */
static void
cmd_y(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(R_YES);
}

/* Processes users choice.  Leaves the mode if the result is found among the
 * list of expected results. */
static void
handle_response(Result r)
{
	/* Stay in message mode unless such result is not expected. */
	if(accept_mask & MASK(r))
	{
		leave(r);
	}
}

/* Leaves the mode with given result. */
static void
leave(Result r)
{
	curr_stats.use_input_bar = prev_use_input_bar;
	result = r;

	vle_mode_set(prev_mode, VMT_SECONDARY);

	quit = 1;
}

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

	if(curr_stats.load_stage == 0)
		return 1;
	if(curr_stats.load_stage < 2 && skip_until_started)
		return 1;

	message = skip_whitespace(message);
	if(*message == '\0')
	{
		return 0;
	}

	msg_kind = D_ERROR;

	redraw_error_msg(title, message, prompt_skip);

	enter(MASK(R_OK) | (prompt_skip ? MASK(R_CANCEL) : 0));

	if(curr_stats.load_stage < 2)
		skip_until_started = (result == R_CANCEL);

	werase(error_win);
	wrefresh(error_win);

	modes_update();
	if(curr_stats.need_update != UT_NONE)
		modes_redraw();

	return result == R_CANCEL;
}

int
query_user_menu(const char title[], const char message[])
{
	char *dup = strdup(message);

	msg_kind = D_QUERY;

	redraw_error_msg(title, message, 0);

	enter(MASK(R_YES, R_NO));

	free(dup);

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_update != UT_NONE)
		update_screen(UT_FULL);

	return result == R_YES;
}

/* Enters the mode, which won't be left until one of expected results specified
 * by the mask is picked by the user. */
static void
enter(int result_mask)
{
	accept_mask = result_mask;
	prev_use_input_bar = curr_stats.use_input_bar;
	curr_stats.use_input_bar = 0;

	prev_mode = vle_mode_get();
	vle_mode_set(MSG_MODE, VMT_SECONDARY);

	quit = 0;
	event_loop(&quit);
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
	const char *control_msg;
	const int centered = (msg_kind == D_QUERY);

	if(title_arg != NULL && message_arg != NULL)
	{
		title = title_arg;
		message = message_arg;
		ctrl_c = prompt_skip;
	}

	if(title == NULL || message == NULL)
	{
		assert(title != NULL && "Asked to redraw dialog, but no title is set.");
		assert(message != NULL && "Asked to redraw dialog, but no message is set.");
		return;
	}

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
			int cx;

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

			cx = centered ? (x - strlen(buf) - 2)/2 : 1;
			checked_wmove(error_win, cy++, cx);
			wprint(error_win, buf);
		}
	}

	box(error_win, 0, 0);
	if(title[0] != '\0')
		mvwprintw(error_win, 0, (x - strlen(title) - 2)/2, " %s ", title);

	control_msg = get_control_msg(msg_kind, ctrl_c);
	mvwaddstr(error_win, y - 2, (x - strlen(control_msg))/2, control_msg);

	wrefresh(error_win);
}

/* Picks control message (information on available actions) basing on dialog
 * kind and previous actions.  Returns the message. */
static const char *
get_control_msg(Dialog msg_kind, int global_skip)
{
	if(msg_kind == D_QUERY)
	{
		return "Enter [y]es or [n]o";
	}
	else if(global_skip)
	{
		return "Press Return to continue or Ctrl-C to skip other error messages";
	}
	else
	{
		return "Press Return to continue";
	}
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

void
show_errors_from_file(FILE *ef)
{
	char linebuf[160];
	char buf[sizeof(linebuf)*5];

	if(ef == NULL)
		return;

	buf[0] = '\0';
	while(fgets(linebuf, sizeof(linebuf), ef) == linebuf)
	{
		if(linebuf[0] == '\n')
			continue;
		if(strlen(buf) + strlen(linebuf) + 1 >= sizeof(buf))
		{
			int skip = (prompt_error_msg("Background Process Error", buf) != 0);
			buf[0] = '\0';
			if(skip)
				break;
		}
		strcat(buf, linebuf);
	}

	if(buf[0] != '\0')
		show_error_msg("Background Process Error", buf);

	fclose(ef);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
