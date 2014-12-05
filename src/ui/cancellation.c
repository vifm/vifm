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

#include "cancellation.h"

#include <curses.h> /* mvwin() wbkgdset() werase() */

#include <assert.h> /* assert() */

static int ui_cancellation_enabled(void);
static int ui_cancellation_disabled(void);

/* State of cancellation request processing. */
typedef enum
{
	CRS_DISABLED,           /* Cancellation is disabled. */
	CRS_DISABLED_REQUESTED, /* Cancellation is disabled and was requested. */
	CRS_ENABLED,            /* Cancellation is enabled, but wasn't requested. */
	CRS_ENABLED_REQUESTED,  /* Cancellation is enabled and was requested. */
}
cancellation_request_state;

/* Whether cancellation was requested.  Used by ui_cancellation_* group of
 * functions. */
static cancellation_request_state cancellation_state;

void
ui_cancellation_reset(void)
{
	assert(ui_cancellation_disabled() && "Can't reset while active.");

	cancellation_state = CRS_DISABLED;
}

void
ui_cancellation_enable(void)
{
	assert(ui_cancellation_disabled() && "Can't enable twice in a row.");

	cancellation_state = (cancellation_state == CRS_DISABLED)
	                   ? CRS_ENABLED
	                   : CRS_ENABLED_REQUESTED;

	/* Temporary disable raw mode of terminal so that Ctrl-C is handled as SIGINT
	 * signal rather than as regular input character. */
	noraw();
}

void
ui_cancellation_request(void)
{
	if(ui_cancellation_enabled())
	{
		cancellation_state = CRS_ENABLED_REQUESTED;
	}
}

int
ui_cancellation_requested(void)
{
	return cancellation_state == CRS_ENABLED_REQUESTED
	    || cancellation_state == CRS_DISABLED_REQUESTED;
}

void
ui_cancellation_disable(void)
{
	assert(ui_cancellation_enabled() && "Can't disable what disabled.");

	/* Restore raw mode of terminal so that Ctrl-C is be handled as regular input
	 * character rather than as SIGINT signal. */
	raw();

	cancellation_state = (cancellation_state == CRS_ENABLED_REQUESTED)
	                   ? CRS_DISABLED_REQUESTED
	                   : CRS_DISABLED;
}

/* Checks whether cancellation processing is enabled.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
ui_cancellation_enabled(void)
{
	return cancellation_state == CRS_ENABLED
	    || cancellation_state == CRS_ENABLED_REQUESTED;
}

/* Checks whether cancellation processing is disabled.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
ui_cancellation_disabled(void)
{
	return !ui_cancellation_enabled();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
