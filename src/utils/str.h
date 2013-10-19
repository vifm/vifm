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

#ifndef VIFM__UTILS__STR_H__
#define VIFM__UTILS__STR_H__

#include <stddef.h> /* size_t */
#include <wchar.h> /* wchar_t */

#if defined(_WIN64)
#define WPRINTF_MBSTR L"s"
#define WPRINTF_WSTR L"ls"
#define PRINTF_PID_T "%llu"
#define PRINTF_SIZE_T "%llu"
#elif defined(_WIN32)
#define WPRINTF_MBSTR L"S"
#define WPRINTF_WSTR L"s"
#define PRINTF_PID_T "%d"
#define PRINTF_SIZE_T "%u"
#else
#define WPRINTF_MBSTR L"s"
#define WPRINTF_WSTR L"ls"
#define PRINTF_PID_T "%d"
#define PRINTF_SIZE_T "%lu"
#endif

/* Various string functions. */

/* Checks whether str starts with the given prefix, which should be string
 * literal.  Returns non-zero if it's so, otherwise zero is returned. */
#define starts_with_lit(str, prefix) \
	starts_withn((str), (prefix), sizeof(prefix)/sizeof((prefix)[0]) - 1U)

/* Removes at most one trailing newline character. */
void chomp(char str[]);
/* Removes all trailing whitespace. Returns new length of the string. */
size_t trim_right(char *text);
/* Converts multibyte string to a wide string.  On success returns newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
wchar_t * to_wide(const char s[]);
wchar_t * my_wcsdup(const wchar_t *ws);
/* Checks whether str starts with the given prefix.  Returns non-zero if it's
 * so, otherwise zero is returned. */
int starts_with(const char str[], const char prefix[]);
/* Checks whether str starts with the given prefix of specified length.  Returns
 * non-zero if it's so, otherwise zero is returned. */
int starts_withn(const char str[], const char prefix[], size_t prefix_len);
int ends_with(const char *str, const char *suffix);
char * to_multibyte(const wchar_t *s);
void strtolower(char *s);
/* Converts all characters of the string s to their lowercase equivalents. */
void wcstolower(wchar_t s[]);
/* Replaces first occurrence of the c character in the str with '\0'.  Nothing
 * is done if the character isn't found. */
void break_at(char str[], char c);
/* Replaces the last occurrence of the c character in the str with '\0'.
 * Nothing is done if the character isn't found. */
void break_atr(char str[], char c);
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
/* Replaces *str with a copy of the with string.  *str can be NULL or equal to
 * the with (then function does nothing).  Returns non-zero if memory allocation
 * failed. */
int replace_string(char **str, const char with[]);
/* Adds a character to the end of a string. */
char * strcatch(char *str, char c);
/* A wrapper of swprintf() functions to make its differences on various
 * platforms transparently in other parts of the program. */
int my_swprintf(wchar_t *str, size_t len, const wchar_t *format, ...);
/* Extracts next non-empry part of string with separators.  Returns pointer to
 * the beginning of the next part or NULL at the end of a string. */
const char * extract_part(const char str[], char separator, char part_buf[]);
/* Skips all leading characters of the str which are equal to the c. */
char * skip_char(const char str[], char c);
/* Escapes chars symbols in the string.  Returns new string, caller should free
 * it. */
char * escape_chars(const char string[], const char chars[]);
/* Returns non-zero if the string is NULL or empty. */
int is_null_or_empty(const char string[]);
/* Formats string like printf, but instead of printing it, allocates memory and
 * and prints it there.  Returns newly allocated string, which should be freed
 * by the caller, or NULL if there is not enough memory. */
char * format_str(const char format[], ...);
/* Replaces all occurrences of horizontal tabulation character with appropriate
 * number of spaces.  The max parameter designates the maximum number of screen
 * characters to put into the buf.  The tab_stops parameter shows how many
 * character position are taken by one tabulation.  Returns pointer to the first
 * unprocessed character of the line. */
const char * expand_tabulation(const char line[], size_t max, size_t tab_stops,
		char buf[]);
/* Returns the first wide character of a multi-byte string. */
wchar_t get_first_wchar(const char str[]);
/* Concatenates the str with the with by reallocating string.  Returns str, when
 * there is not enough memory or it was enough space in piece of memory pointed
 * to by the str (check *len in this case). */
char * extend_string(char str[], const char with[], size_t *len);
/* Checks that at least one of Unicode letters (for UTF-8) is an uppercase
 * letter.  Returns non-zero for that case, otherwise zero is returned. */
int has_uppercase_letters(const char str[]);
/* Copies characters from the string pointed to by str to piece of memory of
 * size dst_len pointed to by dst.  Ensures that copied string ends with null
 * character.  Does nothing for zero dst_len. */
void copy_str(char dst[], size_t dst_len, const char src[]);

#if defined(_WIN32) && !defined(strtok_r)
#define strtok_r(str, delim, saveptr) (*(saveptr) = strtok((str), (delim)))
#endif

#endif /* VIFM__UTILS__STR_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
