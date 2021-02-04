/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__UTILS__FSWATCH_H__
#define VIFM__UTILS__FSWATCH_H__

/* Implementation of file system changes checks via polling. */

/* Kinds of state reports. */
typedef enum
{
	FSWS_UNCHANGED, /* No update or some change. */
	FSWS_UPDATED,   /* A change of the state. */
	FSWS_ERRORED,   /* An error occurred. */
	FSWS_REPLACED   /* Path target has been replaced. */
}
FSWatchState;

/* Opaque type of a watcher. */
typedef struct fswatch_t fswatch_t;

/* Creates new watcher for the specified path.  Returns the watcher or NULL on
 * error. */
fswatch_t * fswatch_create(const char path[]);

/* Frees a watcher.  w can be NULL. */
void fswatch_free(fswatch_t *w);

/* Checks whether any changes were made to the entity being watched since last
 * query.  Returns latest state. */
FSWatchState fswatch_poll(fswatch_t *w);

#endif /* VIFM__UTILS__FSWATCH_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
