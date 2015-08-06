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

#include <time.h> /* time_t */

#include "ui/ui.h"
#include "utils/test_helpers.h"

#define NUM_REGULAR_BOOKMARKS 61
#define NUM_SPECIAL_BOOKMARKS 3
#define NUM_BOOKMARKS (NUM_REGULAR_BOOKMARKS + NUM_SPECIAL_BOOKMARKS)

/* Special file name value for "directory bookmarks". */
#define NO_BOOKMARK_FILE ".."

/* Structure that describes bookmark data. */
typedef struct
{
	char *file;       /* Name of bookmarked file. */
	char *directory;  /* Path to directory at which bookmark was made. */
	time_t timestamp; /* Last bookmark update time (-1 means "never"). */
}
bookmark_t;

extern const char valid_bookmarks[];

/* Gets bookmark by its index.  Returns pointer to a statically allocated
 * bookmark_t structure or NULL for wrong index. */
const bookmark_t * get_bookmark(const int bmark_index);

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

/* Checks whether given bookmark is older than given time.  Returns non-zero if
 * so, otherwise zero is returned. */
int is_bookmark_older(const char mark, const time_t than);

/* Sets user's bookmark interactively.  Returns non-zero if UI message was
 * printed, otherwise zero is returned. */
int set_user_bookmark(const char mark, const char directory[],
		const char file[]);

/* Sets all properties of user's bookmark (e.g. from saved configuration). */
void setup_user_bookmark(const char mark, const char directory[],
		const char file[], time_t timestamp);

/* Sets special bookmark.  Does nothing for invalid mark value. */
void set_spec_bookmark(const char mark, const char directory[],
		const char file[]);

/* Handles all kinds of bookmarks.  Returns new value for save_msg flag. */
int goto_bookmark(FileView *view, char mark);

/* Clears a bookmark by its name. */
void clear_bookmark(const int mark);

/* Clears all bookmarks. */
void clear_all_bookmarks(void);

int check_mark_directory(FileView *view, char mark);

/* Fills array of booleans (active_bookmarks) each of which shows whether
 * specified bookmark index is active.  active_bookmarks should be an array of
 * at least NUM_BOOKMARKS items.  Returns number of active bookmarks. */
int init_active_bookmarks(const char marks[], int active_bookmarks[]);

TSTATIC_DEFS(
	bookmark_t * get_bmark_by_name(const char mark);
)

#endif /* VIFM__BOOKMARKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
