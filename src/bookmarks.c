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

#include "bookmarks.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <time.h> /* time_t time() */

#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "filelist.h"
#include "fileview.h"

static int is_valid_index(const int bmark_index);
static void clear_bmarks(bookmark_t bmarks[], int count);
static void clear_mark(bookmark_t *bmark);
static int is_user_bookmark(const char mark);
static void set_mark(const char mark, const char directory[], const char file[],
		time_t timestamp, int force);
static int is_bmark_points_to(const bookmark_t *bmark, const char directory[],
		const char file[]);
static int navigate_to_bookmark(FileView *view, const char mark);
TSTATIC bookmark_t * get_bmark_by_name(const char mark);
static bookmark_t * get_bmark(const int bmark_index);
static int is_bmark_valid(const bookmark_t *bmark);
static int is_bmark_empty(const bookmark_t *bmark);

/* Data of regular bookmarks. */
static bookmark_t regular_bookmarks[NUM_REGULAR_BOOKMARKS];

/* Data of special bookmarks. */
static bookmark_t lspecial_bookmarks[NUM_SPECIAL_BOOKMARKS];
static bookmark_t rspecial_bookmarks[NUM_SPECIAL_BOOKMARKS];

const char valid_bookmarks[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'<', '>',
	'\0'
};
ARRAY_GUARD(valid_bookmarks, NUM_BOOKMARKS + 1);

/* List of special bookmarks that can't be set manually, hence require special
 * treating in some cases. */
static const char spec_bookmarks[] =
{
	'<', '>', '\'',
	'\0'
};
ARRAY_GUARD(spec_bookmarks, NUM_SPECIAL_BOOKMARKS + 1);

const bookmark_t *
get_bookmark(const int bmark_index)
{
	return get_bmark(bmark_index);
}

char
index2mark(const int bmark_index)
{
	if(is_valid_index(bmark_index))
	{
		return valid_bookmarks[bmark_index];
	}
	return '\0';
}

/* Checks whether bookmark index is valid.  Returns non-zero if so, otherwise
 * zero is returned. */
static int
is_valid_index(const int bmark_index)
{
	return bmark_index >= 0 && bmark_index < ARRAY_LEN(valid_bookmarks) - 1;
}

int
is_valid_bookmark(const int bmark_index)
{
	const char mark = index2mark(bmark_index);
	const bookmark_t *const bmark = get_bmark_by_name(mark);
	return is_bmark_valid(bmark);
}

int
is_bookmark_empty(const char mark)
{
	const bookmark_t *const bmark = get_bmark_by_name(mark);
	return is_bmark_empty(bmark);
}

int
is_spec_bookmark(const int x)
{
	const char mark = index2mark(x);
	return char_is_one_of(spec_bookmarks, mark);
}

void
clear_bookmark(const int mark)
{
	bookmark_t *const bmark = get_bmark_by_name(mark);
	clear_mark(bmark);
}

void
clear_all_bookmarks(void)
{
	clear_bmarks(regular_bookmarks, ARRAY_LEN(regular_bookmarks));
	clear_bmarks(lspecial_bookmarks, ARRAY_LEN(lspecial_bookmarks));
	clear_bmarks(rspecial_bookmarks, ARRAY_LEN(rspecial_bookmarks));
}

static void
clear_bmarks(bookmark_t bmarks[], int count)
{
	int i;
	for(i = 0; i < count; ++i)
	{
		clear_mark(&bmarks[i]);
	}
}

/* Frees memory allocated for bookmark with given index.  For convenience
 * bmark can be NULL. */
static void
clear_mark(bookmark_t *bmark)
{
	if(bmark != NULL && !is_bmark_empty(bmark))
	{
		free(bmark->directory);
		bmark->directory = NULL;

		free(bmark->file);
		bmark->file = NULL;

		bmark->timestamp = time(NULL);
	}
}

int
is_bookmark_older(const char mark, const time_t than)
{
	const bookmark_t *const bmark = get_bmark_by_name(mark);
	if(bmark != NULL)
	{
		static const time_t undef_time = (time_t)-1;
		if(bmark->timestamp == undef_time || than == undef_time)
		{
			return bmark->timestamp == undef_time;
		}
		return bmark->timestamp < than;
	}
	return 1;
}

int
set_user_bookmark(const char mark, const char directory[], const char file[])
{
	if(!is_user_bookmark(mark))
	{
		status_bar_message("Invalid mark name");
		return 1;
	}

	set_mark(mark, directory, file, time(NULL), 0);
	return 0;
}

void
setup_user_bookmark(const char mark, const char directory[], const char file[],
		time_t timestamp)
{
	if(is_user_bookmark(mark))
	{
		set_mark(mark, directory, file, timestamp, 1);
	}
	else
	{
		status_bar_errorf("Only user's bookmarks can be loaded, but got: %c", mark);
	}
}

/* Checks whether given mark corresponds to bookmark that can be set by a user.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_user_bookmark(const char mark)
{
	return char_is_one_of(valid_bookmarks, mark)
	    && !char_is_one_of(spec_bookmarks, mark);
}

void
set_spec_bookmark(const char mark, const char directory[], const char file[])
{
	if(char_is_one_of(spec_bookmarks, mark))
	{
		set_mark(mark, directory, file, time(NULL), 1);
	}
}

/* Sets values of the mark.  The force parameter controls whether bookmark is
 * updated even when it already points to the specified directory-file pair. */
static void
set_mark(const char mark, const char directory[], const char file[],
		time_t timestamp, int force)
{
	bookmark_t *const bmark = get_bmark_by_name(mark);
	if(bmark != NULL && (force || !is_bmark_points_to(bmark, directory, file)))
	{
		clear_mark(bmark);

		bmark->directory = strdup(directory);
		bmark->file = strdup(file);
		bmark->timestamp = timestamp;

		/* Remove any trailing slashes, they might be convenient in configuration
		 * file (hence they are permitted), but shouldn't be stored internally. */
		chosp(bmark->file);
	}
}

/* Checks whether given bookmark points to specified directory-file pair.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_bmark_points_to(const bookmark_t *bmark, const char directory[],
		const char file[])
{
	return !is_bmark_empty(bmark)
	    && bmark->timestamp != (time_t)-1
	    && strcmp(bmark->directory, directory) == 0
	    && strcmp(bmark->file, file) == 0;
}

int
check_mark_directory(FileView *view, char mark)
{
	const bookmark_t *const bmark = get_bmark_by_name(mark);

	if(!is_bmark_empty(bmark))
	{
		if(paths_are_equal(view->curr_dir, bmark->directory))
		{
			return find_file_pos_in_list(view, bmark->file);
		}
	}

	return -1;
}

int
goto_bookmark(FileView *view, char mark)
{
	switch(mark)
	{
		case '\'':
			navigate_back(view);
			return 0;
		case '\x03': /* Ctrl-C. */
		case '\x1b': /* Escape. */
			fview_cursor_redraw(view);
			return 0;

		default:
			return navigate_to_bookmark(view, mark);
	}
}

/* Navigates the view to given mark if it's valid.  Returns new value for
 * save_msg flag. */
static int
navigate_to_bookmark(FileView *view, char mark)
{
	const bookmark_t *const bmark = get_bmark_by_name(mark);

	if(is_bmark_valid(bmark))
	{
		navigate_to_file(view, bmark->directory, bmark->file);
		return 0;
	}

	if(!char_is_one_of(valid_bookmarks, mark))
	{
		status_bar_message("Invalid mark name");
	}
	else if(is_bmark_empty(bmark))
	{
		status_bar_message("Mark is not set");
	}
	else
	{
		status_bar_message("Mark is invalid");
	}

	fview_cursor_redraw(view);
	return 1;
}

/* Gets bookmark data structure by name of a bookmark.  Returns pointer to
 * bookmark's data structure or NULL. */
TSTATIC bookmark_t *
get_bmark_by_name(const char mark)
{
	const char *const pos = strchr(valid_bookmarks, mark);
	return (pos == NULL) ? NULL : get_bmark(pos - valid_bookmarks);
}

/* Gets bookmark by its index.  Returns pointer to a statically allocated
 * bookmark_t structure or NULL for wrong index. */
static bookmark_t *
get_bmark(const int bmark_index)
{
	int spec_bmark_index;

	if(!is_valid_index(bmark_index))
	{
		return NULL;
	}

	if(bmark_index < NUM_REGULAR_BOOKMARKS)
	{
		return &regular_bookmarks[bmark_index];
	}

	spec_bmark_index = bmark_index - NUM_REGULAR_BOOKMARKS;
	return (curr_view == &lwin)
	     ? &lspecial_bookmarks[spec_bmark_index]
	     : &rspecial_bookmarks[spec_bmark_index];
}

/* Checks if a bookmark is valid (exists and points to an existing directory).
 * For convenience bmark can be NULL.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_bmark_valid(const bookmark_t *bmark)
{
	return !is_bmark_empty(bmark) && is_valid_dir(bmark->directory);
}

int
init_active_bookmarks(const char marks[], int active_bookmarks[])
{
	int i, x;

	i = 0;
	for(x = 0; x < NUM_BOOKMARKS; ++x)
	{
		if(!char_is_one_of(marks, index2mark(x)))
			continue;
		if(is_bmark_empty(get_bookmark(x)))
			continue;
		active_bookmarks[i++] = x;
	}
	return i;
}

/* Checks whether bookmark specified is empty.  For convenience bmark can be
 * NULL.  Returns non-zero for non-empty or NULL bookmark, otherwise zero is
 * returned. */
static int
is_bmark_empty(const bookmark_t *bmark)
{
	return bmark == NULL
	    || bmark->directory == NULL
	    || bmark->file == NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
