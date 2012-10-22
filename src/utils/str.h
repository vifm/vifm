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

#ifndef __STR_H__
#define __STR_H__

#include <wchar.h> /* wchar_t */

#ifndef _WIN32
#define WPRINTF_MBSTR L"s"
#define WPRINTF_WSTR L"ls"
#else
#define WPRINTF_MBSTR L"S"
#define WPRINTF_WSTR L"s"
#endif

/* Various string functions. */

void chomp(char *text);
/* Removes all trailing whitespace. Returns new length of the string. */
size_t trim_right(char *text);
wchar_t * to_wide(const char *s);
wchar_t * my_wcsdup(const wchar_t *ws);
int starts_with(const char *str, const char *prefix);
int ends_with(const char *str, const char *suffix);
char * to_multibyte(const wchar_t *s);
void strtolower(char *s);
void break_at(char *str, char c);
void break_atr(char *str, char c);
char * skip_non_whitespace(const char *str);
/* Skips consecutive whitespace characters. */
char * skip_whitespace(const char *str);
/* Checks if the c is one of characters in the list string. c cannot be '\0'. */
int char_is_one_of(const char *list, char c);
/* Compares strings in OS dependent way. */
int stroscmp(const char *s, const char *t);
/* Compares part of strings in OS dependent way. */
int strnoscmp(const char *s, const char *t, size_t n);
/* Returns pointer to first character after last occurrence of c in str or
 * str. */
char * after_last(const char *str, char c);
/* Returns pointer to the first occurrence of c in str or a pointer to its
 * end. */
char * until_first(const char str[], char c);
/* Replaces *str with a copy of with string.  *str can be NULL or equal to
 * with (then function does nothing).  Returns non-zero if memory allocation
 * failed. */
int replace_string(char **str, const char with[]);
/* Adds a character to the end of a string. */
char * strcatch(char *str, char c);
/* A wrapper of swprintf() functions to make its differences on various
 * platforms transparently in other parts of the program. */
int my_swprintf(wchar_t *str, size_t len, const wchar_t *format, ...);
/* Extracts next part of string with separators.  Call with str - 1 first time,
 * and with value returned until pointer to '\0' is returned. */
const char * extract_part(const char str[], char separator, char part_buf[]);
/* Escapes chars symbols in the string.  Returns new string, caller should free
 * it. */
char * escape_chars(const char string[], const char chars[]);
/* Returns non-zero if the string is NULL or empty. */
int is_null_or_empty(const char string[]);
/* Returns pointer to first character in the string not equal to the ch. */
char * skip_all(const char string[], char ch);
/* Formats string like printf, but instead of printing it, allocates memory and
 * and prints it there.  Returns newly allocated string, which should be freed
 * by the caller, or NULL if there is not enough memory. */
char * format_str(const char format[], ...);
#ifdef _WIN32
char * strtok_r(char str[], const char delim[], char *saveptr[]);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
