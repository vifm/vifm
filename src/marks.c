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

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "modes/wk.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "filelist.h"
#include "flist_pos.h"

static int is_valid_index(const int index);
static void clear_marks(mark_t marks[], int count);
static void reset_mark(mark_t *mark);
static int is_user_mark(char name);
static void set_mark(view_t *view, char name, const char directory[],
		const char file[], time_t timestamp, int force);
static int is_mark_points_to(const mark_t *mark, const char directory[],
		const char file[]);
static int navigate_to_mark(view_t *view, char name);
TSTATIC mark_t * get_mark_by_name(view_t *view, char name);
static mark_t * find_mark(view_t *view, const int index);
static int is_mark_valid(const mark_t *mark);
static int is_empty(const mark_t *mark);

/* User-writable marks. */
#define USER_MARKS \
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
/* Marks that can't be explicitly set by the user. */
#define SPECIAL_MARKS "<>'"

/* Data of regular marks. */
static mark_t regular_marks[NUM_REGULAR_MARKS];

const char marks_all[] = USER_MARKS SPECIAL_MARKS;
ARRAY_GUARD(marks_all, NUM_MARKS + 1);

/* List of special marks that can't be set manually, hence require special
 * treatment in some cases. */
static const char spec_marks[] = SPECIAL_MARKS;
ARRAY_GUARD(spec_marks, NUM_SPECIAL_MARKS + 1);

const mark_t *
marks_by_index(view_t *view, const int index)
{
	return find_mark(view, index);
}

char
marks_resolve_index(const int index)
{
	if(is_valid_index(index))
	{
		return marks_all[index];
	}
	return '\0';
}

/* Checks whether mark index is valid.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
is_valid_index(const int index)
{
	return index >= 0 && index < (int)ARRAY_LEN(marks_all) - 1;
}

int
marks_is_valid(view_t *view, const int index)
{
	const char m = marks_resolve_index(index);
	const mark_t *const mark = get_mark_by_name(view, m);
	return is_mark_valid(mark);
}

int
marks_is_empty(view_t *view, const char name)
{
	return is_empty(get_mark_by_name(view, name));
}

int
marks_is_special(const int index)
{
	return char_is_one_of(spec_marks, marks_resolve_index(index));
}

void
marks_clear_one(view_t *view, char name)
{
	reset_mark(get_mark_by_name(view, name));
}

void
marks_clear_all(void)
{
	clear_marks(regular_marks, ARRAY_LEN(regular_marks));

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		marks_clear_view(tab_info.view);
	}
}

void
marks_clear_view(struct view_t *view)
{
	clear_marks(view->special_marks, ARRAY_LEN(view->special_marks));
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
marks_is_older(view_t *view, char name, const time_t than)
{
	const mark_t *const mark = get_mark_by_name(view, name);
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
marks_set_user(view_t *view, char name, const char directory[],
		const char file[])
{
	if(!is_user_mark(name))
	{
		ui_sb_msg("Invalid mark name");
		return 1;
	}

	set_mark(view, name, directory, file, time(NULL), 0);
	return 0;
}

void
marks_setup_user(view_t *view, char name, const char directory[],
		const char file[], time_t timestamp)
{
	if(is_user_mark(name))
	{
		set_mark(view, name, directory, file, timestamp, 1);
	}
	else
	{
		ui_sb_errf("Only user's marks can be loaded, but got: %c", name);
	}
}

/* Checks whether given mark corresponds to mark that can be set by a user.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_user_mark(char name)
{
	return char_is_one_of(marks_all, name)
	    && !char_is_one_of(spec_marks, name);
}

void
marks_set_special(view_t *view, char name, const char directory[],
		const char file[])
{
	if(char_is_one_of(spec_marks, name))
	{
		set_mark(view, name, directory, file, time(NULL), 1);
	}
}

/* Sets values of the mark.  The force parameter controls whether mark is
 * updated even when it already points to the specified directory-file pair. */
static void
set_mark(view_t *view, char name, const char directory[], const char file[],
		time_t timestamp, int force)
{
	mark_t *const mark = get_mark_by_name(view, name);
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
marks_find_in_view(view_t *view, char name)
{
	int custom;
	const mark_t *const mark = get_mark_by_name(view, name);

	if(is_empty(mark))
	{
		return -1;
	}

	custom = flist_custom_active(view);

	if(custom && cv_tree(view->custom.type) && strcmp(mark->file, "..") == 0)
	{
		return fpos_find_entry(view, mark->file, mark->directory);
	}

	if(custom)
	{
		dir_entry_t *entry;
		char path[PATH_MAX + 1];
		snprintf(path, sizeof(path), "%s/%s", mark->directory, mark->file);
		entry = entry_from_path(view, view->dir_entry, view->list_rows, path);
		if(entry != NULL)
		{
			return entry_to_pos(view, entry);
		}
	}
	else if(paths_are_equal(view->curr_dir, mark->directory))
	{
		return fpos_find_by_name(view, mark->file);
	}

	return -1;
}

int
marks_goto(view_t *view, char name)
{
	switch(name)
	{
		case '\'':
			navigate_back(view);
			return 0;
		case NC_C_c:
		case NC_ESC:
			fview_cursor_redraw(view);
			return 0;

		default:
			return navigate_to_mark(view, name);
	}
}

/* Navigates the view to given mark if it's valid.  Returns new value for
 * save_msg flag. */
static int
navigate_to_mark(view_t *view, char name)
{
	const mark_t *const mark = get_mark_by_name(view, name);

	if(is_mark_valid(mark))
	{
		if(!cfg_ch_pos_on(CHPOS_DIRMARK) || flist_custom_active(view) ||
				!is_parent_dir(mark->file))
		{
			navigate_to_file(view, mark->directory, mark->file, 1);
		}
		else
		{
			navigate_to(view, mark->directory);
		}
		return 0;
	}

	if(!char_is_one_of(marks_all, name))
	{
		ui_sb_msg("Invalid mark name");
	}
	else if(is_empty(mark))
	{
		ui_sb_msg("Mark is not set");
	}
	else
	{
		ui_sb_msg("Mark is invalid");
	}

	fview_cursor_redraw(view);
	return 1;
}

/* Gets mark data structure by name of a mark.  Returns pointer to mark's data
 * structure or NULL. */
TSTATIC mark_t *
get_mark_by_name(view_t *view, char name)
{
	const char *const pos = strchr(marks_all, name);
	return (pos == NULL) ? NULL : find_mark(view, pos - marks_all);
}

/* Gets mark by its index.  Returns pointer to a statically allocated mark_t
 * structure or NULL for wrong index. */
static mark_t *
find_mark(view_t *view, const int index)
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
	return &view->special_marks[spec_mark_index];
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
marks_list_active(view_t *view, const char marks[], int active_marks[])
{
	int i, x;

	i = 0;
	for(x = 0; x < NUM_MARKS; ++x)
	{
		if(!char_is_one_of(marks, marks_resolve_index(x)))
			continue;
		if(is_empty(marks_by_index(view, x)))
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
marks_suggest(view_t *view, mark_suggest_cb cb, int local_only)
{
	int active_marks[NUM_MARKS];
	const int count = marks_list_active(view, marks_all, active_marks);
	int i;

	for(i = 0; i < count; ++i)
	{
		char *descr;
		const char *file = "";
		const char *suffix = "";
		const int m = active_marks[i];
		const wchar_t mark_name[] = {
			L'm', L'a', L'r', L'k', L':', L' ', marks_resolve_index(m), L'\0'
		};
		const mark_t *const mark = marks_by_index(view, m);

		if(local_only && marks_find_in_view(view, marks_resolve_index(m)) == -1)
		{
			continue;
		}

		if(marks_is_valid(view, m) && !is_parent_dir(mark->file))
		{
			char path[PATH_MAX + 1];
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
