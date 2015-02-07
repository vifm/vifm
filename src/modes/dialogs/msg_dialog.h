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

#include "../../ui/ui.h"

#include <stdio.h> /* FILE */

/* Initializes message dialog mode. */
void init_msg_dialog_mode(void);

/* Redraws currently visible error message on the screen. */
void redraw_msg_dialog(void);

/* Shows error message to a user. */
void show_error_msg(const char title[], const char message[]);

/* Same as show_error_msg(...), but with format. */
void show_error_msgf(const char title[], const char format[], ...);

/* Same as show_error_msg(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msg(const char title[], const char message[]);

/* Same as show_error_msgf(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msgf(const char title[], const char format[], ...);

/* Asks user to confirm some action by answering "Yes" or "No".  Returns
 * non-zero when user answers yes, otherwise zero is returned. */
int query_user_menu(const char title[], const char message[]);

/* Checks with the user that deletion is permitted.  Returns non-zero if so,
 * otherwise zero is returned. */
int confirm_deletion(int use_trash);

/* Reads contents of the file and displays it in series of dialog messages.  ef
 * can be NULL.  Closes ef. */
void show_errors_from_file(FILE *ef);

#endif /* VIFM__MODES__DIALOGS__MSG_DIALOG_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
