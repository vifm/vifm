/* vifm
 * Copyright (C) 2016 xaizek.
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

#ifndef VIFM__UTILS__CANCELLATION_H__
#define VIFM__UTILS__CANCELLATION_H__

/* Generic operation cancellation mechanism. */

/* Type of hook responsible for querying cancellation status.  Should return
 * non-zero if operation is to be cancelled and zero otherwise. */
typedef int (*cancellation_hook)(void *arg);

/* Cancellation settings. */
typedef struct cancellation_t
{
	cancellation_hook hook; /* Hook to query cancellation state. */
	void *arg;              /* Parameter for the hook. */
}
cancellation_t;

/* Convenience object for cases when cancellation is not available. */
extern const cancellation_t no_cancellation;

/* Checks whether operation was cancelled via cancellation hook.  Returns
 * non-zero if so, otherwise zero is returned.  */
int cancellation_requested(const cancellation_t *info);

/* Checks whether cancellation is enabled (i.e. hook is provided).  Returns
 * non-zero if so, otherwise zero is returned. */
int cancellation_possible(const cancellation_t *info);

#endif /* VIFM__UTILS__CANCELLATION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
