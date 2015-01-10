/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__UTILS__TS_H__
#define VIFM__UTILS__TS_H__

#include <time.h> /* time_t timespec */

/* Various time stamp service functions. */

/* Data type of timestamp storage. */
#ifdef HAVE_STRUCT_STAT_ST_MTIM
typedef struct timespec timestamp_t;
#else
typedef time_t timestamp_t;
#endif

/* Gets last file modification timestamp.  Returns zero on success, otherwise
 * non-zero is returned. */
int ts_get_file_mtime(const char path[], timestamp_t *timestamp);

/* Checks whether two timestamps are equal.  Returns non-zero if so, otherwise
 * zero is returned. */
int ts_equal(const timestamp_t *a, const timestamp_t *b);

/* Assigns value of the *rhs to *lhs. */
void ts_assign(timestamp_t *lhs, const timestamp_t *rhs);

#endif /* VIFM__UTILS__TS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
