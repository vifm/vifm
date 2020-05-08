/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "selector.h"

#include <sys/select.h> /* FD_* fd_set select() */

#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memcpy() */

/* Selector object. */
struct selector_t
{
	fd_set set;   /* Set of selectors to check. */
	fd_set ready; /* Set of ready selectors after successful check. */
	int max_fd;   /* Maximal value among descriptors in the set. */
};

selector_t *
selector_alloc(void)
{
	selector_t *selector = malloc(sizeof(*selector));
	if(selector != NULL)
	{
		selector_reset(selector);
	}
	return selector;
}

void
selector_free(selector_t *selector)
{
	free(selector);
}

void
selector_reset(selector_t *selector)
{
	FD_ZERO(&selector->set);
	FD_ZERO(&selector->ready);
	selector->max_fd = -1;
}

void
selector_add(selector_t *selector, selector_item_t item)
{
	FD_SET(item, &selector->set);
	if(item > selector->max_fd)
	{
		selector->max_fd = item;
	}
}

void
selector_remove(selector_t *selector, selector_item_t item)
{
	FD_CLR(item, &selector->set);
	if(item == selector->max_fd)
	{
		/* Could do more here. */
		--selector->max_fd;
	}
}

int
selector_wait(selector_t *selector, int delay)
{
	if(delay < 0)
	{
		delay = 0;
	}

	memcpy(&selector->ready, &selector->set, sizeof(selector->ready));

	struct timeval ts = { .tv_sec = delay/1000, .tv_usec = (delay%1000)*1000 };
	int r = (select(selector->max_fd + 1, &selector->ready, NULL, NULL, &ts) > 0);
	if(!r)
	{
		FD_ZERO(&selector->ready);
	}
	return r;
}

int
selector_is_ready(selector_t *selector, selector_item_t item)
{
	return FD_ISSET(item, &selector->ready);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
