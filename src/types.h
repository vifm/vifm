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

#ifndef VIFM__TYPES_H__
#define VIFM__TYPES_H__

#include <sys/types.h> /* mode_t */
#include <dirent.h> /* struct dirent */

typedef enum
{
	LINK,
	DIRECTORY,
	CHARACTER_DEVICE,
	BLOCK_DEVICE,
#ifndef _WIN32
	SOCKET,
#endif
	EXECUTABLE,
	REGULAR,
	FIFO,
	UNKNOWN,
	FILE_TYPE_COUNT
}FileType;

/* Returns a pointer to a statically allocated file type string for given file
 * mode. */
const char * get_mode_str(mode_t mode);
/* Returns a pointer to a statically allocated file type string for given file
 * type. */
const char * get_type_str(FileType type);
/* Returns file type from file mode. */
FileType get_type_from_mode(mode_t mode);
#ifndef _WIN32
/* Returns file type from dirent structure returned by call to readdir(). */
FileType type_from_dir_entry(const struct dirent *d);
#endif

#endif /* VIFM__TYPES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
