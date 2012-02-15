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

#include <limits.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "string_array.h"

#if !defined(NAME_MAX) && defined(_WIN32)
#include <io.h>
#define NAME_MAX FILENAME_MAX
#endif

int
add_to_string_array(char ***array, int len, int count, ...)
{
	char **p;
	va_list va;

	p = realloc(*array, sizeof(char *)*(len + count));
	if(p == NULL)
		return count;
	*array = p;

	va_start(va, count);
	while(count-- > 0)
	{
		char *arg = va_arg(va, char *);
		if(arg == NULL)
			p[len] = NULL;
		else if((p[len] = strdup(arg)) == NULL)
			break;
		len++;
	}
	va_end(va);

	return len;
}

void
remove_from_string_array(char **array, size_t len, int pos)
{
	free(array[pos]);
	memmove(array + pos, array + pos + 1, sizeof(char *)*((len - 1) - pos));
}

int
is_in_string_array(char **array, size_t len, const char *key)
{
	int i;

	if(key == NULL)
		return 0;

	for(i = 0; i < len; i++)
		if(strcmp(array[i], key) == 0)
			return 1;
	return 0;
}

int
is_in_string_array_case(char **array, size_t len, const char *key)
{
	int i;
	for(i = 0; i < len; i++)
		if(strcasecmp(array[i], key) == 0)
			return 1;
	return 0;
}

char **
copy_string_array(char **array, size_t len)
{
	char **result = malloc(sizeof(char *)*len);
	int i;
	for(i = 0; i < len; i++)
		result[i] = strdup(array[i]);
	return result;
}

int
string_array_pos(char **array, size_t len, const char *key)
{
	int i;
	for(i = 0; i < len; i++)
		if(strcmp(array[i], key) == 0)
			return i;
	return -1;
}

int
string_array_pos_case(char **array, size_t len, const char *key)
{
	int i;
	for(i = 0; i < len; i++)
		if(strcasecmp(array[i], key) == 0)
			return i;
	return -1;
}

void
free_string_array(char **array, size_t len)
{
	int i;

	for(i = 0; i < len; i++)
		free(array[i]);
	free(array);
}

void
free_wstring_array(wchar_t **array, size_t len)
{
	free_string_array((char **)array, len);
}

char **
read_file_lines(FILE *f, int *nlines)
{
	char **list = NULL;
	char name[NAME_MAX];

	*nlines = 0;
	while(get_line(f, name, sizeof(name)) != NULL)
	{
		chomp(name);
		*nlines = add_to_string_array(&list, *nlines, 1, name);
	}
	return list;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
