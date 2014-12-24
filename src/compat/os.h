/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__COMPAT__OS_H__
#define VIFM__COMPAT__OS_H__

#ifndef _WIN32

#include <sys/stat.h> /* mkdir() */
#include <unistd.h> /* access() rename() */

#include <stdlib.h> /* system() */

#define os_access access
#define os_chdir chdir
#define os_mkdir mkdir
#define os_rename rename
#define os_system system

#else

#include <fcntl.h> /* *_OK */

int os_access(const char pathname[], int mode);

int os_chdir(const char path[]);

int os_rename(const char oldpath[], const char newpath[]);

int os_mkdir(const char pathname[], int mode);

int os_system(const char command[]);

#endif

#endif /* VIFM__COMPAT__OS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
