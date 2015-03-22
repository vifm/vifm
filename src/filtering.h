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

/* Generic filters related functions. */

void set_dot_files_visible(FileView *view, int visible);

void toggle_dot_files(FileView *view);

void filter_selected_files(FileView *view);

void remove_filename_filter(FileView *view);

void restore_filename_filter(FileView *view);

/* Toggles filter inversion state of the view.  Reloads filelist and resets
 * cursor position. */
void toggle_filter_inversion(FileView *view);

/* Checks whether file/directory passes filename filters of the view.  Returns
 * non-zero if given filename passes filter and should be visible, otherwise
 * zero is returned, in which case the file should be hidden. */
int file_is_visible(FileView *view, const char filename[], int is_dir);

/* Local filter related functions. */

/* Sets regular expression of the local filter for the view.  First call of this
 * function initiates filter set process, which should be ended by call to
 * local_filter_accept() or local_filter_cancel(). */
void local_filter_set(FileView *view, const char filter[]);

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

#endif /* VIFM__FILTERING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
