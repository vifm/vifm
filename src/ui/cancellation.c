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

#include <curses.h> /* noraw() raw() */

#include <assert.h> /* assert() */

#include "../utils/cancellation.h"
#include "../utils/int_stack.h"
#include "../status.h"

/* State of cancellation request processing. */
typedef enum
{
	CRS_DISABLED,           /* Cancellation is disabled. */
	CRS_DISABLED_REQUESTED, /* Cancellation is disabled and was requested. */
	CRS_ENABLED,            /* Cancellation is enabled, but wasn't requested. */
	CRS_ENABLED_REQUESTED,  /* Cancellation is enabled and was requested. */
}
cancellation_request_state;

static int ui_cancellation_hook(void *arg);
static int ui_cancellation_enabled(void);
static int is_enabled(cancellation_request_state state);

const cancellation_t ui_cancellation_info = { .hook = &ui_cancellation_hook };

/* Whether cancellation was requested.  Used by ui_cancellation_* group of
 * functions. */
static cancellation_request_state cancellation_state;

/* Previous cancellation states. */
static int_stack_t cancellation_stack;

/* Implementation of cancellation hook for. */
static int
ui_cancellation_hook(void *arg)
{
	return ui_cancellation_requested();
}

void
ui_cancellation_push_on(void)
{
	int_stack_push(&cancellation_stack, cancellation_state);

	if(!ui_cancellation_enabled())
	{
		ui_cancellation_enable();
	}

	cancellation_state = CRS_ENABLED;
}

void
ui_cancellation_push_off(void)
{
	int_stack_push(&cancellation_stack, cancellation_state);

	if(ui_cancellation_enabled())
	{
		ui_cancellation_disable();
	}

	cancellation_state = CRS_DISABLED;
}

void
ui_cancellation_enable(void)
{
	/* TODO: consider enabling this assert:
	assert(!int_stack_is_empty(&cancellation_stack) && "Must push state first.");
	*/
	assert(!ui_cancellation_enabled() && "Can't enable twice in a row.");

	cancellation_state = (cancellation_state == CRS_DISABLED)
	                   ? CRS_ENABLED
	                   : CRS_ENABLED_REQUESTED;

	/* The check is here for tests, which are running with uninitialized
	 * curses. */
	if(curr_stats.load_stage > 2)
	{
		/* Temporary disable raw mode of terminal so that Ctrl-C is handled as
		 * SIGINT signal rather than as regular input character. */
		noraw();
	}
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
	/* TODO: consider enabling this assert:
	assert(!int_stack_is_empty(&cancellation_stack) && "Must push state first.");
	*/
	assert(ui_cancellation_enabled() && "Can't disable what disabled.");

	/* The check is here for tests, which are running with uninitialized
	 * curses. */
	if(curr_stats.load_stage > 2)
	{
		/* Restore raw mode of terminal so that Ctrl-C is handled as regular input
		 * character rather than as SIGINT signal. */
		raw();
	}

	cancellation_state = (cancellation_state == CRS_ENABLED_REQUESTED)
	                   ? CRS_DISABLED_REQUESTED
	                   : CRS_DISABLED;
}

void
ui_cancellation_pop(void)
{
	assert(!int_stack_is_empty(&cancellation_stack) && "Underflow.");

	cancellation_request_state next = int_stack_get_top(&cancellation_stack);

	if(ui_cancellation_enabled())
	{
		if(!is_enabled(next))
		{
			ui_cancellation_disable();
		}
	}
	else
	{
		if(is_enabled(next))
		{
			ui_cancellation_enable();
		}
	}

	int_stack_pop(&cancellation_stack);
	cancellation_state = next;
}

/* Checks whether cancellation processing is enabled.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
ui_cancellation_enabled(void)
{
	return is_enabled(cancellation_state);
}

/* Checks if state is an enabled state.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
is_enabled(cancellation_request_state state)
{
	return state == CRS_ENABLED
	    || state == CRS_ENABLED_REQUESTED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
