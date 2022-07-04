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

#include "registers.h"

struct view_t;

/* Clears selection saving it for later use. */
void flist_sel_stash(struct view_t *view);

/* Clears selection forgetting not saving it or overwriting last stash. */
void flist_sel_drop(struct view_t *view);

/* Callback-like function which triggers some selection updates when view
 * reload or directory change occurs.  new_dir should be NULL if view is
 * reloaded. */
void flist_sel_view_to_reload(struct view_t *view, const char new_dir[]);

/* Inverts selection of files in the view. */
void flist_sel_invert(struct view_t *view);

/* Removes selection of a view saving current one, but does nothing if none
 * files are selected.  Handles view redrawing. */
void flist_sel_stash_if_nonempty(struct view_t *view);

/* Reselects previously selected entries.  When reg is NULL, saved selection is
 * restored, otherwise list of files to restore is taken from the register. */
void flist_sel_restore(struct view_t *view, reg_t *reg);

/* Counts number of selected files and writes saves the number in
 * view->selected_files. */
void flist_sel_recount(struct view_t *view);

/* Selects or unselects entries in the given range. */
void flist_sel_by_range(struct view_t *view, int begin, int end, int select);

/* Selects or unselects entries that match list of files supplied by external
 * utility.  Returns zero on success, otherwise non-zero is returned and error
 * message is printed on status bar. */
int flist_sel_by_filter(struct view_t *view, const char cmd[], int erase_old,
		int select);

/* Selects or unselects entries that match given pattern.  Returns zero on
 * success, otherwise non-zero is returned and error message is printed on
 * status bar. */
int flist_sel_by_pattern(struct view_t *view, const char pattern[],
		int erase_old, int select);

/* Selects up to count elements starting at position at (or current cursor
 * position, if at is negative). */
void flist_sel_count(struct view_t *view, int at, int count);

/* Marks entries in the range [begin; end].  If begin isn't given (negative),
 * end is marked.  Otherwise, if end is negative and mark_current is non-zero,
 * current item is marked.  Returns non-zero if marking was set up, otherwise
 * zero is returned. */
int flist_sel_range(struct view_t *view, int begin, int end, int mark_current);

#endif /* VIFM__FLIST_SELECT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
