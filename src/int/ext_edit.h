/* vifm
 * Copyright (C) 2021 xaizek.
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

#ifndef VIFM__INT__EXT_EDIT_H__
#define VIFM__INT__EXT_EDIT_H__

/* Management of state of external editing on interactive renaming. */

#include "../utils/string_array.h"

/* Information about external editing. */
typedef struct ext_edit_t ext_edit_t;
struct ext_edit_t
{
	char *location;   /* Path of the operation. */
	char *last_error; /* Last error message about related operation or NULL. */
	strlist_t files;  /* List of files. */
	strlist_t lines;  /* Editing result. */
};

/* Produces list of files to present the user with, which might be result of
 * previous editing.  Returns the list. */
strlist_t ext_edit_prepare(ext_edit_t *ext_edit, char *files[],
		int files_len);

/* Performs caching and post-editing processing of user edited file. */
void ext_edit_done(ext_edit_t *ext_edit, char *files[], int files_len,
		char *edited[], int *edited_len);

/* Clears the cache of external editing. */
void ext_edit_discard(ext_edit_t *ext_edit);

#endif /* VIFM__INT__EXT_EDIT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
