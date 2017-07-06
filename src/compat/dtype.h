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

#ifndef VIFM__COMPAT__DTYPE_H__
#define VIFM__COMPAT__DTYPE_H__

#if !defined(HAVE_STRUCT_DIRENT_D_TYPE) || !HAVE_STRUCT_DIRENT_D_TYPE

/* Declaration of entry types that match those defined by POSIX. */
enum
{
	DT_BLK,
	DT_CHR,
	DT_DIR,
	DT_FIFO,
	DT_REG,
#ifndef _WIN32
	DT_LNK,
	DT_SOCK,
#endif
	DT_UNKNOWN
};

#endif

struct dirent;

/* Retrieves entry type.  Either directly from dirent or by running lstat() on
 * the path.  Returns the type. */
unsigned char get_dirent_type(const struct dirent *dentry, const char path[]);

#endif // VIFM__COMPAT__DTYPE_H__

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
