/* vifm
 * Copyright (C) 2023 xaizek.
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

#ifndef VIFM__UTILS__MEM_H__
#define VIFM__UTILS__MEM_H__

#include <stddef.h> /* size_t */

/* Rotates array's elements to the right once: 0 -> 1, ..., (n - 1) -> 0. */
void mem_ror(void *ptr, size_t count, size_t item_len);

/* Shifts array's elements to the left by n positions (offset):
 *   0 <- (n), 1 <- (n + 1), ... */
void mem_shl(void *ptr, size_t count, size_t item_len, int offset);

/* Shifts array's elements to the right by n positions (offset):
 *   0 -> 1, 1 -> 2, ... */
void mem_shr(void *ptr, size_t count, size_t item_len, int offset);

#endif /* VIFM__UTILS__MEM_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
