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

#include "ext_edit.h"

#include <stdlib.h> /* free() */
#include <string.h> /* memmove() strlen() */

#include "../compat/fs_limits.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"

static int ext_edit_is_reedit(const ext_edit_t *ext_edit, char *files[],
		int files_len);
static strlist_t ext_edit_reedit(ext_edit_t *ext_edit);
static strlist_t prepare_edit_list(char *list[], int count);
static void cleanup_edit_list(char *list[], int *count);

strlist_t
ext_edit_prepare(ext_edit_t *ext_edit, char *files[], int files_len)
{
	if(ext_edit_is_reedit(ext_edit, files, files_len))
	{
		return ext_edit_reedit(ext_edit);
	}

	return prepare_edit_list(files, files_len);
}

/* Checks whether this is an instance of re-editing.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
ext_edit_is_reedit(const ext_edit_t *ext_edit, char *files[], int files_len)
{
	char cwd[PATH_MAX + 1];
	if(get_cwd(cwd, sizeof(cwd)) != cwd)
	{
		cwd[0] = '\0';
	}

	return ext_edit->location != NULL
	    && stroscmp(ext_edit->location, cwd) == 0
	    && string_array_equal(files, files_len, ext_edit->files.items,
	                          ext_edit->files.nitems);
}

/* Produces buffer contents for re-editing.  Returns the contents. */
static strlist_t
ext_edit_reedit(ext_edit_t *ext_edit)
{
	const strlist_t lines = ext_edit->lines;
	const strlist_t files = ext_edit->files;

	strlist_t result = {};

	if(ext_edit->last_error != NULL)
	{
		result.nitems = add_to_string_array(&result.items, result.nitems,
				"# Last error:");
		result.nitems = put_into_string_array(&result.items, result.nitems,
				format_str("#  %s", ext_edit->last_error));
		update_string(&ext_edit->last_error, NULL);
	}

	result.nitems = add_to_string_array(&result.items, result.nitems,
			"# Original names:");
	int i;
	for(i = 0; i < files.nitems; ++i)
	{
		result.nitems = put_into_string_array(&result.items, result.nitems,
				format_str("#  %s", files.items[i]));
	}

	result.nitems = add_to_string_array(&result.items, result.nitems,
			"# Edited names:");
	for(i = 0; i < lines.nitems; ++i)
	{
		if(lines.items[i][0] != '#')
		{
			result.nitems = add_to_string_array(&result.items, result.nitems,
					lines.items[i]);
		}
	}

	return result;
}

/* Prepares file list for editing by the user.  Returns a modified list to be
 * processed. */
static strlist_t
prepare_edit_list(char *list[], int count)
{
	strlist_t prepared = {};

	int i;
	for(i = 0; i < count; ++i)
	{
		if(list[i][0] == '#' || list[i][0] == '\\')
		{
			prepared.nitems = put_into_string_array(&prepared.items, prepared.nitems,
					format_str("\\%s", list[i]));
		}
		else
		{
			prepared.nitems = add_to_string_array(&prepared.items, prepared.nitems,
					list[i]);
		}
	}

	return prepared;
}

void
ext_edit_done(ext_edit_t *ext_edit, char *files[], int files_len,
		char *edited[], int *edited_len)
{
	update_string(&ext_edit->location, NULL);

	char cwd[PATH_MAX + 1];
	if(get_cwd(cwd, sizeof(cwd)) == cwd)
	{
		replace_string(&ext_edit->location, cwd);
	}

	free_string_array(ext_edit->files.items, ext_edit->files.nitems);
	free_string_array(ext_edit->lines.items, ext_edit->lines.nitems);

	ext_edit->files.items = copy_string_array(files, files_len);
	ext_edit->files.nitems = files_len;
	ext_edit->lines.items = copy_string_array(edited, *edited_len);
	ext_edit->lines.nitems = *edited_len;

	cleanup_edit_list(edited, edited_len);

	/* Discard the cache on editing cancellation. */
	if(*edited_len == 0)
	{
		ext_edit_discard(ext_edit);
	}
}

void
ext_edit_discard(ext_edit_t *ext_edit)
{
	strlist_t empty = {};

	update_string(&ext_edit->location, NULL);
	update_string(&ext_edit->last_error, NULL);
	free_string_array(ext_edit->files.items, ext_edit->files.nitems);
	free_string_array(ext_edit->lines.items, ext_edit->lines.nitems);
	ext_edit->files = empty;
	ext_edit->lines = empty;
}

/* Cleans up edited list preparing it for processing. */
static void
cleanup_edit_list(char *list[], int *count)
{
	int i, j = 0;
	for(i = 0; i < *count; ++i)
	{
		if(list[i][0] == '#')
		{
			free(list[i]);
			continue;
		}

		if(list[i][0] == '\\')
		{
			memmove(list[i], list[i] + 1, strlen(list[i] + 1) + 1);
		}
		list[j++] = list[i];
	}

	*count = j;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
