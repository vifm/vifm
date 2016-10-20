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
#include <stdlib.h> /* free() */
#include <stdio.h> /* popen() */
#include <unistd.h> /* read() */

#include "../utils/path.h"
#include "../utils/str.h"
#include "../filetype.h"
#include "../status.h"
#include "desktop.h"

static assoc_records_t handlers;

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
	return get_handlers(get_mimetype(file));
}

const char *
get_mimetype(const char file[])
{
	static char mimetype[128];

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

	return mimetype;
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
	magic_t magic;
	const char *descr;
	pid_t pid;
	int pipes[2];
	ssize_t r;

	if (pipe(pipes) == -1)
		return -1;

	switch (pid = fork()) {
		case -1:
			return -1;
		case 0:
			close(pipes[0]);

#if HAVE_DECL_MAGIC_MIME_TYPE
			magic = magic_open(MAGIC_MIME_TYPE);
#else
			magic = magic_open(MAGIC_MIME);
#endif
			if (magic == NULL)
				_exit(-1);

			if (magic_load(magic, NULL) == -1) {
				magic_close(magic);
				_exit(-1);
			}

			if((descr = magic_file(magic, filename)) == NULL) {
				magic_close(magic);
				_exit(-1);
			}

#if !HAVE_DECL_MAGIC_MIME_TYPE
			break_atr(descr, ';');
#endif
			if (write(pipes[1], descr, strlen(descr)) == -1)
				_exit(-1);
			close(pipes[1]);
			magic_close(magic);
			_exit(0);
		default:
			close(pipes[1]);
			r = read(pipes[0], buf, buf_sz - 1);
			buf[r] = '\0';
			close(pipes[0]);
			waitpid(pid, NULL, 0);
	}
	return (r == -1 || r == 0) ? -1 : 0;
#else /* #ifdef HAVE_LIBMAGIC */
	return -1;
#endif /* #ifdef HAVE_LIBMAGIC */
}

static int
get_file_mimetype(const char filename[], char buf[], size_t buf_sz)
{
#ifdef HAVE_FILE_PROG
	char *const escaped_filename = shell_like_escape(filename, 0);
	int pipes[2];
	pid_t pid;
	ssize_t r;

	if (pipe(pipes) == -1)
		return -1;

	switch (pid = fork())
	{
		case -1:
			printf("ERROR: forking child process failed\n");
			return -1;
		case 0:
			dup2 (pipes[1], STDOUT_FILENO);
			close(pipes[0]);
			close(pipes[1]);
			/* Use the file command to get mimetype */
			execl("/usr/bin/file", "file", "-b", "--mime-type", escaped_filename, NULL);
			_exit(-1);
		default:
			close(pipes[1]);
			if ((r = read(pipes[0], buf, buf_sz - 1)) == -1)
				r = 0; /* if read(2) failed, we still want buf to be a C string */
			buf[r] = '\0';
			waitpid(pid, NULL, 0);
			close(pipes[0]);
	}

	return (r == 0) ? -1 : 0;
#else /* #ifdef HAVE_FILE_PROG */
	return -1;
#endif /* #ifdef HAVE_FILE_PROG */
}

static assoc_records_t
get_handlers(const char mime_type[])
{
	ft_assoc_records_free(&handlers);

#if !defined(_WIN32) && defined(ENABLE_DESKTOP_FILES)
	parse_app_dir("/usr/share/applications", mime_type, &handlers);
	parse_app_dir("/usr/local/share/applications", mime_type, &handlers);
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
