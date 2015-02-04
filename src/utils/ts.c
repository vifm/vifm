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

#include "ts.h"

#include <sys/stat.h> /* stat */

#include <string.h> /* memcmp() memcpy() */

#include "../compat/os.h"

int
ts_get_file_mtime(const char path[], timestamp_t *timestamp)
{
	struct stat s;

	if(os_stat(path, &s) != 0)
	{
		return 1;
	}

#ifdef HAVE_STRUCT_STAT_ST_MTIM
	memcpy(&timestamp->ts, &s.st_mtim, sizeof(s.st_mtim));
#else
	memcpy(&timestamp->ts, &s.st_mtime, sizeof(s.st_mtime));
#endif
	timestamp->dev = s.st_dev;
	timestamp->inode = s.st_ino;

	return 0;
}

int
ts_equal(const timestamp_t *a, const timestamp_t *b)
{
	return memcmp(a, b, sizeof(*a)) == 0;
}

void
ts_assign(timestamp_t *lhs, const timestamp_t *rhs)
{
	memcpy(lhs, rhs, sizeof(*rhs));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
