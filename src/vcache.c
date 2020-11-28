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

#include <string.h> /* memmove() memset() strcmp() */

#include "compat/os.h"
#include "ui/cancellation.h"
#include "ui/quickview.h"
#include "utils/darray.h"
#include "utils/file_streams.h"
#include "utils/filemon.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "filetype.h"

/* Cached output of a specific previewer for a specific file. */
typedef struct
{
	char *path;        /* Full path to the file. */
	char *viewer;      /* Viewer of the file. */
	filemon_t filemon; /* Timestamp for the file. */
	strlist_t lines;   /* Top lines of preview contents. */
	int complete;      /* Whether cache contains complete output of the viewer. */
}
vcache_entry_t;

static vcache_entry_t * find_cache_entry(const char full_path[],
		const char viewer[], int max_lines);
static vcache_entry_t * alloc_cache_entry(void);
TSTATIC void vcache_reset(int max_size);
static void free_cache_entry(vcache_entry_t *centry);
static int is_cache_match(const vcache_entry_t *centry, const char path[],
		const char viewer[]);
static int is_cache_valid(const vcache_entry_t *centry, const char path[],
		const char viewer[], int max_lines);
static void update_cache_entry(vcache_entry_t *centry, const char path[],
		const char viewer[], int max_lines, const char **error);
static strlist_t get_data(const char full_path[], const char viewer[],
		int max_lines, int *complete, const char **error);
TSTATIC strlist_t read_lines(FILE *fp, int max_lines, int *complete);

/* Cache of viewers' output.  Most recent entry is the last one. */
static vcache_entry_t *cache;
/* Declarations to enable use of DA_* on cache. */
static DA_INSTANCE(cache);
/* Maximum number of allocated cache entries. */
static size_t max_cache_entries = 100U;

strlist_t
vcache_lookup(const char full_path[], const char viewer[], ViewerKind kind,
		int max_lines, const char **error)
{
	*error = NULL;

	if(kind == VK_GRAPHICAL)
	{
		/* Skip caching of data we can't really cache. */
		static strlist_t non_cache;
		free_string_array(non_cache.items, non_cache.nitems);

		int complete;
		non_cache = get_data(full_path, viewer, max_lines, &complete, error);
		return non_cache;
	}

	vcache_entry_t *centry = find_cache_entry(full_path, viewer, max_lines);
	if(centry != NULL && is_cache_valid(centry, full_path, viewer, max_lines))
	{
		return centry->lines;
	}

	if(centry == NULL)
	{
		centry = alloc_cache_entry();
		if(centry == NULL)
		{
			*error = "Failed to allocate cache entry";
			strlist_t empty_list = {};
			return empty_list;
		}
	}

	update_cache_entry(centry, full_path, viewer, max_lines, error);
	return centry->lines;
}

/* Looks up existing cache entry that matches specified set of parameters.
 * Returns the entry or NULL. */
static vcache_entry_t *
find_cache_entry(const char full_path[], const char viewer[], int max_lines)
{
	size_t i;
	for(i = 0U; i < DA_SIZE(cache); ++i)
	{
		if(is_cache_match(&cache[i], full_path, viewer))
		{
			return &cache[i];
		}
	}
	return NULL;
}

/* Allocates a zero-initialized cache entry.  When cache size limit is reached
 * older cache entries are reused.  Returns the entry or NULL. */
static vcache_entry_t *
alloc_cache_entry(void)
{
	if(DA_SIZE(cache) < max_cache_entries)
	{
		vcache_entry_t *centry = DA_EXTEND(cache);
		if(centry != NULL)
		{
			memset(centry, 0, sizeof(*centry));
			DA_COMMIT(cache);
			return centry;
		}
	}

	if(DA_SIZE(cache) == 0U)
	{
		return NULL;
	}

	free_cache_entry(&cache[0]);
	memmove(cache, cache + 1, sizeof(*cache)*(DA_SIZE(cache) - 1U));

	vcache_entry_t *centry = &cache[DA_SIZE(cache) - 1U];
	memset(centry, 0, sizeof(*centry));
	return centry;
}

/* Invalidates all cache entries and changes size limit. */
TSTATIC void
vcache_reset(int max_size)
{
	size_t i;
	for(i = 0U; i < DA_SIZE(cache); ++i)
	{
		free_cache_entry(&cache[i]);
	}
	DA_REMOVE_ALL(cache);

	max_cache_entries = max_size;
}

/* Frees resources of a cache entry. */
static void
free_cache_entry(vcache_entry_t *centry)
{
	update_string(&centry->path, NULL);
	update_string(&centry->viewer, NULL);

	free_string_array(centry->lines.items, centry->lines.nitems);
	centry->lines.items = NULL;
	centry->lines.nitems = 0;
}

/* Checks whether cache entry matches specified file and viewer.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_cache_match(const vcache_entry_t *centry, const char path[],
		const char viewer[])
{
	int same_viewer = (centry->viewer == NULL && viewer == NULL)
	               || (centry->viewer != NULL && viewer != NULL &&
	                   strcmp(centry->viewer, viewer) == 0);
	return same_viewer
	    && centry->path != NULL
	    && paths_are_equal(centry->path, path);
}

/* Checks whether data in the cache entry is up to date with the file on disk
 * and contains enough lines.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_cache_valid(const vcache_entry_t *centry, const char path[],
		const char viewer[], int max_lines)
{
	filemon_t filemon;
	(void)filemon_from_file(path, FMT_MODIFIED, &filemon);
	if(!filemon_equal(&centry->filemon, &filemon))
	{
		return 0;
	}

	return (centry->complete || centry->lines.nitems >= max_lines);
}

/* Setups cache entry for a specific file and viewer replacing its previous
 * state in the process.  *error is set either to NULL or an error code on
 * failure. */
static void
update_cache_entry(vcache_entry_t *centry, const char path[],
		const char viewer[], int max_lines, const char **error)
{
	(void)filemon_from_file(path, FMT_MODIFIED, &centry->filemon);

	replace_string(&centry->path, path);
	update_string(&centry->viewer, viewer);

	free_string_array(centry->lines.items, centry->lines.nitems);
	centry->lines = get_data(path, viewer, max_lines, &centry->complete, error);
}

/* Invokes viewer of a file to get its output.  *error is set either to NULL or
 * an error code on failure.  Returns output and sets
 * *complete. */
static strlist_t
get_data(const char full_path[], const char viewer[], int max_lines,
		int *complete, const char **error)
{
	ui_cancellation_push_on();

	FILE *fp;
	if(is_null_or_empty(viewer))
	{
		int dir = is_dir(full_path);

		/* Binary mode is important on Windows. */
		fp = (dir ? qv_view_dir(full_path, max_lines) : os_fopen(full_path, "rb"));
		if(fp == NULL)
		{
			*error = dir ? "Failed to list directory's contents"
			             : "Failed to read file's contents";
		}
	}
	else
	{
		fp = read_cmd_output(viewer, 0);
		if(fp == NULL)
		{
			*error = "Failed to start a viewer";
		}
	}

	strlist_t lines = {};

	if(fp != NULL)
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
