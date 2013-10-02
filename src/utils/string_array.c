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

#include "string_array.h"

#include <stdarg.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fprintf() */
#include <stdlib.h>
#include <string.h>

#include "file_streams.h"
#include "fs_limits.h"
#include "str.h"

int
add_to_string_array(char ***array, int len, int count, ...)
{
	char **p;
	va_list va;

	p = realloc(*array, sizeof(char *)*(len + count));
	if(p == NULL)
		return len;
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

int
put_into_string_array(char ***array, int len, char item[])
{
	char **const arr = realloc(*array, sizeof(char *)*(len + 1));
	if(arr != NULL)
	{
		*array = arr;
		arr[len++] = item;
	}
	return len;
}

void
remove_from_string_array(char **array, size_t len, int pos)
{
	free(array[pos]);
	memmove(array + pos, array + pos + 1, sizeof(char *)*((len - 1) - pos));
}

int
is_in_string_array(char *array[], size_t len, const char item[])
{
	const int pos = string_array_pos(array, len, item);
	return pos >= 0;
}

int
is_in_string_array_case(char *array[], size_t len, const char item[])
{
	const int pos = string_array_pos_case(array, len, item);
	return pos >= 0;
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
string_array_pos(char *array[], size_t len, const char item[])
{
	size_t i = len;
	if(item != NULL)
	{
		for(i = 0; i < len; i++)
		{
			if(strcmp(array[i], item) == 0)
			{
				break;
			}
		}
	}
	return (i < len) ? i : -1;
}

int
string_array_pos_case(char *array[], size_t len, const char item[])
{
	size_t i = len;
	if(item != NULL)
	{
		for(i = 0; i < len; i++)
		{
			if(strcasecmp(array[i], item) == 0)
			{
				break;
			}
		}
	}
	return (i < len) ? i : -1;
}

void
free_string_array(char *array[], size_t len)
{
	if(array != NULL)
	{
		free_strings(array, len);
		free(array);
	}
}

void
free_strings(char *array[], size_t len)
{
	size_t i;
	for(i = 0; i < len; i++)
	{
		free(array[i]);
	}
}

void
free_wstring_array(wchar_t **array, size_t len)
{
	free_string_array((char **)array, len);
}

char **
read_file_of_lines(const char filepath[], int *nlines)
{
	char **list;
	FILE *fp;

	if((fp = fopen(filepath, "r")) == NULL)
	{
		return NULL;
	}

	list = read_file_lines(fp, nlines);
	if(list == NULL)
	{
		list = malloc(1U);
		*nlines = 0;
	}

	fclose(fp);
	return list;
}

char **
read_file_lines(FILE *f, int *nlines)
{
	char **list = NULL;
	char *line = NULL;

	*nlines = 0;
	while((line = read_line(f, line)) != NULL)
	{
		*nlines = add_to_string_array(&list, *nlines, 1, line);
	}
	return list;
}

int
write_file_of_lines(const char filepath[], char *lines[], size_t nlines)
{
	FILE *fp;
	size_t i;

	if((fp = fopen(filepath, "w")) == NULL)
	{
		return 1;
	}

	for(i = 0U; i < nlines; i++)
	{
		fprintf(fp, "%s\n", lines[i]);
	}

	fclose(fp);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
