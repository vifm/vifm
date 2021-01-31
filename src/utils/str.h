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

#include <inttypes.h> /* PRIu64 */
#include <stddef.h> /* size_t wchar_t */

#include "macros.h"
#include "test_helpers.h"

#if defined(_WIN32) && !defined(_WIN64)
#ifdef BROKEN_SWPRINTF
#define WPRINTF_WSTR L"s"
#else
#define WPRINTF_WSTR L"ls"
#endif
#define PRINTF_ULL PRIu64
#else
#define WPRINTF_WSTR L"ls"
#define PRINTF_ULL "llu"
#endif

/* Various string functions. */

/* Checks whether str starts with the given prefix, which should be string
 * literal.  Returns non-zero if it's so, otherwise zero is returned. */
#define starts_with_lit(str, prefix) \
	starts_withn((str), (prefix), sizeof(prefix)/sizeof((prefix)[0]) - 1U)

/* Removes at most one trailing newline character. */
void chomp(char str[]);

/* Converts multibyte string to a wide string.  On success returns newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
wchar_t * to_wide(const char s[]);

/* Same as to_wide() except that it never fails due to conversion errors and
 * returns something, thus returns NULL only on memory allocation failure. */
wchar_t * to_wide_force(const char s[]);

/* Calculates length of multibyte string after converting it to wide string.
 * Returns the length. */
size_t wide_len(const char s[]);

/* Duplicates a wide-character string.  Returns pointer to new string or NULL on
 * error. */
wchar_t * vifm_wcsdup(const wchar_t ws[]);

/* Checks whether str starts with the given prefix.  Returns non-zero if it's
 * so, otherwise zero is returned. */
int starts_with(const char str[], const char prefix[]);

/* Checks whether *str starts with the given prefix and advances *str to point
 * past the prefix.  Returns non-zero if prefix was skipped, otherwise zero is
 * returned. */
int skip_prefix(const char **str, const char prefix[]);

/* Checks whether str ends with the given suffix and removes it if found.
 * Returns non-zero if it's so, otherwise zero is returned. */
int cut_suffix(char str[], const char suffix[]);

/* Checks whether str starts with the given prefix of specified length.  Returns
 * non-zero if it's so, otherwise zero is returned. */
int starts_withn(const char str[], const char prefix[], size_t prefix_len);

/* Checks whether str ends with the given suffix.  Returns non-zero if it's so,
 * otherwise zero is returned. */
int ends_with(const char str[], const char suffix[]);

/* Checks whether str ends with the given suffix in a case insensitive way.
 * Returns non-zero if it's so, otherwise zero is returned. */
int ends_with_case(const char str[], const char suffix[]);

/* Checks whether the string starts and ends with specified mark characters.
 * Substring can't be empty.  The string can't consist of single mark character
 * when they are equal.  Returns non-zero if so, otherwise zero is returned. */
int surrounded_with(const char str[], char left, char right);

/* Converts wide string into narrow one.  Returns new string which caller should
 * free or NULL on error. */
char * to_multibyte(const wchar_t s[]);

/* Converts characters of the string to lower case while they fit in the buffer.
 * Returns zero on success or non-zero if output buffer is too small. */
int str_to_lower(const char str[], char buf[], size_t buf_len);

/* Converts characters of the string to upper case while they fit in the buffer.
 * Returns zero on success or non-zero if output buffer is too small. */
int str_to_upper(const char str[], char buf[], size_t buf_len);

/* Converts all characters of the string s to their lowercase equivalents. */
void wcstolower(wchar_t str[]);

/* Replaces first occurrence of the c character in the str with '\0'.  Nothing
 * is done if the character isn't found. */
void break_at(char str[], char c);

/* Replaces the last occurrence of the c character in the str with '\0'.
 * Nothing is done if the character isn't found. */
void break_atr(char str[], char c);

/* Skips consecutive whitespace characters.  Returns pointer to the next
 * character in the str. */
char * skip_whitespace(const char str[]);

/* Checks if the c is one of characters in the list string. c cannot be '\0'. */
int char_is_one_of(const char *list, char c);

/* Compares strings in OS dependent way. */
int stroscmp(const char *s, const char *t);

/* Compares part of strings in OS dependent way. */
int strnoscmp(const char *s, const char *t, size_t n);

/* Wraps stroscmp() for use with qsort(). */
int strossorter(const void *s, const void *t);

/* Returns pointer to first character after last occurrence of c in str or
 * str. */
char * after_last(const char *str, char c);

/* Returns pointer to the first occurrence of c in str or a pointer to its
 * end. */
char * until_first(const char str[], char c);

/* Returns pointer to the next character after first occurrence of c in str or a
 * pointer to its end. */
char * after_first(const char str[], char c);

/* Replaces *str with a copy of the with string.  *str can be NULL or equal to
 * the with (then function does nothing).  Returns non-zero if memory allocation
 * failed. */
int replace_string(char **str, const char with[]);

/* Same as replace_string(), but also allows "to" to be NULL, in which case
 * *str is freed and set to NULL. */
int update_string(char **str, const char to[]);

/* Replaces *str with the with string, if it's non-NULL.  Returns non-zero if
 * with is NULL, otherwise zero is returned. */
int put_string(char **str, char with[]);

/* Adds a character to the end of the string.  Returns the str argument. */
char * strcatch(char str[], char c);

/* Prepends prefix to a string.  Might reallocate the string.  Updates *len
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
int strprepend(char **str, size_t *len, const char prefix[]);

/* Appends single character to a string.  Might reallocate it.  Updates *len
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
int strappendch(char **str, size_t *len, char c);

/* Appends suffix to a string.  Might reallocate the string.  Updates *len
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
int strappend(char **str, size_t *len, const char suffix[]);

/* Appends single character to statically allocated string of current length
 * *len which has size as its limit.  Updates *len appropriately.  Returns zero
 * if string didn't overflow, otherwise non-zero is returned. */
int sstrappendch(char str[], size_t *len, size_t size, char c);

/* Appends suffix to statically allocated string of current length *len which
 * has size as its limit.  Updates *len appropriately.  Returns zero if string
 * didn't overflow, otherwise non-zero is returned. */
int sstrappend(char str[], size_t *len, size_t size, const char suffix[]);

/* Pads buffer pointed to by str to be at least of width "width + 1". */
void stralign(char str[], size_t width, char pad, int left_align);

/* Ensures that str is of width (in character positions) less than or equal to
 * max_width and is right aligned putting ellipsis on the left side if needed.
 * Returns newly allocated modified string. */
char * left_ellipsis(const char str[], size_t max_width, const char ell[]);

/* Ensures that str is of width (in character positions) less than or equal to
 * max_width and is left aligned putting ellipsis on the right side if needed.
 * Returns newly allocated modified string. */
char * right_ellipsis(const char str[], size_t max_width, const char ell[]);

/* "Breaks" single line it two parts (before and after separator), and
 * re-formats it filling specified width by putting "left part", padded centre
 * followed by "right part".  Frees the str.  Returns re-formatted string in
 * newly allocated buffer. */
char * break_in_two(char str[], size_t max, const char separator[]);

/* A wrapper of swprintf() functions to make its differences on various
 * platforms transparently in other parts of the program. */
int vifm_swprintf(wchar_t str[], size_t len, const wchar_t format[], ...);

/* Extracts non-empty part of the string with separators.  Returns pointer to
 * the beginning of the next part or NULL at the end of the string. */
const char * extract_part(const char str[], const char separators[],
		char part_buf[]);

/* Skips all leading characters of the str which are equal to the c. */
char * skip_char(const char str[], char c);

/* Escapes chars symbols in the string.  Returns new string, caller should free
 * it. */
char * escape_chars(const char string[], const char chars[]);

/* Unescapes string in place (removes extra slashes).  regexp flag narrows set
 * of unescaped characters to "/". */
void unescape(char s[], int regexp);

/* Returns non-zero if the string is NULL or empty. */
int is_null_or_empty(const char string[]);

/* Formats string like printf, but instead of printing it, allocates memory and
 * and prints it there.  Returns newly allocated string, which should be freed
 * by the caller, or NULL if there is not enough memory. */
char * format_str(const char format[], ...) _gnuc_printf(1, 2);

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
 * character.  Does nothing for zero dst_len.  Returns number of characters
 * written, including terminating null character. */
size_t copy_str(char dst[], size_t dst_len, const char src[]);

/* Converts string into integer handling underflow and overflow.  The string is
 * assumed to consist of numbers.  Returns converted number, which is
 * INT_MIN/INT_MAX in case of underflow/overflow happened. */
int str_to_int(const char str[]);

/* Converts line to a number.  Handles overflow/underflow by saturating
 * resulting value.  Returns non-zero on success and zero otherwise. */
int read_int(const char line[], int *i);

/* Replaces all occurrences of the from non-nul character in the str to the to
 * character. */
void replace_char(char str[], char from, char to);

/* Splits string on a separator multiple times returning next part.  Empty items
 * are skipped.  *state must be NULL for the first call.  Usage example:
 *   char *part = input, *state = NULL;
 *   while((part = split_and_get(part, ':', &state)) != NULL)
 *   {
 *     process <part>;
 *   }
 * Each next step undoes string change made on the previous one, so it could
 * also be (parts of <input> will be added from left to right):
 *   char *part = input, *state = NULL;
 *   while((part = split_and_get(part, ':', &state)) != NULL)
 *   {
 *     process <input>;
 *   } */
char * split_and_get(char str[], char sep, char **state);

/* Like split_and_get(), but works on comma-separated list with double commas
 * signifying literal comma character. */
char * split_and_get_dc(char str[], char **state);

/* Counts lines in the text.  Considers wrapping around max_width column unless
 * it's equal to INT_MAX. */
int count_lines(const char text[], int max_width);

/* Counts number of c char occurrences in the s string.  Returns the number. */
size_t chars_in_str(const char s[], char c);

/* Clones string passed as the argument doubling specified character in the
 * process.  Returns cloned value. */
char * double_char(const char str[], char c);

#ifndef HAVE_STRCASESTR

/* Same as strstr(), but in case insensitive way. */
char * strcasestr(const char haystack[], const char needle[]);

#endif

#ifdef _WIN32

#ifndef strtok_r
#define strtok_r(str, delim, saveptr) (*(saveptr) = strtok((str), (delim)))
#endif

#endif

TSTATIC_DEFS(
	void squash_double_commas(char str[]);
)

#endif /* VIFM__UTILS__STR_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
