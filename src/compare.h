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

#ifndef VIFM__DIFF_H__
#define VIFM__DIFF_H__

#include "ui/ui.h"

/* Type of files to list after a comparison. */
typedef enum
{
	LT_ALL,    /* All files. */
	LT_DUPS,   /* Files that have at least 1 dup on other side or in this view. */
	LT_UNIQUE, /* Files unique to this view or within this view. */
}
ListType;

/* Composes two panes containing information about derived from two file system
 * trees.  If group_paths is zero, views are sorted by ids.  Returns non-zero if
 * status bar message should be preserved. */
int compare_two_panes(CompareType ct, ListType lt, int group_paths,
		int skip_empty);

/* Replaces single pane with information derived from its files.  Returns
 * non-zero if status bar message should be preserved. */
int compare_one_pane(view_t *view, CompareType ct, ListType lt, int skip_empty);

/* Moves current file from one view to the other.  Returns non-zero if status
 * bar message should be preserved. */
int compare_move(view_t *from, view_t *to);

#endif /* VIFM__DIFF_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
