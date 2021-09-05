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

#ifndef VIFM__UTILS__FILEMON_H__
#define VIFM__UTILS__FILEMON_H__

#include <sys/types.h> /* dev_t ino_t */

#include <time.h> /* time_t timespec */

/* Various time stamp service functions. */

/* Type of file monitor. */
typedef enum
{
	FMT_UNINITIALIZED, /* Compares false with any monitor (even with itself). */
	FMT_MODIFIED,      /* File modification time monitor. */
	FMT_CHANGED        /* File change time monitor. */
}
FileMonType;

/* Storage for file monitoring information. */
typedef struct
{
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	struct timespec ts;
#else
	time_t ts;
#endif

	/* The following members are to react on file system change, during which
	 * timestamp can remain the same. */
	dev_t dev;
	ino_t inode;

	FileMonType type; /* What exactly is being monitored. */
}
filemon_t;

/* Puts file monitor in an uninitialized state in which it's equal to no other
 * instance. */
void filemon_reset(filemon_t *timestamp);

/* Checks whether this monitor is initialized.  Returns non-zero if so,
 * otherwise zero is returned. */
int filemon_is_set(const filemon_t *timestamp);

/* Sets file monitor from a file.  Returns zero on success, otherwise non-zero
 * is returned and *timestamp is reset. */
int filemon_from_file(const char path[], FileMonType type,
		filemon_t *timestamp);

/* Checks whether two timestamps are equal.  Returns non-zero if so, otherwise
 * zero is returned. */
int filemon_equal(const filemon_t *a, const filemon_t *b);

#endif /* VIFM__UTILS__FILEMON_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
