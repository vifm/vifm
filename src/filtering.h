/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__FILTERING_H__
#define VIFM__FILTERING_H__

#include "ui/ui.h"

/* Default value of case sensitivity for filters. */
#ifdef _WIN32
#define FILTER_DEF_CASE_SENSITIVITY 0
#else
#define FILTER_DEF_CASE_SENSITIVITY 1
#endif

/* Initialization/termination functions. */

void filters_view_reset(FileView *view);

/* Generic filters related functions. */

/* Toggles filter inversion state of the view.  Reloads filelist and resets
 * cursor position. */
void filters_invert(FileView *view);

/* Filters out nodes that bear "temporary" mark.  The entries can be NULL,
 * otherwise it's a memory to be used for the new list as an optimization. */
void filters_drop_temporaries(FileView *view, dir_entry_t entries[]);

/* Callback-like function which triggers some view-specific updates after
 * directory of the view changes. */
void filters_dir_updated(FileView *view);

/* Checks whether file/directory passes filename filters of the view.  Returns
 * non-zero if given name passes filters and should be visible, otherwise zero
 * is returned, in which case the file should be hidden. */
int filters_file_is_visible(FileView *view, const char dir[], const char name[],
		int is_dir, int apply_local_filter);

/* Dot filter related functions. */

/* Sets new value of the dot files filter of the view.  Performs updates of the
 * view and options. */
void dot_filter_set(FileView *view, int visible);

/* Toggles state of the dot files filter of the view.  Performs updates of the
 * view and options. */
void dot_filter_toggle(FileView *view);

/* Filename filters related functions. */

/* Adds currently selected entries of the view or just current file (if no file
 * is selected) to auto filename filter.  Performs necessary view updates. */
void name_filters_add_selection(FileView *view);

/* Clears both manual and auto filename filters remembering their state for
 * future restoration.  Does nothing if both filters are already reset.
 * Performs necessary view updates. */
void name_filters_remove(FileView *view);

/* Checks whether file name filter of the view is empty.  Returns non-zero if
 * so, and zero otherwise. */
int name_filters_empty(FileView *view);

/* Clears filename filter dropping (not remembering) its previous state. */
void name_filters_drop(FileView *view);

/* Restores values of manual and auto filename filters (including state of
 * filter inversion).  Schedules view update. */
void name_filters_restore(FileView *view);

/* Local filter related functions. */

/* Sets regular expression of the local filter for the view.  First call of this
 * function initiates filter set process, which should be ended by call to
 * local_filter_accept() or local_filter_cancel().  Returns zero if not all
 * files are filtered out, -1 if filter expression is incorrect and 1 if all
 * files were filtered out. */
int local_filter_set(FileView *view, const char filter[]);

/* Updates cursor position and top line of the view according to interactive
 * local filter in progress. */
void local_filter_update_view(FileView *view, int rel_pos);

/* Accepts current value of local filter. */
void local_filter_accept(FileView *view);

/* Sets local filter non-interactively. */
void local_filter_apply(FileView *view, const char filter[]);

/* Cancels local filter set process.  Restores previous values of the filter. */
void local_filter_cancel(FileView *view);

/* Removes local filter after storing its current value to make restore
 * operation possible. */
void local_filter_remove(FileView *view);

/* Restores previously removed local filter. */
void local_filter_restore(FileView *view);

/* Checks whether given entry matches currently set local filter.  Returns
 * non-zero if file should be left in the view, and zero otherwise. */
int local_filter_matches(FileView *view, const dir_entry_t *entry);

#endif /* VIFM__FILTERING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
