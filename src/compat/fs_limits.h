/* vifm
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

#ifndef VIFM__COMPAT__FS_LIMITS_H__
#define VIFM__COMPAT__FS_LIMITS_H__

/* Define NAME_MAX and PATH_MAX constants in a most portable way. */

#include <limits.h> /* NAME_MAX PATH_MAX */

/* For Windows. */
#if !defined(NAME_MAX) && defined(_WIN32)
#include <stdio.h> /* FILENAME_MAX */
#define NAME_MAX (FILENAME_MAX)
#endif

/* For Solaris. */
#ifndef NAME_MAX
#include <dirent.h>
#ifndef MAXNAMLEN
#define MAXNAMLEN FILENAME_MAX
#endif
#define NAME_MAX MAXNAMLEN
#endif

/* For GNU Hurd (which doesn't have path length limit at all). */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#endif /* VIFM__COMPAT__FS_LIMITS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
