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

#define NUM_REGULAR_MARKS 62
#define NUM_SPECIAL_MARKS 3
#define NUM_MARKS (NUM_REGULAR_MARKS + NUM_SPECIAL_MARKS)

/* Special file name value for "directory marks". */
#define NO_MARK_FILE ".."

struct view_t;

/* Callback for marks_suggest() function invoked per active mark. */
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

/* List of recognized marks. */
extern const char marks_all[];

/* Gets mark by its index.  Returns pointer to a statically allocated
 * mark_t structure or NULL for wrong index. */
const mark_t * marks_by_index(struct view_t *view, int index);

/* Transform an index to a mark.  Returns name of the mark or '\0' on invalid
 * index. */
char marks_resolve_index(int index);

/* Checks if a mark specified by its index is valid (exists and points to an
 * existing directory).  Returns non-zero if so, otherwise zero is returned. */
int marks_is_valid(struct view_t *view, int index);

/* Checks whether given mark is empty.  Returns non-zero if so, otherwise zero
 * is returned. */
int marks_is_empty(struct view_t *view, char name);

/* Checks whether given mark is a special mark.  Returns non-zero if so,
 * otherwise zero is returned. */
int marks_is_special(int index);

/* Checks whether given mark is older than given time.  Returns non-zero if so,
 * otherwise zero is returned. */
int marks_is_older(struct view_t *view, char name, const time_t than);

/* Sets user's mark interactively.  Returns non-zero if UI message was printed,
 * otherwise zero is returned. */
int marks_set_user(struct view_t *view, char name, const char directory[],
		const char file[]);

/* Sets all properties of user's mark (e.g. from saved configuration). */
void marks_setup_user(struct view_t *view, char name, const char directory[],
		const char file[], time_t timestamp);

/* Sets special mark.  Does nothing for invalid mark value. */
void marks_set_special(struct view_t *view, char name, const char directory[],
		const char file[]);

/* Handles all kinds of marks.  Returns new value for save_msg flag. */
int marks_goto(struct view_t *view, char name);

/* Clears a mark by its name. */
void marks_clear_one(struct view_t *view, char name);

/* Clears all marks. */
void marks_clear_all(void);

/* Clears marks stored inside the view. */
void marks_clear_view(struct view_t *view);

/* Looks up file specified by the name parameter in the view.  Returns the
 * position if found, otherwise -1 is returned. */
int marks_find_in_view(struct view_t *view, char name);

/* Fills array of booleans (active_marks) each of which shows whether specified
 * mark index is active.  active_marks should be an array of at least NUM_MARKS
 * items.  Returns number of active marks. */
int marks_list_active(struct view_t *view, const char name[],
		int active_marks[]);

/* Lists active marks.  Local marks are those that point into current view. */
void marks_suggest(struct view_t *view, mark_suggest_cb cb, int local_only);

TSTATIC_DEFS(
	mark_t * get_mark_by_name(struct view_t *view, char name);
)

#endif /* VIFM__MARKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
