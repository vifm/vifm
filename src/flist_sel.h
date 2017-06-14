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

#ifndef VIFM__FLIST_SELECT_H__
#define VIFM__FLIST_SELECT_H__

/* This unit provides functions related to selecting items in file lists. */

#include "ui/ui.h"
#include "registers.h"

/* Clears selection saving it for later use. */
void flist_sel_stash(FileView *view);

/* Clears selection forgetting not saving it or overwriting last stash. */
void flist_sel_drop(FileView *view);

/* Callback-like function which triggers some selection updates after view
 * reload. */
void flist_sel_view_reloaded(FileView *view, int location_changed);

/* Inverts selection of files in the view. */
void flist_sel_invert(FileView *view);

/* Removes selection of a view saving current one, but does nothing if none
 * files are selected.  Handles view redrawing. */
void flist_sel_stash_if_nonempty(FileView *view);

/* Reselects previously selected entries.  When reg is NULL, saved selection is
 * restored, otherwise list of files to restore is taken from the register. */
void flist_sel_restore(FileView *view, reg_t *reg);

/* Counts number of selected files and writes saves the number in
 * view->selected_files. */
void flist_sel_recount(FileView *view);

/* Selects or unselects entries in the given range. */
void flist_sel_by_range(FileView *view, int begin, int end, int select);

/* Selects or unselects entries that match list of files supplied by external
 * utility.  Returns zero on success, otherwise non-zero is returned and error
 * message is printed on statusbar. */
int flist_sel_by_filter(FileView *view, const char cmd[], int erase_old,
		int select);

/* Selects or unselects entries that match given pattern.  Returns zero on
 * success, otherwise non-zero is returned and error message is printed on
 * statusbar. */
int flist_sel_by_pattern(FileView *view, const char pattern[], int erase_old,
		int select);

/* Selects up to count elements starting at position at (or current cursor
 * position, if at is negative). */
void flist_sel_count(FileView *view, int at, int count);

/* Selects entries in the range [begin; end].  If begin isn't given (negative),
 * end is selected.  Otherwise, if end is negative and select_current is
 * non-zero, current item is selected.  Returns non-zero if something was
 * selected by this method, otherwise zero is returned. */
int flist_sel_range(FileView *view, int begin, int end, int select_current);

#endif /* VIFM__FLIST_SELECT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
