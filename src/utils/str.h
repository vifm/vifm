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

/* Various string functions. */

void chomp(char *text);
wchar_t * to_wide(const char *s);
wchar_t * my_wcsdup(const wchar_t *ws);
int starts_with(const char *str, const char *prefix);
int ends_with(const char *str, const char *suffix);
char * to_multibyte(const wchar_t *s);
void strtolower(char *s);
void break_at(char *str, char c);
void break_atr(char *str, char c);
char * skip_non_whitespace(const char *str);
char * skip_whitespace(const char *str);
/* Returns pointer to first character after last c occurrence in str or str. */
char * after_last(const char *str, char c);
/* Replaces *str with a copy of with string. */
void replace_string(char **str, const char *with);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
