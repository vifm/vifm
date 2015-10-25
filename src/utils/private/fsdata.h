/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__UTILS__PRIVATE__FSDATA_H__
#define VIFM__UTILS__PRIVATE__FSDATA_H__

#include "../fsdata.h"

/* Type of cleanup function that is called whenever node data is dismissed.
 * Data is pointer to data of the node. */
typedef void (*fsd_cleanup_func)(void *data);

/* Sets custom node cleanup function for the particular fsdata instance. */
void fsdata_set_cleanup(fsdata_t *fsd, fsd_cleanup_func cleanup);

#endif /* VIFM__UTILS__PRIVATE__FSDATA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
