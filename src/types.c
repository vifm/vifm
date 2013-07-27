/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2012 xaizek.
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

#include "types.h"

#include <sys/stat.h> /* S_*() */
#include <sys/types.h> /* mode_t */
#include <dirent.h> /* struct dirent, DT_* */

#include <assert.h> /* assert() */

#include "utils/macros.h"
#include "utils/utils.h"

const char *
get_mode_str(mode_t mode)
{
	return get_type_str(get_type_from_mode(mode));
}

const char *
get_type_str(FileType type)
{
	static const char *str[] = {
		[LINK]             = "link",
		[DIRECTORY]        = "dir",
		[CHARACTER_DEVICE] = "char",
		[BLOCK_DEVICE]     = "block",
#ifndef _WIN32
		[SOCKET]           = "sock",
#endif
		[EXECUTABLE]       = "exe",
		[REGULAR]          = "reg",
		[FIFO]             = "fifo",
		[UNKNOWN]          = "?",
	};

	assert(type >= 0 && "FileType numeration value invalid");
	assert(type < ARRAY_LEN(str) && "FileType numeration value invalid");
	return str[type];
}

FileType
get_type_from_mode(mode_t mode)
{
	switch(mode & S_IFMT)
	{
#ifndef _WIN32
		case S_IFLNK:
			return LINK;
#endif
		case S_IFDIR:
			return DIRECTORY;
		case S_IFCHR:
			return CHARACTER_DEVICE;
		case S_IFBLK:
			return BLOCK_DEVICE;
#ifndef _WIN32
		case S_IFSOCK:
			return SOCKET;
#endif
		case S_IFREG:
#ifndef _WIN32
			return S_ISEXE(mode) ? EXECUTABLE : REGULAR;
#else
			return REGULAR;
#endif
		case S_IFIFO:
			return FIFO;

		default:
			return UNKNOWN;
	}
}

#ifndef _WIN32
FileType
type_from_dir_entry(const struct dirent *d)
{
	switch(d->d_type)
	{
		case DT_CHR:
			return CHARACTER_DEVICE;
		case DT_BLK:
			return BLOCK_DEVICE;
		case DT_DIR:
			return DIRECTORY;
		case DT_LNK:
			return LINK;
		case DT_REG:
			return REGULAR;
		case DT_SOCK:
			return SOCKET;
		case DT_FIFO:
			return FIFO;

		default:
			return UNKNOWN;
	}
}
#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
