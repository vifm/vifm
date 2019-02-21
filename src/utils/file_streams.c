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

#include "file_streams.h"

#include <stddef.h> /* NULL size_t ssize_t */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* strlen() */

static int get_char(FILE *fp);

char *
read_line(FILE *fp, char buf[])
{
	enum { PART_BUFFER_LEN = 512 };
	char part_buf[PART_BUFFER_LEN];
	char *last_allocated_block = NULL;
	size_t len = 0;

	while(get_line(fp, part_buf, sizeof(part_buf)) != NULL)
	{
		size_t part_len = strlen(part_buf);
		const int eol = (part_len > 0) && (part_buf[part_len - 1] == '\n');
		const size_t new_len = len + (part_len - eol);

		last_allocated_block = realloc(buf, new_len + 1);
		if(last_allocated_block == NULL)
		{
			break;
		}

		if(eol)
		{
			part_buf[--part_len] = '\0';
		}

		buf = last_allocated_block;
		strcpy(buf + len, part_buf);

		if(eol)
		{
			break;
		}

		len = new_len;
	}

	if(last_allocated_block == NULL)
	{
		free(buf);
		buf = NULL;
	}

	return buf;
}

char *
get_line(FILE *fp, char buf[], size_t bufsz)
{
	int c = '\0';
	char *start = buf;

	if(bufsz <= 1)
	{
		return NULL;
	}

	while(bufsz > 1 && (c = get_char(fp)) != EOF)
	{
		*buf++ = c;
		bufsz--;
		if(c == '\n')
		{
			break;
		}
	}
	*buf = '\0';

	return (c == EOF && buf == start) ? NULL : start;
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
skip_bom(FILE *fp)
{
	int c;

	c = fgetc(fp);
	if(c != 0xef)
	{
		ungetc(c, fp);
		return;
	}

	c = fgetc(fp);
	if(c != 0xbb)
	{
		ungetc(c, fp);
		return;
	}

	c = fgetc(fp);
	if(c != 0xbf)
	{
		ungetc(c, fp);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
