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

#ifndef VIFM__MARKS_H__
#define VIFM__MARKS_H__

#include <stddef.h> /* wchar_t */
#include <time.h> /* time_t */

#include "utils/test_helpers.h"

#define NUM_REGULAR_MARKS 61
#define NUM_SPECIAL_MARKS 3
#define NUM_MARKS (NUM_REGULAR_MARKS + NUM_SPECIAL_MARKS)

/* Special file name value for "directory marks". */
#define NO_MARK_FILE ".."

struct view_t;

/* Callback for suggest_marks() function invoked per active mark. */
typedef void (*mark_suggest_cb)(const wchar_t text[], const wchar_t value[],
		const char descr[]);

/* Structure that describes mark data. */
typedef struct
{
	char *file;       /* Name of marked file. */
	char *directory;  /* Path to directory at which mark was made. */
	time_t timestamp; /* Last mark update time (-1 means "never"). */
}
mark_t;

extern const char valid_marks[];

/* Gets mark by its index.  Returns pointer to a statically allocated
 * mark_t structure or NULL for wrong index. */
const mark_t * get_mark(const int bmark_index);

/* Transform an index to a mark.  Returns name of the mark or '\0' on invalid
 * index. */
char index2mark(const int bmark_index);

/* Checks if a mark specified by its index is valid (exists and points to an
 * existing directory).  Returns non-zero if so, otherwise zero is returned. */
int is_valid_mark(const int bmark_index);

/* Checks whether given mark is empty.  Returns non-zero if so, otherwise zero
 * is returned. */
int is_mark_empty(const char m);

int is_spec_mark(const int x);

/* Checks whether given mark is older than given time.  Returns non-zero if so,
 * otherwise zero is returned. */
int is_mark_older(const char m, const time_t than);

/* Sets user's mark interactively.  Returns non-zero if UI message was printed,
 * otherwise zero is returned. */
int set_user_mark(const char mark, const char directory[], const char file[]);

/* Sets all properties of user's mark (e.g. from saved configuration). */
void setup_user_mark(const char mark, const char directory[], const char file[],
		time_t timestamp);

/* Sets special mark.  Does nothing for invalid mark value. */
void set_spec_mark(const char mark, const char directory[], const char file[]);

/* Handles all kinds of marks.  Returns new value for save_msg flag. */
int goto_mark(struct view_t *view, char mark);

/* Clears a mark by its name. */
void clear_mark(const int m);

/* Clears all marks. */
void clear_all_marks(void);

/* Looks up file specified by the mark m in the view.  Returns the position if
 * found, otherwise -1 is returned. */
int check_mark_directory(struct view_t *view, char m);

/* Fills array of booleans (active_marks) each of which shows whether specified
 * mark index is active.  active_marks should be an array of at least NUM_MARKS
 * items.  Returns number of active marks. */
int init_active_marks(const char marks[], int active_marks[]);

/* Lists active marks.  Local marks are those that point into current view. */
void suggest_marks(mark_suggest_cb cb, int local_only);

TSTATIC_DEFS(
	mark_t * get_mark_by_name(const char mark);
)

#endif /* VIFM__MARKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
