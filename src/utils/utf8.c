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

/* returns width of utf8 character */
size_t
get_char_width(const char* string)
{
	if((string[0] & 0xe0) == 0xc0 && (string[1] & 0xc0) == 0x80)
		return 2;
	else if((string[0] & 0xf0) == 0xe0 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80)
		return 3;
	else if((string[0] & 0xf8) == 0xf0 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80 && (string[3] & 0xc0) == 0x80)
		return 4;
	else if(string[0] == '\0')
		return 0;
	else
		return 1;
}

/* returns count of bytes of whole string or of first max_len utf8 characters */
size_t
get_real_string_width(char *string, size_t max_len)
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

static size_t
guess_char_width(char c)
{
	if((c & 0xe0) == 0xc0)
		return 2;
	else if((c & 0xf0) == 0xe0)
		return 3;
	else if((c & 0xf8) == 0xf0)
		return 4;
	else
		return 1;
}

/* returns count utf8 characters excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_length(const char *string)
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

/* returns count of bytes excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_widthn(const char *string, size_t max)
{
#ifndef _WIN32
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
#else
	return MIN(strlen(string), max);
#endif
}

/* returns count of bytes excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_width(const char *string)
{
	size_t length = 0;
	while(*string != '\0')
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

/* returns count of utf8 characters in string */
size_t
get_utf8_string_length(const char *string)
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

/* returns (string_width - string_length) */
size_t
get_utf8_overhead(const char *string)
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
