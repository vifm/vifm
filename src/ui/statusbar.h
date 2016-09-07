/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__UI__STATUSBAR_H__
#define VIFM__UI__STATUSBAR_H__

#include "../utils/macros.h"

/* Managing status bar. */

/* Clears the status bar. */
void ui_sb_clear(void);

/* Immediately (UI is updated) displays message on the status bar without
 * storing it in message history. */
void ui_sb_quick_msgf(const char format[], ...) _gnuc_printf(1, 2);

/* Clears message displayed by ui_sb_quick_msgf(). */
void ui_sb_quick_msg_clear(void);

/* Repeats last message if message is NULL. */
void status_bar_message(const char message[]);

void status_bar_messagef(const char format[], ...) _gnuc_printf(1, 2);

void status_bar_error(const char message[]);

void status_bar_errorf(const char message[], ...) _gnuc_printf(1, 2);

int is_status_bar_multiline(void);

#endif /* VIFM__UI__STATUSBAR_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
