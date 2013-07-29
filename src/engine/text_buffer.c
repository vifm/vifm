/* vifm
 * Copyright (C) 2012 xaizek.
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

#include "text_buffer.h"

#include <stdarg.h> /* va_list va_copy() va_start() va_end() */
#include <stddef.h> /* size_t */
#include <stdio.h> /* vsnprintf() vsprintf() */
#include <stdlib.h> /* realloc() */
#include <string.h> /* strcpy() */

static void ensure_allocated(size_t extra);

static size_t buffer_capacity;
static size_t buffer_len;
static char *buffer;

void
text_buffer_clear(void)
{
	if(buffer != NULL)
	{
		buffer[0] = '\0';
		buffer_len = 0;
	}
}

void
text_buffer_add(const char msg[])
{
	text_buffer_addf("%s", msg);
}

void
text_buffer_addf(const char format[], ...)
{
	va_list ap;
	va_list aq;
	size_t needed_size;

	va_start(ap, format);
	va_copy(aq, ap);
	needed_size = (buffer_len != 0) + vsnprintf(buffer, 0, format, ap);
	va_end(ap);

	ensure_allocated(needed_size);
	if(buffer_len != 0 && buffer_capacity > buffer_len)
	{
		strcpy(&buffer[buffer_len++], "\n");
	}

	buffer_len += vsnprintf(buffer + buffer_len, buffer_capacity - buffer_len,
			format, aq);
	va_end(aq);
}

/* Ensures that buffer has needed amount of bytes after its end. */
static void
ensure_allocated(size_t extra)
{
	size_t needed_size = buffer_len + extra + 1;
	if(buffer_capacity < needed_size)
	{
		char *ptr = realloc(buffer, needed_size);
		if(ptr != NULL)
		{
			buffer = ptr;
			buffer_capacity = needed_size;
		}
	}
}

const char *
text_buffer_get(void)
{
	return (buffer == NULL) ? "" : buffer;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
