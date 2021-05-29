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

#ifndef VIFM__UTILS__STRING_ARRAY_H__
#define VIFM__UTILS__STRING_ARRAY_H__

#include <stddef.h> /* size_t wchar_t */
#include <stdio.h> /* FILE */

/* Convenience structure (not used here directly, but is related
 * functionally). */
typedef struct strlist_t
{
	int nitems;   /* Number of items in the list. */
	char **items; /* The list itself. */
}
strlist_t;

/* Type of callback function to get notification on reading another portion of
 * data. */
typedef void (*progress_cb)(const void *arg);

/* Adds copy of a string to a string array.  Input pointer can be NULL.  Returns
 * new length of the array, which is unchanged on allocation failure. */
int add_to_string_array(char ***array, int len, const char item[]);

/* Puts pointer into string array without making a copy.  Reallocates *array.
 * item can be NULL.  Returns new size of the array, which can be equal to len
 * on reallocation failure. */
int put_into_string_array(char **array[], int len, char item[]);

void remove_from_string_array(char **array, size_t len, int pos);

/* Checks whether item is in the array.  Always uses case sensitive comparison.
 * Returns non-zero on successful search, otherwise zero is returned. */
int is_in_string_array(char *array[], size_t len, const char item[]);

/* Checks whether item is in the array.  Always uses case insensitive
 * comparison.  Returns non-zero on successful search, otherwise zero is
 * returned. */
int is_in_string_array_case(char *array[], size_t len, const char item[]);

/* Checks whether item is in the array.  Uses case sensitive or case insensitive
 * comparison depending on OS case sensitivity.  Returns non-zero on successful
 * search, otherwise zero is returned. */
int is_in_string_array_os(char *array[], size_t len, const char item[]);

char ** copy_string_array(char **array, size_t len);

/* Returns position of the item in the array, -1 if no match found. */
int string_array_pos(char *array[], size_t len, const char item[]);

/* Returns position of the item in the array, -1 if no match found. */
int string_array_pos_case(char *array[], size_t len, const char item[]);

/* Checks if two string lists are equal.  Returns non-zero if so, otherwise zero
 * is returned. */
int string_array_equal(char *first[], size_t first_len, char *second[],
		size_t second_len);

/* Frees memory of all array items and from the array itself.  Does nothing for
 * NULL arrays. */
void free_string_array(char *array[], size_t len);

/* Frees memory of all array items, but not from the array itself. */
void free_strings(char *array[], size_t len);

/* Counts number of elements of a null terminated array of strings.  Returns the
 * count. */
int count_strings(char *array[]);

/* Reads file specified by filepath into an array of strings.  Returns non-NULL
 * on success, otherwise NULL is returned, *nlines is untouched and errno
 * contains error code.  For empty file non-NULL will be returned, but *nlines
 * will be zero. */
char ** read_file_of_lines(const char filepath[], int *nlines);

/* Reads content of the file stream (required to be seekable) f into array of
 * strings.  Returns NULL for an empty file stream. */
char ** read_file_lines(FILE *f, int *nlines);

/* Reads content of the stream (not required to be seekable) f into array of
 * strings.  The flag enables heuristic according to which if stream contents
 * includes null character, it's taken as a separator instead of regular newline
 * characters.  cb can be NULL.  Returns NULL for an empty file stream. */
char ** read_stream_lines(FILE *f, int *nlines, int null_sep_heuristic,
		progress_cb cb, const void *arg);

/* Reads content of the fp stream that doesn't support seek operation (e.g. it
 * points to a pipe) until end-of-file into null terminated string.  cb can be
 * NULL.  Returns string of length *read to be freed by caller on success,
 * otherwise NULL is returned. */
char * read_nonseekable_stream(FILE *fp, size_t *read, progress_cb cb,
		const void *arg);

/* Converts text of length text_len into an array of strings.  Returns non-NULL
 * on success, otherwise NULL is returned, *nlines is untouched.  For empty file
 * non-NULL will be returned, but *nlines will be zero. */
char ** break_into_lines(char text[], size_t text_len, int *nlines,
		int null_sep);

/* Overwrites file specified by filepath with lines.  Returns zero on success,
 * otherwise non-zero is returned and errno contains error code. */
int write_file_of_lines(const char filepath[], char *strs[], size_t nstrs);

#endif /* VIFM__UTILS__STRING_ARRAY_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
