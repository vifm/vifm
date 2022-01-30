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
#include <limits.h> /* CHAR_MAX CHAR_MIN */
#include <stdarg.h> /* va_list va_start() va_end() vsnprintf() */
#include <stddef.h> /* NULL */
#include <string.h> /* strchr() strlen() */

#include "../../cfg/config.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../modes/modes.h"
#include "../../ui/quickview.h"
#include "../../ui/statusline.h"
#include "../../ui/ui.h"
#include "../../utils/macros.h"
#include "../../utils/str.h"
#include "../../utils/utf8.h"
#include "../../event_loop.h"
#include "../../status.h"
#include "../wk.h"

/* Kinds of dialogs. */
typedef enum
{
	D_ERROR,              /* Error message. */
	D_QUERY_WITHOUT_LIST, /* User query with all lines centered. */
	D_QUERY_WITH_LIST,    /* User query with left aligned list and one line
	                         centered at the top. */
}
Dialog;

/* Kinds of dialog results. */
typedef enum
{
	DR_OK,     /* Agreed. */
	DR_CANCEL, /* Cancelled. */
	DR_YES,    /* Confirmed. */
	DR_NO,     /* Denied. */
	DR_CUSTOM, /* One of user-specified keys. */
}
DialogResult;

static int def_handler(wchar_t key);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_y(key_info_t key_info, keys_info_t *keys_info);
static void handle_response(DialogResult dr);
static void leave(DialogResult dr);
static int prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa);
static int prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip);
static void prompt_msg_internal(const char title[], const char message[],
		const response_variant variants[], int with_list);
static void enter(const char title[], const char message[], int prompt_skip,
		int result_mask);
static void redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip, int lazy);
static const char * get_control_msg(Dialog msg_kind, int global_skip);
static const char * get_custom_control_msg(const response_variant responses[]);
static void draw_msg(const char title[], const char msg[],
		const char ctrl_msg[], int lines_to_center, int recommended_width);
static size_t measure_sub_lines(const char msg[], size_t *max_len);
static size_t determine_width(const char msg[]);

/* List of builtin key bindings. */
static keys_add_info_t builtin_cmds[] = {
	{WK_C_c, {{&cmd_ctrl_c}, .descr = "cancel"}},
	{WK_C_l, {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_C_m, {{&cmd_ctrl_m}, .descr = "agree to the query"}},
	{WK_ESC, {{&cmd_ctrl_c}, .descr = "cancel"}},
	{WK_n,   {{&cmd_n},      .descr = "deny the query"}},
	{WK_y,   {{&cmd_y},      .descr = "confirm the query"}},
};

/* Main loop quit flag. */
static int quit;
/* Dialog result. */
static DialogResult dialog_result;
/* Bit mask of R_* kinds of results that are allowed. */
static int accept_mask;
/* Type of active dialog message. */
static Dialog msg_kind;

/* Possible responses for custom prompt message. */
static const response_variant *responses;
/* One of user-defined input keys. */
static char custom_result;

void
init_msg_dialog_mode(void)
{
	int ret_code;

	vle_keys_set_def_handler(MSG_MODE, def_handler);

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), MSG_MODE);
	assert(ret_code == 0 && "Failed to register msg dialog keys.");

	(void)ret_code;
}

/* Handles all keys uncaught by shortcuts.  Returns zero on success and non-zero
 * on error. */
static int
def_handler(wchar_t key)
{
	if(key < CHAR_MIN || key > CHAR_MAX)
	{
		return 0;
	}

	const response_variant *response = responses;
	while(response != NULL && response->key != '\0')
	{
		if(response->key == (char)key)
		{
			custom_result = key;
			leave(DR_CUSTOM);
			break;
		}
		++response;
	}
	return 0;
}

/* Cancels the query. */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(DR_CANCEL);
}

/* Redraws the screen. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	stats_redraw_later();
}

/* Agrees to the query. */
static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(DR_OK);
}

/* Denies the query. */
static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(DR_NO);
}

/* Confirms the query. */
static void
cmd_y(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(DR_YES);
}

/* Processes users choice.  Leaves the mode if the result is found among the
 * list of expected results. */
static void
handle_response(DialogResult dr)
{
	/* Map result to corresponding input key to omit branching per handler. */
	static const char r_to_c[] = {
		[DR_OK]     = '\r',
		[DR_CANCEL] = NC_C_c,
		[DR_YES]    = 'y',
		[DR_NO]     = 'n',
	};

	(void)def_handler(r_to_c[dr]);
	/* Default handler might have already requested quitting. */
	if(!quit)
	{
		if(accept_mask & MASK(dr))
		{
			leave(dr);
		}
	}
}

/* Leaves the mode with given result. */
static void
leave(DialogResult dr)
{
	dialog_result = dr;
	quit = 1;
}

void
redraw_msg_dialog(int lazy)
{
	redraw_error_msg(NULL, NULL, 0, lazy);
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
	char portion[1024];
	do
	{
		copy_str(portion, sizeof(portion), message);
		message += strlen(portion);
		if(prompt_error_msg_internal(title, portion, 1))
		{
			return 1;
		}
	}
	while(*message != '\0');
	return 0;
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
	char msg[2048];
	vsnprintf(msg, sizeof(msg), format, pa);
	return prompt_error_msg_internal(title, msg, prompt_skip);
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

	enter(title, message, prompt_skip,
			MASK(DR_OK) | (prompt_skip ? MASK(DR_CANCEL) : 0));

	if(curr_stats.load_stage < 2)
	{
		skip_until_started = (dialog_result == DR_CANCEL);
	}

	modes_redraw();

	return (dialog_result == DR_CANCEL);
}

int
prompt_msg(const char title[], const char message[])
{
	prompt_msg_internal(title, message, NULL, 0);
	return (dialog_result == DR_YES);
}

int
prompt_msgf(const char title[], const char format[], ...)
{
	va_list pa;
	va_start(pa, format);

	char msg[2048];
	vsnprintf(msg, sizeof(msg), format, pa);

	va_end(pa);

	return prompt_msg(title, msg);
}

char
prompt_msg_custom(const char title[], const char message[],
		const response_variant variants[])
{
	assert(variants[0].key != '\0' && "Variants should have at least one item.");
	prompt_msg_internal(title, message, variants, 0);
	return custom_result;
}

/* Common implementation of prompt message.  The variants can be NULL. */
static void
prompt_msg_internal(const char title[], const char message[],
		const response_variant variants[], int with_list)
{
	responses = variants;
	msg_kind = (with_list ? D_QUERY_WITH_LIST : D_QUERY_WITHOUT_LIST);

	enter(title, message, /*prompt_skip=*/0,
			variants == NULL ? MASK(DR_YES, DR_NO) : 0);

	modes_redraw();
}

/* Enters the mode, which won't be left until one of expected results specified
 * by the mask is picked by the user. */
static void
enter(const char title[], const char message[], int prompt_skip,
		int result_mask)
{
	const int prev_use_input_bar = curr_stats.use_input_bar;
	const vle_mode_t prev_mode = vle_mode_get();

	/* Message must be visible to the user for him to close it.  It won't happen
	 * without user interaction as new event loop is out of scope of a mapping
	 * that started it. */
	stats_unsilence_ui();

	ui_hide_graphics();

	accept_mask = result_mask;
	curr_stats.use_input_bar = 0;
	vle_mode_set(MSG_MODE, VMT_SECONDARY);

	redraw_error_msg(title, message, prompt_skip, /*lazy=*/0);

	quit = 0;
	/* Avoid starting nested loop in tests. */
	if(curr_stats.load_stage > 0)
	{
		event_loop(&quit, /*manage_marking=*/0);
	}

	vle_mode_set(prev_mode, VMT_SECONDARY);
	curr_stats.use_input_bar = prev_use_input_bar;
}

/* Draws error message on the screen or redraws the last message when both
 * title_arg and message_arg are NULL. */
static void
redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip, int lazy)
{
	/* TODO: refactor this function redraw_error_msg() */

	static const char *title;
	static const char *message;
	static int ctrl_c;

	const char *ctrl_msg;
	const int lines_to_center = msg_kind == D_QUERY_WITHOUT_LIST ? INT_MAX
	                          : msg_kind == D_QUERY_WITH_LIST ? 1 : 0;

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

	ctrl_msg = get_control_msg(msg_kind, ctrl_c);
	draw_msg(title, message, ctrl_msg, lines_to_center, 0);

	if(lazy)
	{
		wnoutrefresh(error_win);
	}
	else
	{
		ui_refresh_win(error_win);
	}
}

/* Picks control message (information on available actions) basing on dialog
 * kind and previous actions.  Returns the message. */
static const char *
get_control_msg(Dialog msg_kind, int global_skip)
{
	if(msg_kind == D_QUERY_WITHOUT_LIST || msg_kind == D_QUERY_WITH_LIST)
	{
		if(responses == NULL)
		{
			return "Enter [y]es or [n]o";
		}

		return get_custom_control_msg(responses);
	}
	else if(global_skip)
	{
		return "Press Return to continue or "
		       "Ctrl-C to skip its future error messages";
	}

	return "Press Return to continue";
}

/* Formats dialog control message for custom set of responses.  Returns pointer
 * to statically allocated buffer. */
static const char *
get_custom_control_msg(const response_variant responses[])
{
	static char msg_buf[256];

	const response_variant *response = responses;
	size_t len = 0U;
	msg_buf[0] = '\0';
	while(response != NULL && response->key != '\0')
	{
		if(response->descr[0] != '\0')
		{
			(void)sstrappend(msg_buf, &len, sizeof(msg_buf), response->descr);
			if(response[1].key != '\0')
			{
				(void)sstrappendch(msg_buf, &len, sizeof(msg_buf), '/');
			}
		}

		++response;
	}

	return msg_buf;
}

void
draw_msgf(const char title[], const char ctrl_msg[], int recommended_width,
		const char format[], ...)
{
	char msg[8192];
	va_list pa;

	va_start(pa, format);
	vsnprintf(msg, sizeof(msg), format, pa);
	va_end(pa);

	draw_msg(title, msg, ctrl_msg, 0, recommended_width);
	touch_all_windows();
	ui_refresh_win(error_win);
}

/* Draws formatted message with lines_to_center top lines centered with
 * specified title and control message on error_win. */
static void
draw_msg(const char title[], const char msg[], const char ctrl_msg[],
		int lines_to_center, int recommended_width)
{
	enum { margin = 1 };

	int sw, sh;
	int w, h;
	int max_h;
	int len;
	size_t ctrl_msg_n;
	size_t wctrl_msg;
	int first_line_x = 1;
	const int first_line_y = 2;

	if(curr_stats.load_stage < 1)
	{
		return;
	}

	curs_set(0);

	getmaxyx(stdscr, sh, sw);

	ctrl_msg_n = MAX(measure_sub_lines(ctrl_msg, &wctrl_msg), 1U);

	max_h = sh - 2 - ctrl_msg_n - ui_stat_height();
	h = max_h;
	/* The outermost condition is for VLA below (to calm static analyzers). */
	w = MAX(2 + 2*margin, MIN(sw - 2,
	        MAX(MAX(recommended_width, sw/3),
	            (int)MAX(wctrl_msg, determine_width(msg)) + 4)));
	wresize(error_win, h, w);

	werase(error_win);

	len = strlen(msg);
	if(len <= w - 2 && strchr(msg, '\n') == NULL)
	{
		first_line_x = (w - len)/2;
		h = 5 + ctrl_msg_n;
		wresize(error_win, h, w);
		mvwin(error_win, (sh - h)/2, (sw - w)/2);
		checked_wmove(error_win, first_line_y, first_line_x);
		wprint(error_win, msg);
	}
	else
	{
		int i = 0;
		int cy = first_line_y;
		while(i < len)
		{
			int j;
			char buf[w - 2 - 2*margin + 1];
			int cx;

			copy_str(buf, sizeof(buf), msg + i);

			for(j = 0; buf[j] != '\0'; j++)
				if(buf[j] == '\n')
					break;

			if(buf[j] != '\0')
				i++;
			buf[j] = '\0';
			i += j;

			if(buf[0] == '\0')
				continue;

			if(cy >= max_h - (int)ctrl_msg_n - 3)
			{
				/* Skip trailing part of the message if it's too long, just print how
				 * many lines we're omitting. */
				size_t max_len;
				const int more_lines = 1U + measure_sub_lines(msg + i, &max_len);
				snprintf(buf, sizeof(buf), "<<%d more line%s not shown>>", more_lines,
						(more_lines == 1) ? "" : "s");
				/* Make sure this is the last iteration of the loop. */
				i = len;
			}

			h = 1 + cy + 1 + ctrl_msg_n + 1;
			wresize(error_win, h, w);
			mvwin(error_win, (sh - h)/2, (sw - w)/2);

			cx = lines_to_center-- > 0 ? (w - utf8_strsw(buf))/2 : (1 + margin);
			if(cy == first_line_y)
			{
				first_line_x = cx;
			}
			checked_wmove(error_win, cy++, cx);
			wprint(error_win, buf);
		}
	}

	box(error_win, 0, 0);
	if(title[0] != '\0')
	{
		mvwprintw(error_win, 0, (w - strlen(title) - 2)/2, " %s ", title);
	}

	/* Print control message line by line. */
	size_t i = ctrl_msg_n;
	while(1)
	{
		const size_t len = strcspn(ctrl_msg, "\n");
		mvwaddnstr(error_win, h - i - 1, MAX(0, (w - (int)len)/2), ctrl_msg, len);

		if(--i == 0)
		{
			break;
		}

		ctrl_msg = skip_char(ctrl_msg + len + 1U, '/');
	}

	checked_wmove(error_win, first_line_y, first_line_x);
}

/* Counts number of sub-lines (separated by new-line character in the msg.  Sets
 * *max_len to the length of the longest sub-line.  Returns total number of
 * sub-lines, which can be zero is msg is an empty line. */
static size_t
measure_sub_lines(const char msg[], size_t *max_len)
{
	size_t nlines = 0U;
	*max_len = 0U;
	while(*msg != '\0')
	{
		const size_t len = strcspn(msg, "\n");
		if(len > *max_len)
		{
			*max_len = len;
		}
		++nlines;
		msg += len + (msg[len] == '\n' ? 1U : 0U);
	}
	return nlines;
}

/* Determines maximum width of line in the message.  Returns the width. */
static size_t
determine_width(const char msg[])
{
	size_t max_width = 0U;

	while(*msg != '\0')
	{
		size_t width = 0U;
		while(*msg != '\n' && *msg != '\0')
		{
			++width;
			++msg;
		}

		if(width > max_width)
		{
			max_width = width;
		}

		if(*msg == '\n')
		{
			++msg;
		}
	}

	return max_width;
}

int
confirm_deletion(char *files[], int nfiles, int use_trash)
{
	if(nfiles == 0)
	{
		return 0;
	}

	curr_stats.confirmed = 0;
	if(!cfg_confirm_delete(use_trash))
	{
		return 1;
	}

	const char *const title = use_trash ? "Deletion" : "Permanent deletion";
	char *msg;

	/* Trailing space is needed to prevent line dropping. */
	if(nfiles == 1)
	{
		msg = format_str("Are you sure you want to delete \"%s\"?", files[0]);
	}
	else
	{
		msg = format_str("Are you sure you want to delete %d files?\n ", nfiles);

		size_t msg_len = strlen(msg);
		int i;
		for(i = 0; i < nfiles; ++i)
		{
			if(strappend(&msg, &msg_len, "\n* ") != 0 ||
					strappend(&msg, &msg_len, files[i]) != 0)
			{
				break;
			}
		}
	}

	prompt_msg_internal(title, msg, NULL, 1);
	free(msg);

	if(dialog_result != DR_YES)
	{
		return 0;
	}

	curr_stats.confirmed = 1;
	return 1;
}

void
show_errors_from_file(FILE *ef, const char title[])
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
			int skip = (prompt_error_msg(title, buf) != 0);
			buf[0] = '\0';
			if(skip)
				break;
		}
		strcat(buf, linebuf);
	}

	if(buf[0] != '\0')
	{
		show_error_msg(title, buf);
	}

	fclose(ef);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
