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

#include "env.h"

#include <stdarg.h> /* va_list va_start() va_arg() va_end() */
#ifdef _WIN32
#include <stdio.h> /* sprintf() */
#endif
#include <stdlib.h> /* free() getenv() setenv() unsetenv() */
#include <string.h> /* strchr() strlen() */
#include <wchar.h> /* wcschr() */

#include "string_array.h"
#include "utf8.h"

char **
env_list(int *count)
{
#ifndef _WIN32
	extern char **environ;

	char **lst = NULL;
	char **env;

	*count = 0;

	for(env = environ; *env != NULL; ++env)
	{
		char *const equal = strchr(*env, '=');
		/* Actually equal shouldn't be NULL unless environ content is corrupted.
		 * But the check below won't harm. */
		if(equal != NULL)
		{
			*equal = '\0';
			*count = add_to_string_array(&lst, *count, *env);
			*equal = '=';
		}
	}

	return lst;
#else
	extern wchar_t **_wenviron;

	char **lst = NULL;
	wchar_t **env;

	*count = 0;

	for(env = _wenviron; *env != NULL; ++env)
	{
		wchar_t *const equal = wcschr(*env, L'=');
		/* Actually equal shouldn't be NULL unless environ content is corrupted.
		 * But the check below won't harm. */
		if(equal != NULL)
		{
			char *val;

			*equal = L'\0';
			val = utf8_from_utf16(*env);
			*equal = L'=';

			if(val != NULL && put_into_string_array(&lst, *count, val) == *count + 1)
			{
				++*count;
			}
			else
			{
				free(val);
			}
		}
	}

	return lst;
#endif
}

const char *
env_get(const char name[])
{
#ifndef _WIN32
	return getenv(name);
#else
	/* This cache only grows, but as number of distinct variables is likely to be
	 * limited to a reasonable number, not much space should be wasted. */
	static char **names, **values;
	static int ncached;

	wchar_t *u16name;
	const wchar_t *u16val;

	int pos = string_array_pos(names, ncached, name);
	if(pos == -1)
	{
		if(add_to_string_array(&names, ncached, name) == ncached + 1)
		{
			if(put_into_string_array(&values, ncached, NULL) == ncached + 1)
			{
				pos = ncached;
				++ncached;
			}
		}
	}

	if(pos == -1)
	{
		return NULL;
	}

	u16name = utf8_to_utf16(name);
	if(u16name == NULL)
	{
		--ncached;
		return NULL;
	}

	u16val = _wgetenv(u16name);
	free(u16name);
	if(u16val == NULL)
	{
		return NULL;
	}

	values[pos] = utf8_from_utf16(u16val);
	return values[pos];
#endif
}

const char *
env_get_def(const char name[], const char def[])
{
	const char *result = env_get(name);
	if(result == NULL || result[0] == '\0')
	{
		result = def;
	}
	return result;
}

const char *
env_get_one_of_def(const char def[], ...)
{
	va_list ap;
	const char *env_name;
	const char *result = def;

	va_start(ap, def);

	while((env_name = va_arg(ap, const char *)) != NULL)
	{
		const char *val = env_get_def(env_name, NULL);
		if(val != NULL)
		{
			result = val;
			break;
		}
	}

	va_end(ap);

	return result;
}

void
env_set(const char name[], const char value[])
{
#ifndef _WIN32
	setenv(name, value, 1);
#else
	wchar_t *u16buf;
	char buf[strlen(name) + 1 + strlen(value) + 1];
	sprintf(buf, "%s=%s", name, value);
	u16buf = utf8_to_utf16(buf);
	_wputenv(u16buf);
	free(u16buf);
#endif
}

void
env_remove(const char name[])
{
#ifndef _WIN32
	unsetenv(name);
#else
	env_set(name, "");
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
