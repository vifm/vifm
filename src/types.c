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

#include "compat/dtype.h"
#include "utils/macros.h"
#include "utils/utils.h"

const char *
get_type_str(FileType type)
{
	static const char *str[] = {
		[FT_LINK]      = "link",
		[FT_DIR]       = "dir",
		[FT_CHAR_DEV]  = "char",
		[FT_BLOCK_DEV] = "block",
#ifndef _WIN32
		[FT_SOCK]      = "sock",
#endif
		[FT_EXEC]      = "exe",
		[FT_REG]       = "reg",
		[FT_FIFO]      = "fifo",
		[FT_UNK]       = "?",
	};
	ARRAY_GUARD(str, FT_COUNT);

	assert(type >= 0 && "FileType numeration value invalid");
	assert(type < ARRAY_LEN(str) && "FileType numeration value invalid");
	return str[type];
}

FileType
get_type_from_mode(mode_t mode)
{
	switch(mode & S_IFMT)
	{
		case S_IFDIR:  return FT_DIR;
		case S_IFCHR:  return FT_CHAR_DEV;
		case S_IFBLK:  return FT_BLOCK_DEV;
		case S_IFIFO:  return FT_FIFO;
#ifndef _WIN32
		case S_IFLNK:  return FT_LINK;
		case S_IFSOCK: return FT_SOCK;
		case S_IFREG:  return S_ISEXE(mode) ? FT_EXEC : FT_REG;
#else
		case S_IFREG:  return FT_REG;
#endif

		default:
			return FT_UNK;
	}
}

#ifndef _WIN32

FileType
type_from_dir_entry(const struct dirent *d, const char path[])
{
	switch(get_dirent_type(d, path))
	{
		case DT_LNK:  return FT_LINK;
		case DT_DIR:  return FT_DIR;
		case DT_CHR:  return FT_CHAR_DEV;
		case DT_BLK:  return FT_BLOCK_DEV;
		case DT_SOCK: return FT_SOCK;
		case DT_REG:  return FT_REG;
		case DT_FIFO: return FT_FIFO;

		default:
			return FT_UNK;
	}
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
