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

#include <stddef.h> /* size_t ssize_t */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* strlen() */

#include "file_streams.h"

static int get_char(FILE *fp);

char *
get_line(FILE *fp, char *buf, size_t bufsz)
{
	int c = '\0';
	char *start = buf;

	while(bufsz-- > 1 && (c = get_char(fp)) != EOF)
	{
		*buf++ = c;
		if(c == '\n')
			break;
		bufsz--;
	}
	*buf = '\0';

	return (c == EOF && buf == start) ? NULL : start;
}

void
skip_until_eol(FILE *fp)
{
	while(get_char(fp) != '\n' && !feof(fp));
}

/* Returns next character from file stream. */
static int
get_char(FILE *fp)
{
	int c = fgetc(fp);
	if(c == '\r')
	{
		int c2 = fgetc(fp);
		if(c2 != '\n')
			ungetc(c2, fp);
		c = '\n';
	}
	return c;
}

void
remove_eol(FILE *fp)
{
	int c = fgetc(fp);
	if(c == '\r')
		c = fgetc(fp);
	if(c != '\n')
		ungetc(c, fp);
}

char *
read_line(FILE *fp, char buffer[])
{
#if defined(HAVE_GETLINE_FUNC) && HAVE_GETLINE_FUNC
	size_t size = 0;
	ssize_t len;
	if((len = getline(&buffer, &size, fp)) == -1)
	{
		free(buffer);
		return NULL;
	}

	if(len > 0 && buffer[len - 1] == '\n')
	{
		buffer[len - 1] = '\0';
	}
	return buffer;
#else
	enum { PART_BUFFER_LEN = 512 };
	char part_buffer[PART_BUFFER_LEN];
	char *last_allocated_block = NULL;
	size_t len = 0;

	while(fgets(part_buffer, sizeof(part_buffer), fp) != NULL)
	{
		const size_t part_len = strlen(part_buffer);
		const int eol = (part_len > 0) && (part_buffer[part_len - 1] == '\n');
		const size_t new_len = len + (part_len - eol);

		if((last_allocated_block = realloc(buffer, new_len + 1)) == NULL)
		{
			break;
		}

		if(eol)
		{
			part_buffer[part_len - 1] = '\0';
		}

		buffer = last_allocated_block;
		strcpy(buffer + len, part_buffer);

		if(eol)
		{
			break;
		}

		len = new_len;
	}

	if(last_allocated_block == NULL)
	{
		free(buffer);
		buffer = NULL;
	}

	return buffer;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
