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

/* Managing operation cancellation.  Typical usage:
 *
 *   ui_cancellation_reset();
 *   ui_cancellation_enable();
 *   ...
 *   Some code possibly checking ui_cancellation_requested() or just interacting
 *   with child process which will get Ctrl-C request from the user.
 *   ...
 *   ui_cancellation_disable(); */

/* Resets state so that ui_cancellation_requested() returns zero. */
void ui_cancellation_reset(void);

/* Enables handling of cancellation requests through the UI. */
void ui_cancellation_enable(void);

/* External callback for notifying this unit about cancellation request.  Should
 * be called between ui_cancellation_enable() and ui_cancellation_disable(). */
void ui_cancellation_request(void);

/* If cancellation is enabled, checks whether cancelling of current operation is
 * requested.  Should be called between ui_cancellation_enable() and
 * ui_cancellation_disable().  Returns non-zero if so, otherwise zero is
 * returned. */
int ui_cancellation_requested(void);

/* Disables handling of cancellation requests through the UI. */
void ui_cancellation_disable(void);

/* Pauses cancellation if it's active, otherwise does nothing.  This effectively
 * prevents cancellation from affecting environment until cancellation is
 * restored.  Returns state to be passed to ui_cancellation_resume() later. */
int ui_cancellation_pause(void);

/* Restores cancellation paused via ui_cancellation_pause(). */
void ui_cancellation_resume(int state);

#endif /* VIFM__UI__CANCELLATION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
