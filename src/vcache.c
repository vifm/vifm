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

#include <fcntl.h> /* F_GETFL O_NONBLOCK fcntl() */

#include <stdio.h> /* FILE */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() memset() strcmp() */
#include <time.h> /* time_t time() */

#include "compat/os.h"
#include "ui/cancellation.h"
#include "ui/quickview.h"
#include "utils/darray.h"
#include "utils/file_streams.h"
#include "utils/filemon.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/selector.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "background.h"
#include "filetype.h"

/* Maximum number of seconds to wait for process to cancel. */
enum { MAX_KILL_DELAY_S = 2 };

/* Cached output of a specific previewer for a specific file. */
typedef struct
{
	char *path;        /* Full path to the file. */
	char *viewer;      /* Viewer of the file. */
	bg_job_t *job;     /* If not NULL, source of file contents. */
	filemon_t filemon; /* Timestamp for the file. */
	strlist_t lines;   /* Top lines of preview contents. */
	time_t kill_timer; /* Since when we're waiting for the job to die or zero. */
	int max_lines;     /* Number of lines requested. */
	int complete;      /* Whether cache contains complete output of the viewer. */
	int truncated;     /* Whether last line is truncated. */
}
vcache_entry_t;

static void wait_async_finish(vcache_entry_t *centry);
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
static int pull_async(vcache_entry_t *centry);
static int read_async_output(vcache_entry_t *centry);
static int is_ready_for_read(FILE *stream);
static int need_more_async_output(vcache_entry_t *centry);
static strlist_t get_data(vcache_entry_t *centry, const char **error);
TSTATIC strlist_t read_lines(FILE *fp, int max_lines, int *complete);

/* Cache of viewers' output.  Most recent entry is the last one. */
static vcache_entry_t *cache;
/* Declarations to enable use of DA_* on cache. */
static DA_INSTANCE(cache);
/* Maximum number of allocated cache entries. */
static size_t max_cache_entries = 100U;

void
vcache_finish(void)
{
	size_t i;
	for(i = 0U; i < DA_SIZE(cache); ++i)
	{
		if(cache[i].job != NULL)
		{
			bg_job_cancel(cache[i].job);
			bg_job_terminate(cache[i].job);
			bg_job_decref(cache[i].job);
			cache[i].job = NULL;
		}
	}
}

int
vcache_check(vcache_is_previewed_cb is_previewed)
{
	int changed = 0;

	/* TODO: consider doing this in a separate thread. */

	size_t i;
	for(i = 0U; i < DA_SIZE(cache); ++i)
	{
		if(cache[i].job != NULL)
		{
			changed |= (pull_async(&cache[i]) && is_previewed(cache[i].path));
		}
	}

	return changed;
}

strlist_t
vcache_lookup(const char full_path[], const char viewer[], ViewerKind kind,
		int max_lines, int sync, const char **error)
{
	*error = NULL;

	if(kind == VK_GRAPHICAL)
	{
		/* Skip caching of data we can't really cache. */
		static vcache_entry_t non_cache;
		free_cache_entry(&non_cache);

		non_cache.max_lines = max_lines;
		replace_string(&non_cache.path, full_path);
		update_string(&non_cache.viewer, viewer);

		strlist_t lines = get_data(&non_cache, error);
		free_string_array(lines.items, lines.nitems);

		wait_async_finish(&non_cache);
		return non_cache.lines;
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

	if(sync)
	{
		wait_async_finish(centry);
	}

	if(kind != VK_PASS_THROUGH && centry->lines.nitems == 0 &&
			centry->job != NULL)
	{
		/* TODO: consider printing time we're waiting for output. */
		static char *items[] = { "[...]" };
		static strlist_t stub = { .items = items, .nitems = 1 };
		return stub;
	}

	return centry->lines;
}

/* Waits for asynchronous job to be done. */
static void
wait_async_finish(vcache_entry_t *centry)
{
	bg_job_t *job = centry->job;
	if(job == NULL)
	{
		return;
	}

	ui_cancellation_push_on();

	do
	{
		wait_for_data_from(job->pid, job->output, 0, &ui_cancellation_info);

		if(ui_cancellation_requested())
		{
			break;
		}

		int read_result;
		do
		{
			read_result = read_async_output(centry);
		}
		while(read_result > 0);

		if(read_result < 0)
		{
			break;
		}
	}
	while(bg_job_is_running(job));

	while(read_async_output(centry) > 0)
	{
		/* Reading is performed in conditional expression. */
	}

	if(ui_cancellation_requested())
	{
		centry->lines.nitems = add_to_string_array(&centry->lines.items,
				centry->lines.nitems, "[cancelled]");
	}
	ui_cancellation_pop();

	bg_job_decref(centry->job);
	centry->job = NULL;
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

	if(centry->job != NULL)
	{
		bg_job_cancel(centry->job);
		bg_job_terminate(centry->job);
		bg_job_decref(centry->job);
		centry->job = NULL;
	}
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
	centry->max_lines = max_lines;

	replace_string(&centry->path, path);
	update_string(&centry->viewer, viewer);

	if(centry->job == NULL)
	{
		free_string_array(centry->lines.items, centry->lines.nitems);
		centry->lines = get_data(centry, error);
	}
	else
	{
		(void)pull_async(centry);
	}
}

/* Updates single entry backed by an asynchronous job.  Returns non-zero if
 * entry was updated, otherwise zero is returned. */
static int
pull_async(vcache_entry_t *centry)
{
	int changed = 0;

	if(centry->kill_timer != 0)
	{
		if(time(NULL) - centry->kill_timer > MAX_KILL_DELAY_S)
		{
			bg_job_terminate(centry->job);
		}
	}
	else
	{
		if(!need_more_async_output(centry))
		{
			centry->kill_timer = time(NULL);
			bg_job_cancel(centry->job);
			return 0;
		}

		while(need_more_async_output(centry) && read_async_output(centry) > 0)
		{
			changed = 1;
		}
	}

	if(!bg_job_is_running(centry->job))
	{
		centry->complete = (read_async_output(centry) <= 0);
		bg_job_decref(centry->job);
		centry->job = NULL;
		changed = 1;
	}

	return changed;
}

/* Populates entry with more data from an asynchronous job if it's available.
 * Returns zero if nothing was read, positive integer if something was read and
 * negative integer on reaching EOF. */
static int
read_async_output(vcache_entry_t *centry)
{
	if(!is_ready_for_read(centry->job->output))
	{
		return 0;
	}

	char piece[4096];
	size_t to_read = sizeof(piece) - 1;

#ifdef _WIN32
	/* Simulate asynchronous reading by not reading more than stream has. */
	HANDLE hpipe = (HANDLE)_get_osfhandle(fileno(centry->job->output));
	DWORD bytes_available = 0;
	if(!PeekNamedPipe(hpipe, NULL, 0, NULL, &bytes_available, NULL))
	{
		return -1;
	}
	if(bytes_available == 0)
	{
		return 0;
	}
	if(bytes_available < to_read)
	{
		to_read = bytes_available;
	}
#endif

	size_t len = fread(piece, 1, to_read, centry->job->output);
	piece[len] = '\0';

	if(len == 0)
	{
		return -1;
	}

	clearerr(centry->job->output);

	int new_truncated = (len > 0)
	                 && (piece[len - 1] != '\r' && piece[len - 1] != '\n');

	int nlines;
	char **lines = break_into_lines(piece, len, &nlines, 0);

	if(centry->truncated)
	{
		char **last = &centry->lines.items[centry->lines.nitems - 1];
		size_t last_len = strlen(*last);
		strappend(last, &last_len, lines[0]);
		free(lines[0]);
	}

	int i = (centry->truncated ? 1 : 0);
	while(i < nlines)
	{
		centry->lines.nitems = put_into_string_array(&centry->lines.items,
				centry->lines.nitems, lines[i]);
		++i;
	}
	free(lines);

	centry->truncated = new_truncated;
	return 1;
}

/* Checks whether stream contains data to be read.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_ready_for_read(FILE *stream)
{
	selector_t *selector = selector_alloc();
	if(selector == NULL)
	{
		return 0;
	}

	int fd = fileno(stream);
#ifndef _WIN32
	selector_add(selector, fd);
#else
	HANDLE handle = (HANDLE)_get_osfhandle(fd);
	selector_add(selector, handle);
#endif

	int has_data = selector_wait(selector, 0);
	selector_free(selector);
	return has_data;
}

/* Checks whether entry is full with data already.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
need_more_async_output(vcache_entry_t *centry)
{
	int effective_lines = centry->lines.nitems;
	if(centry->truncated)
	{
		--effective_lines;
	}

	return (effective_lines < centry->max_lines);
}

/* Invokes viewer of a file to get its output.  *error is set either to NULL or
 * an error code on failure.  Returns output and sets *complete. */
static strlist_t
get_data(vcache_entry_t *centry, const char **error)
{
	ui_cancellation_push_on();

	FILE *fp = NULL;
	if(is_null_or_empty(centry->viewer))
	{
		int dir = is_dir(centry->path);

		/* Binary mode is important on Windows. */
		fp = dir ? qv_view_dir(centry->path, centry->max_lines)
		         : os_fopen(centry->path, "rb");
		if(fp == NULL)
		{
			*error = dir ? "Failed to list directory's contents"
			             : "Failed to read file's contents";
		}
	}
	else
	{
		centry->job = bg_run_external_job(centry->viewer, BJF_MERGE_STREAMS);
		if(centry->job != NULL)
		{
			ui_cancellation_pop();
			centry->complete = 0;
			centry->truncated = 0;

#ifndef _WIN32
			/* Enable non-blocking read from output pipe.  On Windows we read the
			 * exact amount of data present in the stream. */
			int fd = fileno(centry->job->output);
			int flags = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

			strlist_t lines = {};
			return lines;
		}

		*error = "Failed to start a viewer";
	}

	strlist_t lines = {};

	if(fp != NULL)
	{
		lines = read_lines(fp, centry->max_lines, &centry->complete);
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
