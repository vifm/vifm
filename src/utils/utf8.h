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

/* Abbreviations:
 *  - "n"  -- "normal", excluding incomplete (broken) utf-8 sequences;
 *  - "sn" -- "screen number", screen width limit;
 *  - "w"  -- "width", in bytes;
 *  - "o"  -- "overhead";
 *  - "sw" -- "screen width", i.e. number of character positions taken on the
 *            screen;
 *  - "so" -- "screen overhead". */

/* Calculates real width of valid and complete utf-8 character in bytes.
 * Returns the width. */
size_t utf8_chrw(const char str[]);

/* Counts number of utf-8 characters excluding incomplete utf-8 characters.
 * Returns the count. */
size_t utf8_nstrlen(const char str[]);

/* Calculates count of bytes of whole str or of the first max_screen_width utf-8
 * characters (so one character which takes several positions on the screen is
 * counted as several positions).  Returns the count. */
size_t utf8_strsnlen(const char str[], size_t max_screen_width);

/* Same as utf8_strsnlen(), but ignores trailing incomplete utf-8 characters. */
size_t utf8_nstrsnlen(const char str[], size_t max_screen_width);

/* Counts number of screen characters in a utf-8 encoded str.  Returns the
 * number. */
size_t utf8_strsw(const char str[]);

/* Counts number of screen characters in a utf-8 encoded str expanding
 * tabulation according to specified tab stops.  tab_stops must be positive.
 * Returns the number. */
size_t utf8_strsw_with_tabs(const char str[], int tab_stops);

/* Gets screen width of the first character in the string.  Returns the
 * width or (size_t)-1 for unknown/broken characters. */
size_t utf8_chrsw(const char str[]);

/* Calculates string overhead (string_bytes - string_chars).  Returns the
 * overhead. */
size_t utf8_stro(const char str[]);

/* Calculates string screen overhead (string_bytes - string_screen_chars).
 * Returns the overhead. */
size_t utf8_strso(const char str[]);

/* Copies as many full utf-8 characters from source to destination as size of
 * destination buffer permits.  Returns number of actually copied bytes
 * including terminating null character. */
size_t utf8_strcpy(char dst[], const char src[], size_t dst_len);

/* Extracts first utf-8 character from the string converting it to wide
 * character representation.  Returns the wide character and sets *len to width
 * of input character, which can be 0. */
wchar_t utf8_first_char(const char utf8[], int *len);

#ifdef _WIN32

/* Converts utf-8 to utf-16 string.  Returns newly allocated utf-8 string. */
wchar_t * utf8_to_utf16(const char utf8[]);

/* Calculates how many utf-16 chars are needed to store given utf-8 string.
 * Returns the number. */
size_t utf8_widen_len(const char utf8[]);

/* Converts utf-16 to utf-8 string.  Returns newly allocated utf-16 string. */
char * utf8_from_utf16(const wchar_t utf16[]);

#endif

#endif /* VIFM__UTILS__UTF8_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
