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

#include <sys/types.h>

#include <ctype.h> /* isalnum() */
#include <stddef.h> /* NULL */
#include <string.h> /* strdup() */
#include <time.h> /* time_t time() */

#include "cfg/config.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "filelist.h"
#include "status.h"
#include "ui.h"

static void free_bookmark(bookmark_t *bookmark);
static int is_user_bookmark(const char mark);
static void add_mark(const char mark, const char directory[], const char file[],
		time_t timestamp);
static int navigate_to_bookmark(FileView *view, const char mark);
static bookmark_t * get_bookmark(const char mark);
static int is_bmark_valid(const bookmark_t *bookmark);
static int is_bmark_empty(const bookmark_t *bookmark);

bookmark_t bookmarks[NUM_BOOKMARKS];

const char valid_bookmarks[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '<', '>',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
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

char
index2mark(const int bmark_index)
{
	if(bmark_index >= 0 && bmark_index < ARRAY_LEN(valid_bookmarks) - 1)
	{
		return valid_bookmarks[bmark_index];
	}
	return '\0';
}

int
is_valid_bookmark(const int bmark_index)
{
	const char mark = index2mark(bmark_index);
	const bookmark_t *const bookmark = get_bookmark(mark);
	return is_bmark_valid(bookmark);
}

int
is_bookmark_empty(const char mark)
{
	const bookmark_t *const bookmark = get_bookmark(mark);
	return is_bmark_empty(bookmark);
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
	bookmark_t *const bookmark = get_bookmark(mark);
	free_bookmark(bookmark);
}

void
remove_all_bookmarks(void)
{
	bookmark_t *bookmark = &bookmarks[0];
	const bookmark_t *const end = &bookmarks[ARRAY_LEN(bookmarks)];
	while(bookmark != end)
	{
		free_bookmark(bookmark);
		bookmark++;
	}
}

/* Frees memory allocated for bookmark with given index.  For convenience
 * bookmark can be NULL. */
static void
free_bookmark(bookmark_t *bookmark)
{
	if(bookmark != NULL && !is_bmark_empty(bookmark))
	{
		free(bookmark->directory);
		bookmark->directory = NULL;

		free(bookmark->file);
		bookmark->file = NULL;

		bookmark->timestamp = time(NULL);
	}
}

int
is_bookmark_older(const char mark, const time_t than)
{
	const bookmark_t *const bookmark = get_bookmark(mark);
	if(bookmark != NULL)
	{
		static const time_t undef_time = (time_t)-1;
		if(bookmark->timestamp == undef_time || than == undef_time)
		{
			return bookmark->timestamp == undef_time;
		}
		return bookmark->timestamp < than;
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

	add_mark(mark, directory, file, time(NULL));
	return 0;
}

void
load_user_bookmark(const char mark, const char directory[], const char file[],
		time_t timestamp)
{
	if(is_user_bookmark(mark))
	{
		add_mark(mark, directory, file, timestamp);
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
set_specmark(const char mark, const char *directory, const char *file)
{
	if(char_is_one_of(spec_bookmarks, mark))
	{
		add_mark(mark, directory, file, time(NULL));
	}
}

static void
add_mark(const char mark, const char directory[], const char file[],
		time_t timestamp)
{
	bookmark_t *const bookmark = get_bookmark(mark);
	if(bookmark != NULL)
	{
		free_bookmark(bookmark);

		bookmark->directory = strdup(directory);
		bookmark->file = strdup(file);
		bookmark->timestamp = timestamp;
	}
}

int
check_mark_directory(FileView *view, char mark)
{
	const bookmark_t *const bookmark = get_bookmark(mark);

	if(!is_bmark_empty(bookmark))
	{
		if(stroscmp(view->curr_dir, bookmark->directory) == 0)
		{
			return find_file_pos_in_list(view, bookmark->file);
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
			navigate_to(view, view->last_dir);
			return 0;
		case '\x03': /* Ctrl-C. */
		case '\x1b': /* Escape. */
			move_to_list_pos(view, view->list_pos);
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
	const bookmark_t *const bookmark = get_bookmark(mark);

	if(is_bmark_valid(bookmark))
	{
		if(change_directory(view, bookmark->directory) >= 0)
		{
			load_dir_list(view, 1);
			(void)ensure_file_is_selected(view, bookmark->file);
		}
	}
	else
	{
		if(!char_is_one_of(valid_bookmarks, mark))
			status_bar_message("Invalid mark name");
		else if(is_bmark_empty(bookmark))
			status_bar_message("Mark is not set");
		else
			status_bar_message("Mark is invalid");

		move_to_list_pos(view, view->list_pos);
		return 1;
	}
	return 0;
}

/* Gets bookmark data structure by name of a bookmark.  Returns pointer to
 * bookmark's data structure or NULL. */
static bookmark_t *
get_bookmark(const char mark)
{
	const char *pos = strchr(valid_bookmarks, mark);
	return (pos == NULL) ? NULL : &bookmarks[pos - valid_bookmarks];
}

/* Checks if a bookmark is valid (exists and points to an existing directory).
 * For convenience bookmark can be NULL.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
is_bmark_valid(const bookmark_t *bookmark)
{
	return !is_bmark_empty(bookmark) && is_valid_dir(bookmark->directory);
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
		if(is_bmark_empty(&bookmarks[x]))
			continue;
		active_bookmarks[i++] = x;
	}
	return i;
}

/* Checks whether bookmark specified is empty.  For convenience bookmark can be
 * NULL.  Returns non-zero for non-empty or NULL bookmark, otherwise zero is
 * returned. */
static int
is_bmark_empty(const bookmark_t *bookmark)
{
	return bookmark == NULL
	    || bookmark->directory == NULL
	    || bookmark->file == NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
