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

#ifndef VIFM__FOPS_RENAME_H__
#define VIFM__FOPS_RENAME_H__

#include "utils/test_helpers.h"

struct view_t;

/* Renames single file under the cursor. */
void fops_rename_current(struct view_t *view, int name_only);

/* Renames marked files using names given in the list of length nlines (or
 * filled in by the user, when the list is empty).  Recursively traverses
 * directories in selection when recursive flag is not zero.  Recursive
 * traversal is incompatible with list of names.  Returns new value for
 * save_msg flag. */
int fops_rename(struct view_t *view, char *list[], int nlines, int recursive);

/* Increments/decrements first number in names of marked files of the view k
 * times.  Returns new value for save_msg flag. */
int fops_incdec(struct view_t *view, int k);

/* Changes case of all letters in names of marked files of the view.  Returns
 * new value for save_msg flag. */
int fops_case(struct view_t *view, int to_upper);

/* Replaces matches of regular expression in names of files of the view.
 * Returns new value for save_msg flag. */
int fops_subst(struct view_t *view, const char pattern[], const char sub[],
		int ic, int glob);

/* Replaces letters in names of marked files of the view according to the
 * mapping: from[i] -> to[i] (must have the same length).  Returns new value for
 * save_msg flag. */
int fops_tr(struct view_t *view, const char from[], const char to[]);

TSTATIC_DEFS(
	const char * incdec_name(const char fname[], int k);
)

#endif /* VIFM__FOPS_RENAME_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
