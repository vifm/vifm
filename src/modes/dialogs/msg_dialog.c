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
#include "../../utils/test_helpers.h"
#include "../../utils/utf8.h"
#include "../../event_loop.h"
#include "../../status.h"
#include "../wk.h"

/* Both vertical and horizontal margin for the dialog. */
#define MARGIN 1

/* Kinds of dialogs. */
typedef enum
{
	D_ERROR,              /* Error message.  All lines are left-aligned unless
	                         message is a single line that doesn't wrap, in which
	                         case it's centered. */
	D_QUERY_CENTER_EACH,  /* Query with each line centered on its own. */
	D_QUERY_CENTER_FIRST, /* Query with the first line centered. */
	D_QUERY_CENTER_BLOCK, /* Query with all lines centered as a block. */
}
Dialog;

/* Kinds of dialog results. */
typedef enum
{
	DR_OK,     /* Agreed. */
	DR_CANCEL, /* Cancelled. */
	DR_CLOSE,  /* Closed. */
	DR_YES,    /* Confirmed. */
	DR_NO,     /* Denied. */
	DR_CUSTOM, /* One of user-specified keys. */
}
DialogResult;

/* Internal information about a prompt. */
typedef struct
{
	const custom_prompt_t details; /* Essential parts of prompt description. */

	/* Configuration. */
	const Dialog kind;     /* Internal dialog kind. */
	const int prompt_skip; /* Whether to allow skipping future errors. */
	const int accept_mask; /* Mask of allowed DR_* results. */

	/* State. */
	int exit_requested;  /* Main loop quit flag. */
	DialogResult result; /* Dialog result. */
	char custom_result;  /* One of user-defined input keys. */
}
dialog_data_t;

static int def_handler(wchar_t key);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_esc(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_y(key_info_t key_info, keys_info_t *keys_info);
static void handle_response(dialog_data_t *data, DialogResult dr);
static void leave(dialog_data_t *data, DialogResult dr, char custom_result);
static int prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa);
static int prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip);
static void prompt_msg_internal(dialog_data_t *data);
static void enter(dialog_data_t *data);
static void redraw_error_msg(const dialog_data_t *data, int lazy);
static const char * get_control_msg(const dialog_data_t *data);
static const char * get_custom_control_msg(const response_variant responses[]);
static void draw_msg(const char title[], const char msg[],
		const char ctrl_msg[], int lines_to_center, int block_center,
		int recommended_width);
static size_t measure_sub_lines(const char msg[], int skip_empty,
		size_t *max_len);
static size_t measure_text_width(const char msg[]);
TSTATIC void dlg_set_callback(dlg_cb_f cb);

/* List of builtin key bindings. */
static keys_add_info_t builtin_cmds[] = {
	{WK_C_c, {{&cmd_ctrl_c}, .descr = "cancel"}},
	{WK_C_l, {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_C_m, {{&cmd_ctrl_m}, .descr = "agree to the query"}},
	{WK_ESC, {{&cmd_esc},    .descr = "close"}},
	{WK_n,   {{&cmd_n},      .descr = "deny the query"}},
	{WK_y,   {{&cmd_y},      .descr = "confirm the query"}},
};

/* Currently active dialog or NULL. */
static dialog_data_t *current_dialog;

/* Optional callback to be invoked when a dialog is requested. */
static dlg_cb_f dlg_cb;

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

	const response_variant *response = current_dialog->details.variants;
	while(response != NULL && response->key != '\0')
	{
		if(response->key == (char)key)
		{
			leave(current_dialog, DR_CUSTOM, key);
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
	handle_response(current_dialog, DR_CANCEL);
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
	handle_response(current_dialog, DR_OK);
}

/* Closes the query. */
static void
cmd_esc(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(current_dialog, DR_CLOSE);
}

/* Denies the query. */
static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(current_dialog, DR_NO);
}

/* Confirms the query. */
static void
cmd_y(key_info_t key_info, keys_info_t *keys_info)
{
	handle_response(current_dialog, DR_YES);
}

/* Processes users choice.  Leaves the mode if the result is found among the
 * list of expected results. */
static void
handle_response(dialog_data_t *data, DialogResult dr)
{
	/* Map result to corresponding input key to omit branching per handler. */
	static const char r_to_c[] = {
		[DR_OK]     = '\r',
		/* DR_CLOSE used to be part of DR_CANCEL, update handlers if there is a need
		 * to differentiate between them in handlers. */
		[DR_CANCEL] = NC_C_c,
		[DR_CLOSE]  = NC_C_c,
		[DR_YES]    = 'y',
		[DR_NO]     = 'n',
	};
	ARRAY_GUARD(r_to_c, DR_CUSTOM);

	(void)def_handler(r_to_c[dr]);
	/* Default handler might have already requested quitting. */
	if(!data->exit_requested)
	{
		if(data->accept_mask & MASK(dr))
		{
			leave(data, dr, /*custom_result=*/'?');
		}
	}
}

/* Leaves the mode with given result. */
static void
leave(dialog_data_t *data, DialogResult dr, char custom_result)
{
	data->result = dr;
	data->custom_result = custom_result;
	data->exit_requested = 1;
}

void
redraw_msg_dialog(int lazy)
{
	redraw_error_msg(/*data=*/NULL, lazy);
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

	if(dlg_cb != NULL)
	{
		(void)dlg_cb("error", title, message);
	}

	if(curr_stats.load_stage == 0)
		return 1;
	if(curr_stats.load_stage < 2 && skip_until_started)
		return 1;

	message = skip_whitespace(message);
	if(*message == '\0')
	{
		return 0;
	}

	dialog_data_t data = {
		.details = {
			.title = title,
			.message = message,
		},
		.kind = D_ERROR,
		.prompt_skip = prompt_skip,
		.accept_mask = MASK(DR_OK) | (prompt_skip ? MASK(DR_CANCEL) : 0),
	};
	enter(&data);

	if(curr_stats.load_stage < 2)
	{
		skip_until_started = (data.result == DR_CANCEL);
	}

	modes_redraw();

	return (data.result == DR_CANCEL);
}

int
prompt_msg(const char title[], const char message[])
{
	dialog_data_t data = {
		.details = {
			.title = title,
			.message = message,
		},
		.kind = D_QUERY_CENTER_EACH,
		.accept_mask = MASK(DR_YES, DR_NO),
	};
	prompt_msg_internal(&data);
	return (data.result == DR_YES);
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
prompt_msg_custom(const custom_prompt_t *details)
{
	assert(details->variants[0].key != '\0' &&
			"Variants should have at least one item.");

	dialog_data_t data = {
		.details = *details,
		.kind = details->block_center ? D_QUERY_CENTER_BLOCK : D_QUERY_CENTER_EACH,
	};

	prompt_msg_internal(&data);
	return data.custom_result;
}

/* Common implementation of prompt message. */
static void
prompt_msg_internal(dialog_data_t *data)
{
	enter(data);
	modes_redraw();
}

/* Enters the mode, which won't be left until one of expected results specified
 * by the mask is picked by the user. */
static void
enter(dialog_data_t *data)
{
	dialog_data_t *prev_dialog = current_dialog;
	const int prev_use_input_bar = curr_stats.use_input_bar;
	const vle_mode_t prev_mode = vle_mode_get();

	/* Message must be visible to the user for him to close it.  It won't happen
	 * without user interaction as new event loop is out of scope of a mapping
	 * that started it. */
	stats_unsilence_ui();

	ui_hide_graphics();

	current_dialog = data;
	curr_stats.use_input_bar = 0;
	vle_mode_set(MSG_MODE, VMT_SECONDARY);

	redraw_error_msg(data, /*lazy=*/0);

	data->exit_requested = 0;
	/* Avoid starting nested loop in tests. */
	if(curr_stats.load_stage > 0)
	{
		event_loop(&data->exit_requested, /*manage_marking=*/0);
	}
	else if(dlg_cb != NULL)
	{
		const custom_prompt_t *details = &data->details;
		DialogResult dr =
			dlg_cb("prompt", details->title, details->message) ? DR_YES : DR_NO;
		handle_response(current_dialog, dr);
	}

	vle_mode_set(prev_mode, VMT_SECONDARY);
	curr_stats.use_input_bar = prev_use_input_bar;
	current_dialog = prev_dialog;
}

/* Draws error message on the screen or redraws the last message when both
 * title_arg and message_arg are NULL. */
static void
redraw_error_msg(const dialog_data_t *data, int lazy)
{
	static const dialog_data_t *last_data;

	if(data == NULL)
	{
		data = last_data;
	}
	else
	{
		last_data = data;
	}
	assert(data != NULL && "Invalid dialog redraw request!");

	const char *message = data->details.message;
	char *free_me = NULL;
	if(data->details.make_message != NULL)
	{
		int sw, sh;
		getmaxyx(stdscr, sh, sw);
		int max_w = sw - 2 - 2 - MARGIN*2;
		int max_h = sh - 2 - ui_stat_height() - 2 - MARGIN*2;

		free_me = data->details.make_message(max_w, max_h, data->details.user_data);
		if(free_me == NULL)
		{
			return;
		}

		message = free_me;
	}

	const char *ctrl_msg = get_control_msg(data);
	int lines_to_center = (data->kind == D_QUERY_CENTER_EACH) ? INT_MAX
	                    : (data->kind == D_QUERY_CENTER_BLOCK) ? INT_MAX
	                    : (data->kind == D_QUERY_CENTER_FIRST) ? 1
	                    : 0;
	draw_msg(data->details.title, message, ctrl_msg, lines_to_center,
			data->details.block_center, /*recommended_width=*/0);

	free(free_me);

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
 * kind.  Returns the message. */
static const char *
get_control_msg(const dialog_data_t *data)
{
	if(data->kind != D_ERROR)
	{
		if(data->details.variants == NULL)
		{
			return "Enter [y]es or [n]o";
		}

		return get_custom_control_msg(data->details.variants);
	}

	if(data->prompt_skip)
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

	draw_msg(title, msg, ctrl_msg, /*lines_to_center=*/0, /*block_center=*/0,
			recommended_width);
	touch_all_windows();
	ui_refresh_win(error_win);
}

/* Draws formatted message with lines_to_center top lines centered with
 * specified title and control message on error_win. */
static void
draw_msg(const char title[], const char msg[], const char ctrl_msg[],
		int lines_to_center, int block_center, int recommended_width)
{
	/* TODO: refactor this function draw_msg() */

	int sw, sh;
	int w;
	size_t ctrl_msg_n;
	size_t wctrl_msg;
	int first_line_x = 1;
	const int first_line_y = 2;
	int block_margin;

	if(curr_stats.load_stage < 1)
	{
		return;
	}

	ui_set_cursor(/*visibility=*/0);

	getmaxyx(stdscr, sh, sw);

	ctrl_msg_n = MAX(measure_sub_lines(ctrl_msg, /*skip_empty=*/0, &wctrl_msg),
	                 1U);

	int wmsg = measure_text_width(msg);

	/* We start with maximum height and reduce is later. */
	int max_h = sh - 2 - ui_stat_height();
	/* The outermost condition is for VLA below (to calm static analyzers). */
	w = MAX(2 + 2*MARGIN, MIN(sw - 2,
	        MAX(MAX(recommended_width, sw/3), MAX((int)wctrl_msg, wmsg) + 4)));
	wresize(error_win, max_h, w);

	werase(error_win);

	block_margin = 0;
	if(block_center)
	{
		block_margin = MAX(w - wmsg, 2 + 2*MARGIN)/2;
	}

	if(strchr(msg, '\n') == NULL && wmsg <= w - 2 - 2*MARGIN)
	{
		lines_to_center = 1;
	}

	const char *curr = msg;
	int cy = first_line_y;
	while(*curr != '\0')
	{
		int max_width = w - 2 - 2*MARGIN;

		/* Screen line stops at new line or when there is no more space. */
		const char *nl = until_first(curr, '\n');
		const char *sl = curr + utf8_strsnlen(curr, max_width);
		const char *end = (nl < sl ? nl : sl);

		char buf[max_width*10 + 1];
		copy_str(buf, MIN((int)sizeof(buf), end - curr + 1), curr);
		curr = (end[0] == '\n' ? end + 1 : end);
		if(buf[0] == '\0')
			continue;

		if(cy >= max_h - (int)ctrl_msg_n - 3)
		{
			/* Skip trailing part of the message if it's too long, just print how
			 * many lines we're omitting. */
			size_t max_len;
			int more_lines = 1U + measure_sub_lines(curr, /*skip_empty=*/1, &max_len);
			if(more_lines > 1)
			{
				snprintf(buf, sizeof(buf), "<<%d more lines not shown>>", more_lines);
				/* Make sure this is the last iteration of the loop. */
				curr += strlen(curr);
			}
		}

		int cx = 1 + MARGIN;
		if(lines_to_center-- > 0)
		{
			if(block_center)
			{
				cx = block_margin;
			}
			else
			{
				cx = (w - utf8_strsw(buf))/2;
			}
		}

		if(cy == first_line_y)
		{
			first_line_x = cx;
		}
		checked_wmove(error_win, cy++, cx);
		wprint(error_win, buf);
	}

	int h = 1 + cy + ctrl_msg_n + 1;
	wresize(error_win, h, w);
	mvwin(error_win, (sh - h)/2, (sw - w)/2);

	box(error_win, 0, 0);
	if(title[0] != '\0')
	{
		mvwprintw(error_win, 0, (w - strlen(title) - 2)/2, " %s ", title);
	}

	int ctrl_msg_width = measure_text_width(ctrl_msg);
	block_margin = MAX(w - ctrl_msg_width, 2 + 2*MARGIN)/2;

	/* Print control message line by line. */
	size_t ctrl_i = ctrl_msg_n;
	while(1)
	{
		const size_t len = strcspn(ctrl_msg, "\n");
		mvwaddnstr(error_win, h - ctrl_i - 1, block_margin, ctrl_msg, len);

		if(--ctrl_i == 0)
		{
			break;
		}

		ctrl_msg += len + (ctrl_msg[len] == '\n' ? 1 : 0);
	}

	checked_wmove(error_win, first_line_y, first_line_x);
}

/* Counts number of sub-lines (separated by new-line character in the msg.  Sets
 * *max_len to the length of the longest sub-line.  Returns total number of
 * sub-lines, which can be zero is msg is an empty line. */
static size_t
measure_sub_lines(const char msg[], int skip_empty, size_t *max_len)
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
		/* Empty lines are not displayed. */
		if(len != 0 || !skip_empty)
		{
			++nlines;
		}
		msg += len + (msg[len] == '\n' ? 1U : 0U);
	}
	return nlines;
}

/* Measures maximum on-screen width of a line in a message.  Returns the
 * width. */
static size_t
measure_text_width(const char msg[])
{
	size_t max_width = 0U;
	while(*msg != '\0')
	{
		const size_t len = strcspn(msg, "\n");
		const size_t width = utf8_nstrsw(msg, len);
		if(width > max_width)
		{
			max_width = width;
		}
		msg += len + (msg[len] == '\n' ? 1U : 0U);
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

	dialog_data_t data = {
		.details = {
			.title = title,
			.message = msg,
		},
		.kind = D_QUERY_CENTER_FIRST,
		.accept_mask = MASK(DR_YES, DR_NO),
	};
	prompt_msg_internal(&data);
	free(msg);

	if(data.result != DR_YES)
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

TSTATIC void
dlg_set_callback(dlg_cb_f cb)
{
	dlg_cb = cb;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
