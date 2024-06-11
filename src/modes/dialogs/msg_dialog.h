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

#ifndef VIFM__MODES__DIALOGS__MSG_DIALOG_H__
#define VIFM__MODES__DIALOGS__MSG_DIALOG_H__

#include "../../utils/macros.h"
#include "../../utils/test_helpers.h"

#include <stdio.h> /* FILE */

/* Input data for prompt_msg_custom() that describes prompt. */
typedef struct custom_prompt_t
{
	const char *title;   /* Dialog's title. */
	const char *message; /* Dialog's body, can be NULL if make_message isn't. */

	/* If non-NULL, used to generate dialog's body dynamically.  Should return a
	 * newly allocated string or NULL on error in which case the prompt is
	 * aborted. */
	char * (*make_message)(int max_w, int max_h, void *data);
	/* Extra argument for make_message(). */
	void *user_data;

	/* Should be terminated with a record filled with zeroes.  The array must
	 * contain at least one item.  Use '\r' key to handle Enter and '\x03' to
	 * handle cancellation (both Ctrl-C and Escape).  Elements with empty lines
	 * instead of descriptions are not displayed. */
	const struct response_variant *variants;
	/* Whether message lines should be centered as a block. */
	int block_center;
}
custom_prompt_t;

/* Definition of a dialog option. */
typedef struct response_variant
{
	char key;          /* Corresponding key. */
	const char *descr; /* Description to be displayed. */
}
response_variant;

/* Initializes message dialog mode. */
void init_msg_dialog_mode(void);

/* Redraws currently visible error message on the screen. */
void redraw_msg_dialog(int lazy);

/* Shows error message to a user. */
void show_error_msg(const char title[], const char message[]);

/* Same as show_error_msg(...), but with format. */
void show_error_msgf(const char title[], const char format[], ...)
	_gnuc_printf(2, 3);

/* Same as show_error_msg(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msg(const char title[], const char message[]);

/* Same as show_error_msgf(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msgf(const char title[], const char format[], ...)
	_gnuc_printf(2, 3);

/* Asks user to confirm some action by answering "Yes" or "No".  Returns
 * non-zero when user answers yes, otherwise zero is returned. */
int prompt_msg(const char title[], const char message[]);

/* Same as prompt_msgf(...), but with format.  Returns non-zero when user
 * answers yes, otherwise zero is returned. */
int prompt_msgf(const char title[], const char format[], ...)
	_gnuc_printf(2, 3);

/* Same as prompt_msg() but with custom list of options.  Returns one of the
 * keys defined in the array of details->variants. */
char prompt_msg_custom(const custom_prompt_t *details);

/* Draws centered formatted message with specified title and control message on
 * error_win. */
void draw_msgf(const char title[], const char ctrl_msg[], int recommended_width,
		const char format[], ...) _gnuc_printf(4, 5);

/* Checks with the user that deletion is permitted.  Returns non-zero if so,
 * otherwise zero is returned. */
int confirm_deletion(char *files[], int nfiles, int use_trash);

/* Reads contents of the file and displays it in series of dialog messages.  ef
 * can be NULL.  Closes ef. */
void show_errors_from_file(FILE *ef, const char title[]);

/* Type of callback function that can be used in tests.  The type parameter
 * allows to differentiate between different kinds of dialogs.  Should return
 * non-zero to agree to a prompt. */
typedef int (*dlg_cb_f)(const char type[], const char title[],
		const char message[]);

TSTATIC_DEFS(
	/* Sets callback to be invoked when a dialog is requested. */
	void dlg_set_callback(dlg_cb_f cb);
)

#endif /* VIFM__MODES__DIALOGS__MSG_DIALOG_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
