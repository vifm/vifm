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

#include "utf8.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t wchar_t */
#include <string.h> /* strlen() */

#include "macros.h"
#include "utils.h"

static size_t guess_char_width(char c);
static wchar_t utf8_char_to_wchar(const char str[], size_t char_width);
static size_t get_char_screen_width(const char str[], size_t char_width);

size_t
get_char_width(const char str[])
{
	/* On Windows utf-8 is not used. */
#ifndef _WIN32
	const size_t expected = guess_char_width(str[0]);
	if(expected == 2 && (str[1] & 0xc0) == 0x80)
		return 2;
	else if(expected == 3 && (str[1] & 0xc0) == 0x80 && (str[2] & 0xc0) == 0x80)
		return 3;
	else if(expected == 4 && (str[1] & 0xc0) == 0x80 && (str[2] & 0xc0) == 0x80 &&
			(str[3] & 0xc0) == 0x80)
		return 4;
	else if(str[0] == '\0')
		return 0;
#endif
	return 1;
}

size_t
get_real_string_width(const char str[], size_t max_screen_width)
{
	size_t width = 0;
	while(*str != '\0' && max_screen_width != 0)
	{
		size_t char_width = get_char_width(str);
		size_t char_screen_width = get_char_screen_width(str, char_width);
		if(char_screen_width > max_screen_width)
		{
			break;
		}
		max_screen_width -= char_screen_width;
		width += char_width;
		str += char_width;
	}
	return width;
}

size_t
get_normal_utf8_string_length(const char str[])
{
	size_t length = 0;
	while(*str != '\0')
	{
		size_t char_width = guess_char_width(*str);
		if(char_width <= strlen(str))
			length++;
		else
			break;
		str += char_width;
	}
	return length;
}

size_t
get_normal_utf8_string_widthn(const char str[], size_t max_screen_width)
{
	size_t length = 0;
	while(*str != '\0' && max_screen_width > 0)
	{
		size_t char_screen_width;
		const size_t char_width = guess_char_width(*str);
		if(char_width > strlen(str))
		{
			break;
		}

		char_screen_width = get_char_screen_width(str, char_width);
		if(char_screen_width > max_screen_width)
		{
			break;
		}

		length += char_width;
		max_screen_width -= char_screen_width;
		str += char_width;
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

/* Converts one utf-8 encoded character to wide character form. */
static wchar_t
utf8_char_to_wchar(const char str[], size_t char_width)
{
	/* First mask is a fake one, to omit decrementing of char_width. */
	static const int masks[] = { 0x00, 0xff, 0x1f, 0x0f, 0x07 };

	wchar_t result;

	assert(char_width != 0 && "There are no zero width utf-8 characters.");
	assert(char_width < ARRAY_LEN(masks) && "Too long utf-8 character.");

	result = *str&masks[char_width];
	while(--char_width != 0)
	{
		result = (result << 6)|(*++str&0x3f);
	}

	return result;
}

size_t
get_screen_string_length(const char str[])
{
	size_t length = 0;
	while(*str != '\0')
	{
		const size_t char_width = get_char_width(str);
		const size_t char_screen_width = get_char_screen_width(str, char_width);
		str += char_width;
		length += char_screen_width;
	}
	return length;
}

/* Returns width of the character in the terminal. */
static size_t
get_char_screen_width(const char str[], size_t char_width)
{
	const wchar_t wide = utf8_char_to_wchar(str, char_width);
	const size_t result = vifm_wcwidth(wide);
	return (result == (size_t)-1) ? 1 : result;
}

size_t
get_utf8_overhead(const char str[])
{
	size_t overhead = 0;
	while(*str != '\0')
	{
		size_t char_width = get_char_width(str);
		str += char_width;
		overhead += char_width - 1;
	}
	return overhead;
}

size_t
get_screen_overhead(const char str[])
{
	size_t overhead = 0;
	while(*str != '\0')
	{
		const size_t char_width = get_char_width(str);
		const size_t char_screen_width = get_char_screen_width(str, char_width);
		str += char_width;
		overhead += (char_width - 1) - (char_screen_width - 1);
	}
	return overhead;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
