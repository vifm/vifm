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

#include "desktop.h"

#if defined(ENABLE_DESKTOP_FILES)

#ifndef _WIN32
#include <sys/dir.h>
#endif
#include <dirent.h> /* DIR */

#include <stdio.h> /* snprintf() */
#include <string.h> /* strstr() strchr() strlen() strcpy() */

#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "filetype.h"

static const char EXEC_KEY[] = "Exec=";
static const char MIMETYPE_KEY[] = "MimeType=";
static const char NAME_KEY[] = "Name=";

static const char CAPTION_MACRO = 'c';
static const char FILE_MACROS[] = "Uuf";

static void parse_desktop_files_internal(const char *path,
		const char *mime_type, assoc_records_t *result);
static void process_file(const char *path, const char *file_mime_type,
		assoc_records_t *result);
static void expand_desktop(const char *str, char *buf);

assoc_records_t
parse_desktop_files(const char *path, const char *mime_type)
{
	assoc_records_t result = {};
	parse_desktop_files_internal(path, mime_type, &result);
	return result;
}

static void
parse_desktop_files_internal(const char *path, const char *mime_type,
		assoc_records_t *result)
{
	DIR *dir;
	struct dirent *dentry;
	const char *slash;

	if((dir = opendir(path)) == NULL)
	{
		return;
	}

	slash = ends_with_slash(path) ? "" : "/";

	while((dentry = readdir(dir)) != NULL)
	{
		char buf[PATH_MAX];

		if(stroscmp(dentry->d_name, ".") == 0 ||
				stroscmp(dentry->d_name, "..") == 0)
		{
			continue;
		}

		snprintf(buf, sizeof (buf), "%s%s%s", path, slash, dentry->d_name);
		if(dentry->d_type == DT_DIR)
		{
			parse_desktop_files(buf, mime_type);
		}
		else
		{
			process_file(buf, mime_type, result);
		}
	}

	closedir(dir);
}

static void
process_file(const char *path, const char *file_mime_type,
		assoc_records_t *result)
{
	FILE *f;
	char exec[1024] = "", mime_type[2048] = "", name[2048] = "";
	char buf[2048];

	if(!ends_with(path, ".desktop") || (f = fopen(path, "r")) == NULL)
	{
		return;
	}

	while(fgets(buf, sizeof(buf), f) != NULL)
	{
		chomp(buf);

		if(starts_with(buf, EXEC_KEY))
		{
			snprintf(exec, sizeof(exec), "%s", buf + (ARRAY_LEN(EXEC_KEY) - 1));
		}
		else if(starts_with(buf, MIMETYPE_KEY))
		{
			snprintf(mime_type, sizeof(mime_type), "%s",
					buf + (ARRAY_LEN(MIMETYPE_KEY) - 1));
		}
		else if(starts_with(buf, NAME_KEY))
		{
			snprintf(name, sizeof(name), "%s", buf + (ARRAY_LEN(NAME_KEY) - 1));
		}
	}

	fclose(f);

	if(strstr(mime_type, file_mime_type) == NULL || exec[0] == '\0')
	{
		return;
	}

	expand_desktop(exec, buf);
	add_assoc_record(result, buf, name);
}

static void
expand_desktop(const char *str, char *buf)
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
		if(*str == CAPTION_MACRO)
		{
			strcpy(buf, "caption");
			buf += strlen(buf);
		}
		else if(char_is_one_of(FILE_MACROS, *str))
		{
			substituted = 1;
			strcpy(buf, "%f");
			buf += strlen(buf);
		}
		str++;
	}

	*buf = substituted ? '\0' : ' ';
	if(!substituted)
	{
		strcpy(buf + 1, "%f");
	}
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
