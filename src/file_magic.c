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

#include "../config.h"

#ifdef HAVE_LIBGTK
#include <glib.h>
#include <gtk/gtk.h>
#endif

#ifdef HAVE_LIBMAGIC
#include <magic.h>
#endif

#ifndef _WIN32
#include <sys/dir.h>
#endif
#include<dirent.h> /* DIR */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "status.h"
#include "undo.h"
#include "utils.h"

#include "file_magic.h"

static char* handlers;
static size_t handlers_len;

static int get_gtk_mimetype(const char* filename, char* buf);
static int get_magic_mimetype(const char* filename, char* buf);
static int get_file_mimetype(const char* filename, char* buf, size_t buf_sz);
static char * get_handlers(const char* mime_type);
#ifndef _WIN32
static void enum_files(const char* path, const char* mime_type);
static void process_file(const char* path, const char* mime_type);
static void expand_desktop(const char* str, char* buf);
#endif

char*
get_magic_handlers(const char* file)
{
	return get_handlers(get_mimetype(file));
}

/* Returns pointer to a static buffer */
const char *
get_mimetype(const char *file)
{
	static char mimetype[128];

	if(get_gtk_mimetype(file, mimetype) == -1)
	{
		if(get_magic_mimetype(file, mimetype) == -1)
		{
			if(get_file_mimetype(file, mimetype, sizeof(mimetype)) == -1)
				return NULL;
		}
	}

	return mimetype;
}

static int
get_gtk_mimetype(const char* filename, char* buf)
{
#ifdef HAVE_LIBGTK
	GFile* file;
	GFileInfo* info;

	if(!curr_stats.gtk_available)
		return -1;

	file = g_file_new_for_path(filename);
	info = g_file_query_info(file, "standard::", G_FILE_QUERY_INFO_NONE, NULL,
			NULL);
	if(info == NULL)
	{
		g_object_unref(file);
		return -1;
	}

	strcpy(buf, g_file_info_get_content_type(info));
	g_object_unref(info);
	g_object_unref(file);
	return 0;
#else /* #ifdef HAVE_LIBGTK */
	return -1;
#endif /* #ifdef HAVE_LIBGTK */
}

static int
get_magic_mimetype(const char* filename, char* buf)
{
#ifdef HAVE_LIBMAGIC
	magic_t magic;

	magic = magic_open(MAGIC_MIME_TYPE);
	if(magic == NULL)
		return -1;

	magic_load(magic, NULL);

	strcpy(buf, magic_file(magic, filename));

	magic_close(magic);
	return 0;
#else /* #ifdef HAVE_LIBMAGIC */
	return -1;
#endif /* #ifdef HAVE_LIBMAGIC */
}

static int
get_file_mimetype(const char* filename, char* buf, size_t buf_sz)
{
#ifdef HAVE_FILE_PROG
	FILE *pipe;
	char command[1024];

	/* Use the file command to get mimetype */
	snprintf(command, sizeof(command), "file \"%s\" -b --mime-type", filename);

	if((pipe = popen(command, "r")) == NULL)
		return -1;

	if(fgets(buf, buf_sz, pipe) != buf)
	{
		pclose(pipe);
		return -1;
	}

	pclose(pipe);

	return 0;
#else /* #ifdef HAVE_FILE_PROG */
	return -1;
#endif /* #ifdef HAVE_FILE_PROG */
}

char *
get_handlers(const char* mime_type)
{
	free(handlers);
	handlers = NULL;
	handlers_len = 0;

#ifndef _WIN32
	enum_files("/usr/share/applications", mime_type);
	enum_files("/usr/local/share/applications", mime_type);
#endif

	return handlers;
}

#ifndef _WIN32
static void
enum_files(const char *path, const char *mime_type)
{
	DIR* dir;
	struct dirent* dentry;
	const char *slash = "";

	dir = opendir(path);
	if(dir == NULL)
		return;

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	while((dentry = readdir(dir)) != NULL) {
		char buf[PATH_MAX];

		if(pathcmp(dentry->d_name, ".") == 0)
			continue;
		else if(pathcmp(dentry->d_name, "..") == 0)
			continue;

		snprintf(buf, sizeof (buf), "%s%s%s", path, slash, dentry->d_name);
		if(dentry->d_type == DT_DIR)
			enum_files(buf, mime_type);
		else
			process_file(buf, mime_type);
	}

	closedir(dir);
}

static void
process_file(const char* path, const char *mime_type)
{
	FILE* f;
	char *p;
	char exec_buf[1024] = "";
	char mime_type_buf[2048] = "";
	char buf[2048];

	if(!ends_with(path, ".desktop"))
		return;

	f = fopen(path, "r");
	if(f == NULL)
		return;

	while(fgets(buf, sizeof (buf), f) != NULL)
	{
		size_t len = strlen(buf);

		if(buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		if(strncmp(buf, "Exec=", 5) == 0)
			strcpy(exec_buf, buf);
		else if (strncmp(buf, "MimeType=", 9) == 0)
			strcpy(mime_type_buf, buf);
	}

	fclose(f);

	if(strstr(mime_type_buf, mime_type) == NULL)
		return;
	if(exec_buf[0] == '\0')
		return;

	expand_desktop(exec_buf + 5, buf);
	p = realloc(handlers, handlers_len + 1 + strlen(buf) + 1);
	if(p == NULL)
		return;

	handlers = p;
	if(handlers_len == 0)
		*handlers = '\0';
	else
		strcat(handlers, ",");
	handlers_len += 1 + strlen(buf);
	strcat(handlers, buf);
}

static void
expand_desktop(const char* str, char* buf)
{
	int substituted = 0;
	while(*str != '\0')
	{
		if(*str != '%')
		{
			*buf++ = *str++;
			continue;
		}
		str++;
		if(*str == 'c')
		{
			substituted = 1;
			strcpy(buf, "caption");
			buf += strlen(buf);
		}
		else if(strchr("Uuf", *str) != NULL)
		{
			substituted = 1;
			strcpy(buf, "%f");
			buf += strlen(buf);
		}
		str++;
	}
	if(substituted)
		*buf = '\0';
	else
	{
		*buf = ' ';
		strcpy(buf + 1, "%f");
	}
}
#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
