/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "vcache.h"

#include <string.h> /* strcmp() */

#include "compat/os.h"
#include "ui/cancellation.h"
#include "ui/quickview.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"

static strlist_t get_data(const char full_path[], const char viewer[],
		int max_lines, int *complete);
TSTATIC strlist_t read_lines(FILE *fp, int max_lines, int *complete);

strlist_t
vcache_lookup(const char full_path[], const char viewer[], int max_lines)
{
	static strlist_t cache;
	free_string_array(cache.items, cache.nitems);

	int complete;
	cache = get_data(full_path, viewer, max_lines, &complete);
	return cache;
}

/* Invokes viewer of a file to get its output.  Returns output and sets
 * *complete. */
static strlist_t
get_data(const char full_path[], const char viewer[], int max_lines,
		int *complete)
{
	ui_cancellation_push_on();

	FILE *fp;
	if(is_null_or_empty(viewer))
	{
		/* Binary mode is important on Windows. */
		fp = is_dir(full_path) ? qv_view_dir(full_path, max_lines)
		                       : os_fopen(full_path, "rb");
	}
	else
	{
		fp = qv_execute_viewer(viewer);
	}

	strlist_t lines = {};

	if(fp == NULL)
	{
		lines.nitems = add_to_string_array(&lines.items, lines.nitems,
				"Vifm: previewing has failed");
	}
	else
	{
		lines = read_lines(fp, max_lines, complete);
		fclose(fp);
	}

	ui_cancellation_pop();
	return lines;
}

/* Reads at most max_lines from the stream ignoring BOM.  Returns the lines
 * read. */
TSTATIC strlist_t
read_lines(FILE *fp, int max_lines, int *complete)
{
	strlist_t lines = {};
	skip_bom(fp);

	char *next_line;
	while(lines.nitems < max_lines && (next_line = read_line(fp, NULL)) != NULL)
	{
		const int old_len = lines.nitems;
		lines.nitems = put_into_string_array(&lines.items, lines.nitems, next_line);
		if(lines.nitems == old_len)
		{
			free(next_line);
			break;
		}
	}

	*complete = (next_line == NULL);
	return lines;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
