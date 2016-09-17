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
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* vsnprintf() vsprintf() */
#include <stdlib.h> /* calloc() free() realloc() */
#include <string.h> /* strcpy() */

/* Text buffer type. */
struct vle_textbuf
{
	size_t capacity;     /* Total amount of available space. */
	size_t len;          /* Total amount of used space. */
	char *data;          /* Allocated piece of memory. */
	int need_line_break; /* Need to put line break before newly appended data. */
};

static void appendfv(vle_textbuf *tb, int line, const char format[],
		va_list ap);
static void ensure_allocated(vle_textbuf *tb, size_t extra);

/* Definition of predefined buffer for collecting errors of the engine. */
static vle_textbuf vle_err_data;
vle_textbuf *const vle_err = &vle_err_data;

vle_textbuf *
vle_tb_create()
{
	return calloc(1, sizeof(vle_textbuf));
}

void
vle_tb_free(vle_textbuf *tb)
{
	if(tb != NULL)
	{
		vle_tb_clear(tb);
		free(tb);
	}
}

char *
vle_tb_release(vle_textbuf *tb)
{
	char *const data = (tb->data != NULL) ? tb->data : strdup("");
	free(tb);
	return data;
}

void
vle_tb_clear(vle_textbuf *tb)
{
	if(tb->data != NULL)
	{
		free(tb->data);
		tb->data = 0;
		tb->capacity = 0;
		tb->len = 0;
		tb->need_line_break = 0;
	}
}

void
vle_tb_append(vle_textbuf *tb, const char str[])
{
	vle_tb_appendf(tb, "%s", str);
}

void
vle_tb_appendf(vle_textbuf *tb, const char format[], ...)
{
	va_list ap;
	va_start(ap, format);
	appendfv(tb, 0, format, ap);
	va_end(ap);
}

void
vle_tb_append_line(vle_textbuf *tb, const char str[])
{
	vle_tb_append_linef(tb, "%s", str);
}

void
vle_tb_append_linef(vle_textbuf *tb, const char format[], ...)
{
	va_list ap;
	va_start(ap, format);
	appendfv(tb, 1, format, ap);
	va_end(ap);
}

/* Appends formatted string to the buffer.  New data is either a string or a
 * line (string that terminates with new line character).*/
static void
appendfv(vle_textbuf *tb, int line, const char format[], va_list ap)
{
	va_list aq;
	size_t needed_size;

	va_copy(aq, ap);
	needed_size = (tb->len != 0) + vsnprintf(tb->data, 0, format, ap);

	ensure_allocated(tb, needed_size);
	if(tb->need_line_break && tb->len != 0 && tb->capacity > tb->len)
	{
		strcpy(&tb->data[tb->len++], "\n");
		tb->need_line_break = 0;
	}

	tb->len += vsnprintf(tb->data + tb->len, tb->capacity - tb->len, format, aq);
	va_end(aq);

	tb->need_line_break = line;
}

/* Ensures that buffer has at least specified amount of bytes after its end. */
static void
ensure_allocated(vle_textbuf *tb, size_t extra)
{
	const size_t needed_size = tb->len + extra + 1;
	if(tb->capacity < needed_size)
	{
		char *const ptr = realloc(tb->data, needed_size);
		if(ptr != NULL)
		{
			tb->data = ptr;
			tb->capacity = needed_size;
		}
	}
}

const char *
vle_tb_get_data(vle_textbuf *tb)
{
	return (tb->data == NULL) ? "" : tb->data;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
