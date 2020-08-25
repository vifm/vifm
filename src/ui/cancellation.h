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

#ifndef VIFM__UI__CANCELLATION_H__
#define VIFM__UI__CANCELLATION_H__

/* Managing foreground operation cancellation.  Usage example:
 *
 *   ui_cancellation_push_on();
 *   ...
 *   Some code possibly checking ui_cancellation_requested() or just interacting
 *   with child process which will get Ctrl-C request from the user.
 *   ...
 *     ui_cancellation_push_off();
 *     ...
 *     Can receive Ctrl-C as input or call ui_cancellation_requested() here.
 *     ...
 *     ui_cancellation_enable();
 *       ...
 *       Some code possibly checking ui_cancellation_requested() or just
 *       interacting with child process which will get Ctrl-C request from the
 *       user.
 *       ...
 *     ui_cancellation_disable();
 *     ...
 *     Can receive Ctrl-C as input or call ui_cancellation_requested() here.
 *     ...
 *     ui_cancellation_pop();
 *   ...
 *   Some code possibly checking ui_cancellation_requested() or just interacting
 *   with child process which will get Ctrl-C request from the user.
 *   ...
 *   ui_cancellation_pop();
 *   ...
 *   Can call ui_cancellation_requested() here.
 *
 * As the example demonstrates cancellation states can be stacked to do not
 * affect each other. */

/* Convenience object for cases when UI cancellation is in use. */
extern const struct cancellation_t ui_cancellation_info;

/* Pushes new state making sure that cancellation is enabled after the call. */
void ui_cancellation_push_on(void);

/* Pushes new state making sure that cancellation is disabled after the call. */
void ui_cancellation_push_off(void);

/* Enables handling of cancellation requests as interrupts. */
void ui_cancellation_enable(void);

/* External callback for notifying this unit about cancellation request.  Should
 * be called when cancellation is enabled. */
void ui_cancellation_request(void);

/* If cancellation is enabled, checks whether cancelling current operation is
 * requested.  Can be called when cancellation is disabled.  Returns non-zero if
 * so, otherwise zero is returned. */
int ui_cancellation_requested(void);

/* Disables handling of cancellation requests as interrupts. */
void ui_cancellation_disable(void);

/* Removes the topmost state from the stack restoring previous state. */
void ui_cancellation_pop(void);

#endif /* VIFM__UI__CANCELLATION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
