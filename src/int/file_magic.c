/* vifm
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

#include "file_magic.h"

#ifdef HAVE_LIBGTK
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#endif

#ifdef HAVE_LIBMAGIC
#include <magic.h>
#endif

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() malloc() */
#include <stdio.h> /* popen() */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../utils/filemon.h"
#include "../utils/fsddata.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../filetype.h"
#include "../status.h"
#include "desktop.h"

/* Cache entry. */
typedef struct
{
	char *mime;        /* Mime-type. */
	filemon_t filemon; /* Timestamp. */
}
cache_data_t;

static fsddata_t * get_cache(void);
static int lookup_in_cache(fsddata_t *cache, const char path[],
		filemon_t *filemon, cache_data_t **data);
static void update_cache(fsddata_t *cache, const char path[],
		const char mimetype[], filemon_t *filemon, cache_data_t *data);
static int get_gtk_mimetype(const char filename[], char buf[], size_t buf_sz);
static int get_magic_mimetype(const char filename[], char buf[], size_t buf_sz);
static int get_file_mimetype(const char filename[], char buf[], size_t buf_sz);
static assoc_records_t get_handlers(const char mime_type[]);
#if !defined(_WIN32) && defined(ENABLE_DESKTOP_FILES)
static void parse_app_dir(const char directory[], const char mime_type[],
		assoc_records_t *result);
#endif

assoc_records_t
get_magic_handlers(const char file[])
{
	return get_handlers(get_mimetype(file, 1));
}

const char *
get_mimetype(const char file[], int resolve_symlinks)
{
	static char mimetype[128];

	char target[PATH_MAX + 1];
	if(resolve_symlinks)
	{
		if(os_realpath(file, target) == target)
		{
			file = target;
		}
	}

	fsddata_t *mime_cache = get_cache();

	filemon_t filemon;
	cache_data_t *cache_data = NULL;
	if(lookup_in_cache(mime_cache, file, &filemon, &cache_data))
	{
		copy_str(mimetype, sizeof(mimetype), cache_data->mime);
		return mimetype;
	}

	if(get_gtk_mimetype(file, mimetype, sizeof(mimetype)) == -1)
	{
		if(get_magic_mimetype(file, mimetype, sizeof(mimetype)) == -1)
		{
			if(get_file_mimetype(file, mimetype, sizeof(mimetype)) == -1)
			{
				return NULL;
			}
		}
	}

	update_cache(mime_cache, file, mimetype, &filemon, cache_data);

	return mimetype;
}

/* Retrieves mime-type cache, creating it on first call.  Returns the cache. */
static fsddata_t *
get_cache(void)
{
	static fsddata_t *mime_cache;
	if(mime_cache == NULL)
	{
		mime_cache = fsddata_create(0, 0);
	}
	return mime_cache;
}

/* Looks up cache entry for the specified path.  On success, *filemon is
 * initialized from the file.  Returns non-zero if entry is found, otherwise
 * zero is returned. */
static int
lookup_in_cache(fsddata_t *cache, const char path[], filemon_t *filemon,
		cache_data_t **data)
{
	void *value = NULL;
	if(fsddata_get(cache, path, &value) == 0)
	{
		*data = value;

		(void)filemon_from_file(path, FMT_MODIFIED, filemon);
		return filemon_equal(filemon, &(*data)->filemon);
	}
	return 0;
}

/* Updates cache for the path. */
static void
update_cache(fsddata_t *cache, const char path[], const char mimetype[],
		filemon_t *filemon, cache_data_t *data)
{
	if(data != NULL)
	{
		/* Simply update cache entry in place. */
		replace_string(&data->mime, mimetype);
		data->filemon = *filemon;
		return;
	}

	(void)filemon_from_file(path, FMT_MODIFIED, filemon);

	data = malloc(sizeof(*data));
	if(data == NULL)
	{
		return;
	}

	data->filemon = *filemon;
	data->mime = strdup(mimetype);
	if(data->mime == NULL)
	{
		free(data);
		return;
	}

	fsddata_set(cache, path, data);
}

static int
get_gtk_mimetype(const char filename[], char buf[], size_t buf_sz)
{
#ifdef HAVE_LIBGTK
	GFile *file;
	GFileInfo *info;

	if(!curr_stats.gtk_available)
	{
		return -1;
	}

	file = g_file_new_for_path(filename);
	info = g_file_query_info(file, "standard::", G_FILE_QUERY_INFO_NONE, NULL,
			NULL);
	if(info == NULL)
	{
		g_object_unref(file);
		return -1;
	}

	copy_str(buf, buf_sz, g_file_info_get_content_type(info));
	g_object_unref(info);
	g_object_unref(file);
	return 0;
#else /* #ifdef HAVE_LIBGTK */
	return -1;
#endif /* #ifdef HAVE_LIBGTK */
}

static int
get_magic_mimetype(const char filename[], char buf[], size_t buf_sz)
{
#ifdef HAVE_LIBMAGIC
	static magic_t magic;
	const char *descr;

	if(magic == NULL)
	{
#if HAVE_DECL_MAGIC_MIME_TYPE
		magic = magic_open(MAGIC_MIME_TYPE);
#else
		magic = magic_open(MAGIC_MIME);
#endif

		if(magic == NULL)
		{
			return -1;
		}

		if(magic_load(magic, NULL) != 0)
		{
			magic_close(magic);
			magic = NULL;
			return -1;
		}
	}

	descr = magic_file(magic, filename);
	if(descr == NULL)
	{
		return -1;
	}

	copy_str(buf, buf_sz, descr);
#if !HAVE_DECL_MAGIC_MIME_TYPE
	break_atr(buf, ';');
#endif

	return 0;
#else /* #ifdef HAVE_LIBMAGIC */
	return -1;
#endif /* #ifdef HAVE_LIBMAGIC */
}

static int
get_file_mimetype(const char filename[], char buf[], size_t buf_sz)
{
#ifdef HAVE_FILE_PROG
	FILE *pipe;
	char command[1024];
	char *const escaped_filename = shell_like_escape(filename, 0);

	/* Use the file command to get mimetype */
	snprintf(command, sizeof(command), "file %s -b --mime-type",
			escaped_filename);
	free(escaped_filename);

	if((pipe = popen(command, "r")) == NULL)
	{
		return -1;
	}

	if(fgets(buf, buf_sz, pipe) != buf)
	{
		pclose(pipe);
		return -1;
	}

	pclose(pipe);
	chomp(buf);

	return 0;
#else /* #ifdef HAVE_FILE_PROG */
	return -1;
#endif /* #ifdef HAVE_FILE_PROG */
}

static assoc_records_t
get_handlers(const char mime_type[])
{
	static assoc_records_t handlers;
	ft_assoc_records_free(&handlers);

	if(mime_type == NULL)
	{
		return handlers;
	}

#if !defined(_WIN32) && defined(ENABLE_DESKTOP_FILES)
	parse_app_dir("/usr/share/applications", mime_type, &handlers);
	parse_app_dir("/usr/local/share/applications", mime_type, &handlers);

	char local_dir[PATH_MAX + 1];
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	if(is_null_or_empty(xdg_data_home))
	{
		build_path(local_dir, sizeof(local_dir), cfg.home_dir,
				".local/share/applications");
	}
	else
	{
		build_path(local_dir, sizeof(local_dir), xdg_data_home, "applications");
	}
	parse_app_dir(local_dir, mime_type, &handlers);
#endif

	return handlers;
}

#if !defined(_WIN32) && defined(ENABLE_DESKTOP_FILES)
static void
parse_app_dir(const char directory[], const char mime_type[],
		assoc_records_t *result)
{
	assoc_records_t desktop_assocs = parse_desktop_files(directory, mime_type);
	ft_assoc_record_add_all(result, &desktop_assocs);
	ft_assoc_records_free(&desktop_assocs);
}
#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
