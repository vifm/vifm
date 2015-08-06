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

#include "filemon.h"

#include <sys/stat.h> /* stat */

#include <string.h> /* memcmp() memcpy() */

#include "../compat/os.h"

int
filemon_from_file(const char path[], filemon_t *timestamp)
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
filemon_equal(const filemon_t *a, const filemon_t *b)
{
	return memcmp(&a->ts, &b->ts, sizeof(a->ts)) == 0
	    && a->dev == b->dev
	    && a->inode == b->inode;
}

void
filemon_assign(filemon_t *lhs, const filemon_t *rhs)
{
	memcpy(lhs, rhs, sizeof(*rhs));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
