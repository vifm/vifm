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

#include <fcntl.h> /* F_GETFL F_SETFL O_NONBLOCK fcntl() */
#include <unistd.h> /* close() pipe() read() write() */

#include <stdlib.h> /* free() malloc() */

/* Event object data. */
struct event_t
{
	int r; /* Read end of the pipe. */
	int w; /* Write end of the pipe. */
};

event_t *
event_alloc(void)
{
	event_t *event = malloc(sizeof(*event));
	if(event == NULL)
	{
		return NULL;
	}

	int fds[2];
	if(pipe(fds) != 0)
	{
		free(event);
		return NULL;
	}

	event->r = fds[0];
	event->w = fds[1];

	/* Make reading from the pipe to not block. */
	int flags = fcntl(event->r, F_GETFL, 0);
	if(flags == -1)
	{
		event_free(event);
		return NULL;
	}
	if(fcntl(event->r, F_SETFL, flags | O_NONBLOCK) != 0)
	{
		event_free(event);
		return NULL;
	}

	return event;
}

void
event_free(event_t *event)
{
	if(event != NULL)
	{
		close(event->r);
		close(event->w);
		free(event);
	}
}

int
event_signal(event_t *event)
{
	char buf = '\0';
	if(write(event->w, &buf, sizeof(buf)) != sizeof(buf))
	{
		return -1;
	}
	return 0;
}

int
event_reset(event_t *event)
{
	char buf = '\0';
	if(read(event->r, &buf, sizeof(buf)) != sizeof(buf))
	{
		/* Error if we couldn't read anything for whatever reason. */
		return -1;
	}

	ssize_t n;
	while((n = read(event->r, &buf, sizeof(buf))) == sizeof(buf))
	{
		/* Keep reading. */
	}
	/* Don't report an error on EOF (when nothing is read). */
	return (n == 0 ? 0 : -1);
}

int
event_wait_end(const event_t *event)
{
	return event->r;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
