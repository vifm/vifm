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

#ifndef __UTF8_H__
#define __UTF8_H__

#include <stddef.h>

size_t get_char_width(const char* string);
size_t get_normal_utf8_string_length(const char *string);
size_t get_real_string_width(char *string, size_t max_len);
size_t get_normal_utf8_string_widthn(const char *string, size_t max);
size_t get_normal_utf8_string_width(const char *string);
size_t get_utf8_string_length(const char *string);
size_t get_utf8_overhead(const char *string);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
