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

#include "path.h"

#ifndef _WIN32
#include <pwd.h> /* getpwnam() */
#endif
#include <unistd.h>

#include <assert.h> /* assert() */
#ifdef _WIN32
#include <ctype.h>
#endif
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h>  /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcat() strcmp() strdup() strncmp() strncat()
                       strchr() strcpy() strlen() strpbrk() strrchr() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../int/path_env.h"
#include "env.h"
#include "fs.h"
#include "str.h"
#include "utils.h"

static int skip_dotdir_if_any(const char *path[], int has_parent);
static char * try_replace_tilde(const char path[]);
static char * find_ext_dot(const char path[]);

void
chosp(char path[])
{
	size_t len;

	if(path[0] == '\0')
	{
		return;
	}

	len = strlen(path);
#ifndef _WIN32
	if(path[len - 1] == '/')
#else
	if(path[len - 1] == '/' || path[len - 1] == '\\')
#endif
	{
		path[len - 1] = '\0';
	}
}

int
ends_with_slash(const char *str)
{
	return str[0] != '\0' && str[strlen(str) - 1] == '/';
}

int
path_starts_with(const char path[], const char prefix[])
{
	size_t len = strlen(prefix);

	/* Special case for "/", because it doesn't fit general pattern (slash
	 * shouldn't be stripped or we get empty prefix). */
	if(prefix[0] == '/' && prefix[1] == '\0')
	{
		return (path[0] == '/');
	}

	if(len > 0U && prefix[len - 1] == '/')
	{
		--len;
	}

	return strnoscmp(path, prefix, len) == 0
	    && (path[len] == '\0' || path[len] == '/');
}

int
paths_are_equal(const char s[], const char t[])
{
	/* Some additional space is allocated for adding slashes. */
	char s_can[strlen(s) + 8];
	char t_can[strlen(t) + 8];

	canonicalize_path(s, s_can, sizeof(s_can));
	canonicalize_path(t, t_can, sizeof(t_can));

	return stroscmp(s_can, t_can) == 0;
}

void
canonicalize_path(const char directory[], char buf[], size_t buf_size)
{
	/* Source string pointer. */
	const char *p = directory;
	/* Destination string pointer. */
	char *q = buf - 1;

	memset(buf, '\0', buf_size);

#ifdef _WIN32
	/* Handle first component of a UNC path. */
	if(p[0] == '/' && p[1] == '/' && p[2] != '/')
	{
		strcpy(buf, "//");
		q = buf + 1;
		p += 2;
		while(*p != '\0' && *p != '/')
		{
			*++q = *p++;
		}
		buf = q + 1;
	}
#endif

	while(*p != '\0' && (size_t)((q + 1) - buf) < buf_size - 1U)
	{
		const int prev_dir_present = (q != buf - 1 && *q == '/');
		if(skip_dotdir_if_any(&p, prev_dir_present))
		{
			/* skip_dotdir_if_any() function did all the job for us. */
		}
		else if(prev_dir_present &&
				(strncmp(p, "../", 3) == 0 || strcmp(p, "..") == 0) &&
				!(ends_with(buf, "/../") || strcmp(buf, "../") == 0))
		{
			/* Remove the last path component added. */
#ifdef _WIN32
			/* Special handling of Windows disk name. */
			if(*(q - 1) != ':')
#endif
			{
				++p;
				--q;
				while(q >= buf && *q != '/')
				{
					--q;
				}
			}
		}
		else if(*p == '/')
		{
			/* Don't add more than one slash between path components.  And don't add
			 * leading slash if initial path is relative. */
			if(!prev_dir_present && !(q == buf - 1 && *directory != '/'))
			{
				*++q = '/';
			}
		}
		else
		{
			/* Copy current path component till the end (slash or end of
			 * line/buffer). */
			*++q = *p;
			while(p[1] != '\0' && p[1] != '/' &&
					(size_t)((q + 1) - buf) < buf_size - 1U)
			{
				*++q = *++p;
			}
		}

		++p;
	}

	/* If initial path is relative and we ended up with an empty path, produce
	 * "./". */
	if(*directory != '/' && q < buf && (size_t)((q + 1) - buf) < buf_size - 2U)
	{
		*++q = '.';
	}

	if((*directory != '\0' && q < buf) || (q >= buf && *q != '/'))
	{
		*++q = '/';
	}

	*++q = '\0';
}

/* Checks whether *path begins with current directory component ('./') and moves
 * *path to the last character of such component (to slash if present) or to the
 * previous of the last character depending on has_parent and following
 * contents of the path.  When has_parent is zero the function normalizes
 * '\.\.\.+/?' prefix on Windows to '\./?'.  Returns non-zero if a path
 * component was fully skipped. */
static int
skip_dotdir_if_any(const char *path[], int has_parent)
{
	const char *tail;

	size_t dot_count = 0U;
	while((*path)[dot_count] == '.')
	{
		++dot_count;
	}

#ifndef _WIN32
	if(dot_count != 1U)
#else
	if(dot_count == 0U || dot_count == 2U)
#endif
	{
		return 0;
	}

	tail = &(*path)[dot_count];
	if(tail[0] == '/' || tail[0] == '\0')
	{
		const int cut_off = (has_parent || (tail[0] == '/' && tail[1] == '.'));
		if(!cut_off)
		{
			--dot_count;
		}

		/* Possibly step back one character to do not fall out of the string. */
		*path += dot_count - ((*path)[dot_count] == '\0' ? 1 : 0);
		return cut_off;
	}
	return 0;
}

const char *
make_rel_path(const char path[], const char base[])
{
	static char buf[PATH_MAX + 1];

#ifdef _WIN32
	if(path[1] == ':' && base[1] == ':' && path[0] != base[0])
	{
		canonicalize_path(path, buf, sizeof(buf));
		return buf;
	}
#endif

	/* Attempt to resolve all symbolic links in paths, this should give both the
	 * shortest and valid relative path. */
	char path_real[PATH_MAX + 1], base_real[PATH_MAX + 1];
	if(os_realpath(path, path_real) == path_real)
	{
		path = path_real;
	}
	if(os_realpath(base, base_real) == base_real)
	{
		base = base_real;
	}

	const char *p = path, *b = base;
	int i;
	int nslashes;

	while(p[0] != '\0' && p[1] != '\0' && b[0] != '\0' && b[1] != '\0')
	{
		const char *op = p, *ob = b;
		if((p = strchr(p + 1, '/')) == NULL)
			p = path + strlen(path);
		if((b = strchr(b + 1, '/')) == NULL)
			b = base + strlen(base);
		if(p - path != b - base || strnoscmp(path, base, p - path) != 0)
		{
			p = op;
			b = ob;
			break;
		}
	}

	canonicalize_path(b, buf, sizeof(buf));
	chosp(buf);

	nslashes = 0;
	for(i = 0; buf[i] != '\0'; i++)
		if(buf[i] == '/')
			nslashes++;

	buf[0] = '\0';
	while(nslashes-- > 0)
		strcat(buf, "../");
	if(*p == '/')
		p++;
	if(*p != '\0')
	{
		canonicalize_path(p, buf + strlen(buf), sizeof(buf) - strlen(buf));
	}
	chosp(buf);

	if(buf[0] == '\0')
		strcpy(buf, ".");

	return buf;
}

int
is_path_absolute(const char *path)
{
#ifdef _WIN32
	if(isalpha(path[0]) && path[1] == ':')
		return 1;
	if(path[0] == '/' && path[1] == '/')
		return 1;
#endif
	return (path[0] == '/');
}

int
is_root_dir(const char *path)
{
#ifdef _WIN32
	if(isalpha(path[0]) && stroscmp(path + 1, ":/") == 0)
		return 1;

	if(path[0] == '/' && path[1] == '/' && path[2] != '\0')
	{
		char *p = strchr(path + 2, '/');
		if(p == NULL || p[1] == '\0')
			return 1;
	}
#endif
	return (path[0] == '/' && path[1] == '\0');
}

int
is_unc_root(const char *path)
{
#ifdef _WIN32
	if(is_unc_path(path) && path[2] != '\0')
	{
		char *p = strchr(path + 2, '/');
		if(p == NULL || p[1] == '\0')
			return 1;
	}
	return 0;
#else
	return 0;
#endif
}

char *
replace_home_part(const char path[])
{
	char *result = replace_home_part_strict(path);
	if(!is_root_dir(result))
	{
		chosp(result);
	}
	return result;
}

char *
replace_home_part_strict(const char path[])
{
	static char buf[PATH_MAX + 1];
	size_t len;

	len = strlen(cfg.home_dir) - 1;
	if(strnoscmp(path, cfg.home_dir, len) == 0 &&
			(path[len] == '\0' || path[len] == '/'))
	{
		strncat(strcpy(buf, "~"), path + len, sizeof(buf) - strlen(buf) - 1);
	}
	else
	{
		copy_str(buf, sizeof(buf), path);
	}

	return buf;
}

char *
expand_tilde(const char path[])
{
	char *const expanded_path = try_replace_tilde(path);
	if(expanded_path == path)
	{
		return strdup(path);
	}
	return expanded_path;
}

char *
replace_tilde(char path[])
{
	char *const expanded_path = try_replace_tilde(path);
	if(expanded_path != path)
	{
		free(path);
	}
	return expanded_path;
}

/* Tries to expands tilde in the front of the path.  Returns the path or newly
 * allocated string without tilde. */
static char *
try_replace_tilde(const char path[])
{
	if(path[0] != '~')
	{
		return (char *)path;
	}

	if(path[1] == '\0' || path[1] == '/')
	{
		char *const result = format_str("%s%s", cfg.home_dir,
				(path[1] == '/') ? (path + 2) : "");
		return result;
	}

#ifndef _WIN32
	char user_name[NAME_MAX + 1];

	const char *p = until_first(path + 1, '/');
	int user_name_len = p - (path + 1);
	if(user_name_len + 1 > (int)sizeof(user_name))
	{
		return (char *)path;
	}

	copy_str(user_name, user_name_len + 1, path + 1);

	struct passwd *pw = getpwnam(user_name);
	if(pw == NULL)
	{
		return (char *)path;
	}

	return join_paths(pw->pw_dir, p);
#else
	return (char *)path;
#endif
}

char *
get_last_path_component(const char path[])
{
	char *slash = strrchr(path, '/');
	if(slash == NULL)
	{
		slash = (char *)path;
	}
	else if(slash[1] == '\0')
	{
		/* Skip slashes. */
		while(slash > path && slash[0] == '/')
		{
			--slash;
		}
		/* Skip until slash. */
		while(slash > path && slash[-1] != '/')
		{
			--slash;
		}
	}
	else
	{
		++slash;
	}
	return slash;
}

void
remove_last_path_component(char path[])
{
	char *slash;

	/* Get rid of trailing slashes, if any (in rather inefficient way). */
	while(ends_with_slash(path))
	{
		chosp(path);
	}

	slash = strrchr(path, '/');
	if(slash == NULL)
	{
		/* At most one item of path is left. */
		path[0] = '\0';
		return;
	}

	/* Take care to do not turn path in root into no path (should become just
	 * root). */
	slash[1] = '\0';
	if(!is_root_dir(path))
	{
		slash[0] = '\0';
	}
}

int
is_path_well_formed(const char *path)
{
#ifndef _WIN32
	return strchr(path, '/') != NULL;
#else
	return is_unc_path(path) || (strlen(path) >= 2 && path[1] == ':' &&
			drive_exists(path[0]));
#endif
}

void
ensure_path_well_formed(char *path)
{
	if(is_path_well_formed(path))
	{
		return;
	}

#ifndef _WIN32
	strcpy(path, "/");
#else
	strcpy(path, env_get("SYSTEMDRIVE"));
	strcat(path, "/");
#endif
}

void
to_canonic_path(const char path[], const char base[], char buf[],
		size_t buf_len)
{
	char normalized[PATH_MAX + 1];
	copy_str(normalized, sizeof(normalized), path);
	system_to_internal_slashes(normalized);

	if(!is_path_absolute(normalized))
	{
		char full_path[PATH_MAX*2 + 1];
		/* Assert is not level above to keep "." in curr_dir in tests, but this
		 * should be possible to change. */
		assert(is_path_absolute(base) && "Base path has to be absolute.");
		snprintf(full_path, sizeof(full_path), "%s/%s", base, normalized);
		canonicalize_path(full_path, buf, buf_len);
	}
	else
	{
		canonicalize_path(normalized, buf, buf_len);
	}

	if(!is_root_dir(buf))
	{
		chosp(buf);
	}
}

int
contains_slash(const char path[])
{
#ifndef _WIN32
	return (strchr(path, '/') != NULL);
#else
	return (strpbrk(path, "/\\") != NULL);
#endif
}

char *
find_slashr(const char *path)
{
	char *result = strrchr(path, '/');
#ifdef _WIN32
	if(result == NULL)
		result = strrchr(path, '\\');
#endif
	return result;
}

char *
cut_extension(char path[])
{
	char *e;
	char *ext;

	if((ext = strrchr(path, '.')) == NULL)
		return path + strlen(path);

	*ext = '\0';
	if((e = strrchr(path, '.')) != NULL && stroscmp(e + 1, "tar") == 0)
	{
		*ext = '.';
		ext = e;
	}
	*ext = '\0';

	return ext + 1;
}

void
split_ext(char path[], int *root_len, const char **ext_pos)
{
	char *const dot = find_ext_dot(path);

	if(dot == NULL)
	{
		const size_t len = strlen(path);

		*ext_pos = path + len;
		*root_len = len;

		return;
	}

	*dot = '\0';
	*ext_pos = dot + 1;
	*root_len = dot - path;
}

char *
get_ext(const char path[])
{
	char *const dot = find_ext_dot(path);

	if(dot == NULL)
	{
		return (char *)path + strlen(path);
	}

	return dot + 1;
}

/* Gets extension for file path.  Returns pointer to the dot or NULL if file
 * name has no extension. */
static char *
find_ext_dot(const char path[])
{
	const char *const slash = strrchr(path, '/');
	char *const dot = strrchr(path, '.');

	const int no_ext = dot == NULL
	                || (slash != NULL && dot < slash)
	                || dot == path
	                || dot == slash + 1;

	return no_ext ? NULL : dot;
}

void
exclude_file_name(char path[])
{
	if(path_exists(path, DEREF) && !is_valid_dir(path))
	{
		remove_last_path_component(path);
	}
}

int
is_parent_dir(const char path[])
{
	return path[0] == '.' && path[1] == '.' &&
		((path[2] == '/' && path[3] == '\0') || path[2] == '\0');
}

int
is_builtin_dir(const char name[])
{
	return name[0] == '.'
	    && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

int
find_cmd_in_path(const char cmd[], size_t path_len, char path[])
{
	size_t i;
	size_t paths_count;
	char **paths;

	paths = get_paths(&paths_count);
	for(i = 0; i < paths_count; i++)
	{
		char tmp_path[PATH_MAX + 1];
		snprintf(tmp_path, sizeof(tmp_path), "%s/%s", paths[i], cmd);

		/* Need to check for executable, not just a file, as this additionally
		 * checks for path with different executable extensions on Windows. */
		if(executable_exists(tmp_path))
		{
			if(path != NULL)
			{
				copy_str(path, path_len, tmp_path);
			}
			return 0;
		}
	}
	return 1;
}

const char *
get_tmpdir(void)
{
	return env_get_one_of_def("/tmp/", "TMPDIR", "TEMP", "TEMPDIR", "TMP",
			(const char *)NULL);
}

void
build_path(char buf[], size_t buf_len, const char p1[], const char p2[])
{
	p2 = skip_char(p2, '/');
	if(p2[0] == '\0')
	{
		copy_str(buf, buf_len, p1);
	}
	else
	{
		snprintf(buf, buf_len, "%s%s%s", p1, ends_with_slash(p1) ? "" : "/", p2);
	}
}

char *
join_paths(const char p1[], const char p2[])
{
	const char *slash = (ends_with_slash(p1) ? "" : "/");
	p2 = skip_char(p2, '/');
	return format_str("%s%s%s", p1, slash, p2);
}

#ifdef _WIN32

int
is_unc_path(const char path[])
{
	return (path[0] == '/' && path[1] == '/' && path[2] != '/');
}

void
system_to_internal_slashes(char path[])
{
	replace_char(path, '\\', '/');
}

void
internal_to_system_slashes(char path[])
{
	replace_char(path, '/', '\\');
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
