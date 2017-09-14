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

#include <dirent.h>

#include <stdio.h> /* snprintf() */
#include <string.h> /* strstr() strchr() strlen() strcpy() */

#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utils.h"
#include "../filetype.h"

static const char EXEC_KEY[] = "Exec=";
static const char MIMETYPE_KEY[] = "MimeType=";
static const char NAME_KEY[] = "Name=";

static const char CAPTION_MACRO = 'c';
static const char FILE_MACROS[] = "Uuf";

static void parse_desktop_files_internal(const char path[],
		const char mime_type[], assoc_records_t *result);
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
parse_desktop_files_internal(const char path[], const char mime_type[],
		assoc_records_t *result)
{
	DIR *dir;
	struct dirent *dentry;
	const char *slash;

	if((dir = os_opendir(path)) == NULL)
	{
		return;
	}

	slash = ends_with_slash(path) ? "" : "/";

	while((dentry = os_readdir(dir)) != NULL)
	{
		char full_path[PATH_MAX + 1];

		if(is_builtin_dir(dentry->d_name))
		{
			continue;
		}

		snprintf(full_path, sizeof(full_path), "%s%s%s", path, slash,
				dentry->d_name);
		if(get_dirent_type(dentry, full_path) == DT_DIR)
		{
			parse_desktop_files(full_path, mime_type);
		}
		else
		{
			process_file(full_path, mime_type, result);
		}
	}

	os_closedir(dir);
}

static void
process_file(const char *path, const char *file_mime_type,
		assoc_records_t *result)
{
	FILE *f;
	char exec[1024] = "", mime_type[2048] = "", name[2048] = "";
	char buf[2048];

	if(!ends_with(path, ".desktop") || (f = os_fopen(path, "r")) == NULL)
	{
		return;
	}

	while(fgets(buf, sizeof(buf), f) != NULL)
	{
		chomp(buf);

		if(starts_with(buf, EXEC_KEY))
		{
			copy_str(exec, sizeof(exec), buf + (ARRAY_LEN(EXEC_KEY) - 1));
		}
		else if(starts_with(buf, MIMETYPE_KEY))
		{
			copy_str(mime_type, sizeof(mime_type),
					buf + (ARRAY_LEN(MIMETYPE_KEY) - 1));
		}
		else if(starts_with(buf, NAME_KEY))
		{
			copy_str(name, sizeof(name), buf + (ARRAY_LEN(NAME_KEY) - 1));
		}
	}

	fclose(f);

	if(strstr(mime_type, file_mime_type) == NULL || exec[0] == '\0')
	{
		return;
	}

	expand_desktop(exec, buf);
	ft_assoc_record_add(result, buf, name);
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
/* vim: set cinoptions+=t0 filetype=c : */
