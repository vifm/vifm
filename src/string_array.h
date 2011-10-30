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

#ifndef __STRING_ARRAY_H__
#define __STRING_ARRAY_H__

#include <stdio.h>

/* Returns new size */
int add_to_string_array(char ***array, int len, int count, ...);
void remove_from_string_array(char **array, size_t len, int pos);
int is_in_string_array(char **array, size_t len, const char *key);
int is_in_string_array_case(char **array, size_t len, const char *key);
char ** copy_string_array(char **array, size_t len);
int string_array_pos(char **array, size_t len, const char *key);
int string_array_pos_case(char **array, size_t len, const char *key);
void free_string_array(char **array, size_t len);
void free_wstring_array(wchar_t **array, size_t len);
char ** read_file_lines(FILE *f, int *nlines);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
