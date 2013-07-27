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
#include <string.h> /* strlen() */
#endif
#include <stdlib.h> /* getenv() unsetenv() env_set() */

const char *
env_get(const char name[])
{
	return getenv(name);
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
	char buf[strlen(name) + 1 + strlen(value) + 1];
	sprintf(buf, "%s=%s", name, value);
	putenv(buf);
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
/* vim: set cinoptions+=t0 : */
