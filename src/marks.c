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

#include "marks.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <time.h> /* time_t time() */

#include "compat/fs_limits.h"
#include "modes/wk.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "filelist.h"

static int is_valid_index(const int index);
static void clear_marks(mark_t marks[], int count);
static void reset_mark(mark_t *mark);
static int is_user_mark(const char mark);
static void set_mark(const char m, const char directory[], const char file[],
		time_t timestamp, int force);
static int is_mark_points_to(const mark_t *mark, const char directory[],
		const char file[]);
static int navigate_to_mark(FileView *view, const char m);
TSTATIC mark_t * get_mark_by_name(const char mark);
static mark_t * find_mark(const int index);
static int is_mark_valid(const mark_t *mark);
static int is_empty(const mark_t *mark);

/* Data of regular marks. */
static mark_t regular_marks[NUM_REGULAR_MARKS];

/* Data of special marks. */
static mark_t lspecial_marks[NUM_SPECIAL_MARKS];
static mark_t rspecial_marks[NUM_SPECIAL_MARKS];

const char valid_marks[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'<', '>',
	'\0'
};
ARRAY_GUARD(valid_marks, NUM_MARKS + 1);

/* List of special marks that can't be set manually, hence require special
 * treating in some cases. */
static const char spec_marks[] = {
	'<', '>', '\'',
	'\0'
};
ARRAY_GUARD(spec_marks, NUM_SPECIAL_MARKS + 1);

const mark_t *
get_mark(const int index)
{
	return find_mark(index);
}

char
index2mark(const int index)
{
	if(is_valid_index(index))
	{
		return valid_marks[index];
	}
	return '\0';
}

/* Checks whether mark index is valid.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
is_valid_index(const int index)
{
	return index >= 0 && index < (int)ARRAY_LEN(valid_marks) - 1;
}

int
is_valid_mark(const int index)
{
	const char m = index2mark(index);
	const mark_t *const mark = get_mark_by_name(m);
	return is_mark_valid(mark);
}

int
is_mark_empty(const char m)
{
	return is_empty(get_mark_by_name(m));
}

int
is_spec_mark(const int x)
{
	return char_is_one_of(spec_marks, index2mark(x));
}

void
clear_mark(const int m)
{
	reset_mark(get_mark_by_name(m));
}

void
clear_all_marks(void)
{
	clear_marks(regular_marks, ARRAY_LEN(regular_marks));
	clear_marks(lspecial_marks, ARRAY_LEN(lspecial_marks));
	clear_marks(rspecial_marks, ARRAY_LEN(rspecial_marks));
}

static void
clear_marks(mark_t marks[], int count)
{
	int i;
	for(i = 0; i < count; ++i)
	{
		reset_mark(&marks[i]);
	}
}

/* Frees memory allocated for mark with given index.  For convenience mark can
 * be NULL. */
static void
reset_mark(mark_t *mark)
{
	if(mark != NULL && !is_empty(mark))
	{
		free(mark->directory);
		mark->directory = NULL;

		free(mark->file);
		mark->file = NULL;

		mark->timestamp = time(NULL);
	}
}

int
is_mark_older(const char m, const time_t than)
{
	const mark_t *const mark = get_mark_by_name(m);
	if(mark != NULL)
	{
		static const time_t undef_time = (time_t)-1;
		if(mark->timestamp == undef_time || than == undef_time)
		{
			return mark->timestamp == undef_time;
		}
		return mark->timestamp < than;
	}
	return 1;
}

int
set_user_mark(const char mark, const char directory[], const char file[])
{
	if(!is_user_mark(mark))
	{
		status_bar_message("Invalid mark name");
		return 1;
	}

	set_mark(mark, directory, file, time(NULL), 0);
	return 0;
}

void
setup_user_mark(const char mark, const char directory[], const char file[],
		time_t timestamp)
{
	if(is_user_mark(mark))
	{
		set_mark(mark, directory, file, timestamp, 1);
	}
	else
	{
		status_bar_errorf("Only user's marks can be loaded, but got: %c", mark);
	}
}

/* Checks whether given mark corresponds to mark that can be set by a user.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_user_mark(const char mark)
{
	return char_is_one_of(valid_marks, mark)
	    && !char_is_one_of(spec_marks, mark);
}

void
set_spec_mark(const char mark, const char directory[], const char file[])
{
	if(char_is_one_of(spec_marks, mark))
	{
		set_mark(mark, directory, file, time(NULL), 1);
	}
}

/* Sets values of the mark.  The force parameter controls whether mark is
 * updated even when it already points to the specified directory-file pair. */
static void
set_mark(const char m, const char directory[], const char file[],
		time_t timestamp, int force)
{
	mark_t *const mark = get_mark_by_name(m);
	if(mark != NULL && (force || !is_mark_points_to(mark, directory, file)))
	{
		reset_mark(mark);

		mark->directory = strdup(directory);
		mark->file = strdup(file);
		mark->timestamp = timestamp;

		/* Remove any trailing slashes, they might be convenient in configuration
		 * file (hence they are permitted), but shouldn't be stored internally. */
		chosp(mark->file);
	}
}

/* Checks whether given mark points to specified directory-file pair.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_mark_points_to(const mark_t *mark, const char directory[], const char file[])
{
	return !is_empty(mark)
	    && mark->timestamp != (time_t)-1
	    && strcmp(mark->directory, directory) == 0
	    && strcmp(mark->file, file) == 0;
}

int
check_mark_directory(FileView *view, char m)
{
	int custom;
	const mark_t *const mark = get_mark_by_name(m);

	if(is_empty(mark))
	{
		return -1;
	}

	custom = flist_custom_active(view);

	if(custom && view->custom.tree_view && strcmp(mark->file, "..") == 0)
	{
		return flist_find_entry(view, mark->file, mark->directory);
	}

	if(custom)
	{
		dir_entry_t *entry;
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", mark->directory, mark->file);
		entry = entry_from_path(view->dir_entry, view->list_rows, path);
		if(entry != NULL)
		{
			return entry_to_pos(view, entry);
		}
	}
	else if(paths_are_equal(view->curr_dir, mark->directory))
	{
		return find_file_pos_in_list(view, mark->file);
	}

	return -1;
}

int
goto_mark(FileView *view, char mark)
{
	switch(mark)
	{
		case '\'':
			navigate_back(view);
			return 0;
		case NC_C_c:
		case NC_ESC:
			fview_cursor_redraw(view);
			return 0;

		default:
			return navigate_to_mark(view, mark);
	}
}

/* Navigates the view to given mark if it's valid.  Returns new value for
 * save_msg flag. */
static int
navigate_to_mark(FileView *view, char m)
{
	const mark_t *const mark = get_mark_by_name(m);

	if(is_mark_valid(mark))
	{
		navigate_to_file(view, mark->directory, mark->file, 1);
		return 0;
	}

	if(!char_is_one_of(valid_marks, m))
	{
		status_bar_message("Invalid mark name");
	}
	else if(is_empty(mark))
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

/* Gets mark data structure by name of a mark.  Returns pointer to mark's data
 * structure or NULL. */
TSTATIC mark_t *
get_mark_by_name(const char mark)
{
	const char *const pos = strchr(valid_marks, mark);
	return (pos == NULL) ? NULL : find_mark(pos - valid_marks);
}

/* Gets mark by its index.  Returns pointer to a statically allocated mark_t
 * structure or NULL for wrong index. */
static mark_t *
find_mark(const int index)
{
	int spec_mark_index;

	if(!is_valid_index(index))
	{
		return NULL;
	}

	if(index < NUM_REGULAR_MARKS)
	{
		return &regular_marks[index];
	}

	spec_mark_index = index - NUM_REGULAR_MARKS;
	return (curr_view == &lwin)
	     ? &lspecial_marks[spec_mark_index]
	     : &rspecial_marks[spec_mark_index];
}

/* Checks if a mark is valid (exists and points to an existing directory).  For
 * convenience mark can be NULL.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_mark_valid(const mark_t *mark)
{
	return !is_empty(mark) && is_valid_dir(mark->directory);
}

int
init_active_marks(const char marks[], int active_marks[])
{
	int i, x;

	i = 0;
	for(x = 0; x < NUM_MARKS; ++x)
	{
		if(!char_is_one_of(marks, index2mark(x)))
			continue;
		if(is_empty(get_mark(x)))
			continue;
		active_marks[i++] = x;
	}
	return i;
}

/* Checks whether mark specified is empty.  For convenience mark can be NULL.
 * Returns non-zero for non-empty or NULL mark, otherwise zero is returned. */
static int
is_empty(const mark_t *mark)
{
	return mark == NULL
	    || mark->directory == NULL
	    || mark->file == NULL;
}

void
suggest_marks(mark_suggest_cb cb, int local_only)
{
	int active_marks[NUM_MARKS];
	const int count = init_active_marks(valid_marks, active_marks);
	int i;

	for(i = 0; i < count; ++i)
	{
		char *descr;
		const char *file = "";
		const char *suffix = "";
		const int m = active_marks[i];
		const wchar_t mark_name[] = {
			L'm', L'a', L'r', L'k', L':', L' ', index2mark(m), L'\0'
		};
		const mark_t *const mark = get_mark(m);

		if(local_only && check_mark_directory(curr_view, index2mark(m)) == -1)
		{
			continue;
		}

		if(is_valid_mark(m) && !is_parent_dir(mark->file))
		{
			char path[PATH_MAX];
			file = mark->file;
			snprintf(path, sizeof(path), "%s/%s", mark->directory, file);
			if(is_dir(path))
			{
				suffix = "/";
			}
		}

		descr = format_str("%s%s%s%s", replace_home_part(mark->directory),
				(file[0] != '\0') ? "/" : "", file, suffix);

		cb(mark_name, L"", descr);

		free(descr);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
