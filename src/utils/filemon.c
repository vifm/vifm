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

#include <assert.h> /* assert() */
#include <string.h> /* memcmp() memcpy() */

#include "../compat/os.h"

void
filemon_reset(filemon_t *timestamp)
{
	timestamp->type = FMT_UNINITIALIZED;
}

int
filemon_is_set(const filemon_t *timestamp)
{
	return (timestamp->type != FMT_UNINITIALIZED);
}

int
filemon_from_file(const char path[], FileMonType type, filemon_t *timestamp)
{
	assert(type != FMT_UNINITIALIZED && "Wrong type for a file monitor.");

	struct stat s;

	if(os_stat(path, &s) != 0)
	{
		filemon_reset(timestamp);
		return 1;
	}

#ifdef HAVE_STRUCT_STAT_ST_MTIM
	if(type == FMT_MODIFIED)
	{
		memcpy(&timestamp->ts, &s.st_mtim, sizeof(s.st_mtim));
	}
	else
	{
		memcpy(&timestamp->ts, &s.st_ctim, sizeof(s.st_ctim));
	}
#else
	if(type == FMT_MODIFIED)
	{
		memcpy(&timestamp->ts, &s.st_mtime, sizeof(s.st_mtime));
	}
	else
	{
		memcpy(&timestamp->ts, &s.st_ctime, sizeof(s.st_ctime));
	}
#endif
	timestamp->dev = s.st_dev;
	timestamp->inode = s.st_ino;
	timestamp->type = type;

	return 0;
}

int
filemon_equal(const filemon_t *a, const filemon_t *b)
{
	if(a->type == FMT_UNINITIALIZED || b->type == FMT_UNINITIALIZED)
	{
		return 0;
	}

	return a->type == b->type
	    && memcmp(&a->ts, &b->ts, sizeof(a->ts)) == 0
	    && a->dev == b->dev
	    && a->inode == b->inode;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
