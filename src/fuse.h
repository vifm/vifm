/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#ifndef __FUSE_H__
#define __FUSE_H__

#include "ui.h"

/* Wont mount same file twice */
void fuse_try_mount(FileView *view, const char *program);
/* Unmounts all FUSE mounded filesystems. */
void unmount_fuse(void);
/* Returns non-zero on successful unmount. */
int try_updir_from_fuse_mount(const char *path, FileView *view);
int in_mounted_dir(const char *path);
/*
 * Return value:
 *   -1 error occurred.
 *   0  not mount point.
 *   1  left FUSE mount directory.
 */
int try_unmount_fuse(FileView *view);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
