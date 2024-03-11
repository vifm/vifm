/* vifm
 * Copyright (C) 2024 xaizek.
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

#ifndef VIFM__UTILS__EVENT_H__
#define VIFM__UTILS__EVENT_H__

/* This unit is meant to provide abstraction over platform-specific mechanism
 * for interrupting select()-like API call.  It implements a manual-reset
 * event. */

/* Type of event object handle. */
#ifdef _WIN32
#include <windows.h>
typedef HANDLE event_end_t;
#else
typedef int event_end_t;
#endif

/* Opaque event type. */
typedef struct event_t event_t;

/* Allocates new event object in non-signaled state.  Returns the event or NULL
 * on error. */
event_t * event_alloc(void);

/* Frees the event.  The parameter can be NULL. */
void event_free(event_t *event);

/* Switches the event to the signaled state.  Returns zero on success. */
int event_signal(event_t *event);

/* Switches the event back to the non-signaled state.  Should be called after
 * finding event in the signaled state.  Returns zero on success. */
int event_reset(event_t *event);

/* Retrieves value that can be used to wait until the event reaches signaled
 * state. */
event_end_t event_wait_end(const event_t *event);

#endif /* VIFM__UTILS__EVENT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
