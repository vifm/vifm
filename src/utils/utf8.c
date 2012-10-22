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

#include <string.h>

#include "macros.h"

#include "utf8.h"

static size_t guess_char_width(char c);

size_t
get_char_width(const char string[])
{
	/* On Windows utf-8 is not used. */
#ifndef _WIN32
	const size_t expected = guess_char_width(string[0]);
	if(expected == 2 && (string[1] & 0xc0) == 0x80)
		return 2;
	else if(expected == 3 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80)
		return 3;
	else if(expected == 4 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80 && (string[3] & 0xc0) == 0x80)
		return 4;
	else if(string[0] == '\0')
		return 0;
#endif
	return 1;
}

size_t
get_real_string_width(const char string[], size_t max_len)
{
	size_t width = 0;
	while(*string != '\0' && max_len-- != 0)
	{
		size_t char_width = get_char_width(string);
		width += char_width;
		string += char_width;
	}
	return width;
}

size_t
get_normal_utf8_string_length(const char string[])
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = guess_char_width(*string);
		if(char_width <= strlen(string))
			length++;
		else
			break;
		string += char_width;
	}
	return length;
}

size_t
get_normal_utf8_string_widthn(const char string[], size_t max)
{
	size_t length = 0;
	while(*string != '\0' && max-- > 0)
	{
		size_t char_width = guess_char_width(*string);
		if(char_width <= strlen(string))
			length += char_width;
		else
			break;
		string += char_width;
	}
	return length;
}

/* Determines width of a utf-8 characted by its first byte. */
static size_t
guess_char_width(char c)
{
	/* On Windows utf-8 is not used. */
#ifndef _WIN32
	if((c & 0xe0) == 0xc0)
		return 2;
	else if((c & 0xf0) == 0xe0)
		return 3;
	else if((c & 0xf8) == 0xf0)
		return 4;
#endif
	return 1;
}

size_t
get_utf8_string_length(const char string[])
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		length++;
	}
	return length;
}

size_t
get_utf8_overhead(const char string[])
{
	size_t overhead = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		overhead += char_width - 1;
	}
	return overhead;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
