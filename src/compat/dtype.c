/* vifm
 * Copyright (C) 2016 xaizek.
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

#include "dtype.h"
#include <dirent.h> /* dirent */

#if !defined(HAVE_STRUCT_DIRENT_D_TYPE) || !HAVE_STRUCT_DIRENT_D_TYPE

#include "os.h"

unsigned char
get_dirent_type(const struct dirent *dentry, const char path[])
{
	struct stat st;

	if(os_lstat(path, &st) != 0 || st.st_ino == 0)
	{
		return DT_UNKNOWN;
	}

	switch(st.st_mode & S_IFMT)
	{
		case S_IFBLK:  return DT_BLK;
		case S_IFCHR:  return DT_CHR;
		case S_IFDIR:  return DT_DIR;
		case S_IFIFO:  return DT_FIFO;
		case S_IFREG:  return DT_REG;
#ifndef _WIN32
		case S_IFLNK:  return DT_LNK;
		case S_IFSOCK: return DT_SOCK;
#endif

		default: return DT_UNKNOWN;
	}
}

#else

unsigned char
get_dirent_type(const struct dirent *dentry, const char path[])
{
	return dentry->d_type;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
