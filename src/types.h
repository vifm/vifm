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

/* Forward declaration of dirent structure to omit inclusion of <dirent.h>. */
struct dirent;

/* List of types of file system objects. */
typedef enum
{
	FT_LINK,      /* Symbolic link. */
	FT_DIR,       /* Directory. */
	FT_CHAR_DEV,  /* Character device file. */
	FT_BLOCK_DEV, /* Block device file. */
#ifndef _WIN32
	FT_SOCK,      /* Socket file. */
#endif
	FT_EXEC,      /* Executable file. */
	FT_REG,       /* Regular (non-executable) file. */
	FT_FIFO,      /* Named pipe. */
	FT_UNK,       /* Unknown object (shouldn't occur in file list). */
	FT_COUNT      /* Number of types. */
}
FileType;

/* Provides string representation for FileType enumeration item.  Returns
 * pointer to a statically allocated file type string. */
const char * get_type_str(FileType type);

/* Converts file mode to type from FileType enumeration.  Returns item of the
 * enumeration. */
FileType get_type_from_mode(mode_t mode);

#ifndef _WIN32

/* Converts dirent structure returned by call to readdir() to type from FileType
 * enumeration.  Returns item of the enumeration. */
FileType type_from_dir_entry(const struct dirent *d, const char path[]);

#endif

#endif /* VIFM__TYPES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
