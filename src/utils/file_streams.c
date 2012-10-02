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

#include <stdio.h>

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
