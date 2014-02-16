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

#ifndef VIFM__BOOKMARKS_H__
#define VIFM__BOOKMARKS_H__

#include "ui.h"

#define NUM_BOOKMARKS 64

/* Structure that describes bookmark data. */
typedef struct
{
	char *file;      /* Name of bookmarked file. */
	char *directory; /* Path to directory at which bookmark was made. */
}
bookmark_t;

/* Data of all bookmarks.  Contains at least NUM_BOOKMARKS items. */
extern bookmark_t bookmarks[];

extern const char valid_bookmarks[];

/* Transform an index to a mark.  Returns name of the mark or '\0' on invalid
 * index. */
char index2mark(const int bmark_index);

/* Checks if a bookmark specified by its index is valid (exists and points to an
 * existing directory).  Returns non-zero if so, otherwise zero is returned. */
int is_valid_bookmark(const int bmark_index);

/* Checks whether given bookmark is empty.  Returns non-zero if so, otherwise
 * zero is returned. */
int is_bookmark_empty(const char mark);

int is_spec_bookmark(const int x);

int add_user_bookmark(const char mark, const char directory[],
		const char file[]);

void set_specmark(const char mark, const char directory[], const char file[]);

/* Handles all kinds of bookmarks.  Returns new value for save_msg flag. */
int goto_bookmark(FileView *view, char mark);

/* Removes bookmarks by its name. */
void remove_bookmark(const int mark);

/* Removes all bookmarks. */
void remove_all_bookmarks(void);

int check_mark_directory(FileView *view, char mark);

/* Fills array of booleans (active_bookmarks) each of which shows whether
 * specified bookmark index is active.  active_bookmarks should be an array of
 * at least NUM_BOOKMARKS items.  Returns number of active bookmarks. */
int init_active_bookmarks(const char marks[], int active_bookmarks[]);

#endif /* VIFM__BOOKMARKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
