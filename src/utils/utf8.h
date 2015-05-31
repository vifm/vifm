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

#ifndef VIFM__UTILS__UTF8_H__
#define VIFM__UTILS__UTF8_H__

#include <stddef.h> /* size_t wchar_t */

/* Returns real width of valid and complete utf-8 character. */
size_t get_char_width(const char str[]);
/* Returns count of utf-8 characters excluding incomplete utf-8 characters. */
size_t get_normal_utf8_string_length(const char str[]);
/* Returns count of bytes of whole str or of the first max_screen_width utf-8
 * characters (so one character which take several positions on the screen are
 * counted as several positions). */
size_t get_real_string_width(const char str[], size_t max_screen_width);
/* Same as get_real_string_width(), but ignores trailing incomplete utf-8
 * characters. */
size_t get_normal_utf8_string_widthn(const char str[], size_t max_screen_width);
/* Returns number of screen characters in a utf-8 encoded str. */
size_t get_screen_string_length(const char str[]);

/* Gets screen width of the first character in the string.  Returns the
 * width or (size_t)-1 for unknown/broken characters. */
size_t utf8_get_screen_width_of_char(const char str[]);

/* Returns (string_width - string_length). */
size_t get_utf8_overhead(const char str[]);
/* Returns (string_screen_width - string_length). */
size_t get_screen_overhead(const char str[]);

/* Converts utf-8 to utf-16 string.  Returns newly allocated utf-8 string. */
wchar_t * utf8_to_utf16(const char utf8[]);

/* Extracts first utf-8 character from the string converting it to wide
 * character representation.  Returns the wide character. */
wchar_t utf8_first_char(const char utf8[]);

/* Calculates how many utf-16 chars are needed to store given utf-8 string.
 * Returns the number. */
size_t utf8_widen_len(const char utf8[]);

/* Converts utf-16 to utf-8 string. Returns newly allocated utf-16 string. */
char * utf8_from_utf16(const wchar_t utf16[]);

/* Calculate how many utf8 chars are needed to store given utf-16 string.
 * Returns the number. */
size_t utf8_narrowd_len(const wchar_t utf16[]);

/* Copies as many full utf-8 characters from source to destination as size of
 * destination buffer permits.  Returns number of actually copied bytes
 * including terminating null character. */
size_t utf8_strcpy(char dst[], const char src[], size_t dst_len);

#endif /* VIFM__UTILS__UTF8_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
