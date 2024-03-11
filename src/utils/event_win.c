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

#include "event.h"

#include <windows.h>

#include <stdlib.h> /* free() malloc() */

/* Event object data. */
struct event_t
{
	HANDLE h; /* Handle to the event object. */
};

event_t *
event_alloc(void)
{
	event_t *event = malloc(sizeof(*event));
	if(event == NULL)
	{
		return NULL;
	}

	event->h = CreateEvent(/*attrs=*/NULL, /*manual_reset=*/TRUE,
			/*initial_state=*/FALSE, /*name=*/NULL);

	if(event->h == NULL)
	{
		free(event);
		return NULL;
	}

	return event;
}

void
event_free(event_t *event)
{
	if(event != NULL)
	{
		CloseHandle(event->h);
		free(event);
	}
}

int
event_signal(event_t *event)
{
	return (SetEvent(event->h) ? 0 : -1);
}

int
event_reset(event_t *event)
{
	return (ResetEvent(event->h) ? 0 : -1);
}

HANDLE
event_wait_end(const event_t *event)
{
	return event->h;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
