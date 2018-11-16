/* vifm
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__INT__FILE_MAGIC_H__
#define VIFM__INT__FILE_MAGIC_H__

#include "../filetype.h"

/* Retrieves mime type of the file specified by its path.  The resolve_symlinks
 * argument controls whether mime-type of the link should be that of its target.
 * Returns pointer to a statically allocated buffer. */
const char * get_mimetype(const char file[], int resolve_symlinks);

/* Retrieves system-wide desktop file associations.  Caller shouldn't free
 * anything. */
assoc_records_t get_magic_handlers(const char file[]);

#endif /* VIFM__INT__FILE_MAGIC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
