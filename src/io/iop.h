/* vifm
 * Copyright (C) 2013 xaizek.
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

#ifndef VIFM__IO__IOP_H__
#define VIFM__IO__IOP_H__

#include "ioc.h"

/* iop - I/O primitive - Input/Output primitive */

/* All functions return status of the operation. */

/* Creates file.  Expects path in arg1.  Fails if one already exists. */
IoRes iop_mkfile(io_args_t *args);

/* Creates directory or hierarchical directory structure (if parent is true).
 * Expects path in arg1, process_parents in arg2 and mode in arg3.  When
 * process_parents is false, fails if destination already exists. */
IoRes iop_mkdir(io_args_t *args);

/* Deletes file.  Expects path in arg1. */
IoRes iop_rmfile(io_args_t *args);

/* Delete empty directory.  Expects path in arg1.  Fails if directory is not
 * empty. */
IoRes iop_rmdir(io_args_t *args);

/* Copies file.  Expects source in arg1, destination in arg2 and crs in arg3. */
IoRes iop_cp(io_args_t *args);

/* Change owner of file/directory.  Expects path in arg1 and uid in arg3. */
IoRes iop_chown(io_args_t *args);

/* Change group of file/directory.  Expects path in arg1 and gid in arg3. */
IoRes iop_chgrp(io_args_t *args);

/* Change permissions of file/directory.  Expects path in arg1 and mode in
 * arg3. */
IoRes iop_chmod(io_args_t *args);

/* Create symbolic link or change its target.  Expects path in arg1, target in
 * arg2 and crs in arg3.  Fails to overwrite if path exists and not a symbolic
 * link. */
IoRes iop_ln(io_args_t *args);

#endif /* VIFM__IO__IOP_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
