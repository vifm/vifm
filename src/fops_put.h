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

#ifndef VIFM__FOPS_PUT_H__
#define VIFM__FOPS_PUT_H__

#include "utils/test_helpers.h"

struct view_t;

/* Puts files from specified register into current directory.  at specifies
 * index of entry to be used to obtain destination path, -1 means current
 * position.  Returns new value for save_msg flag. */
int fops_put(struct view_t *view, int at, int reg_name, int move);

/* Starts background task that puts files from specified register into current
 * directory.  at specifies index of entry to be used to obtain destination
 * path, -1 means current position.  Returns new value for save_msg flag. */
int fops_put_bg(struct view_t *view, int at, int reg_name, int move);

/* Like fops_put(), but makes absolute or relative symbolic links to files.
 * Returns new value for save_msg flag. */
int fops_put_links(struct view_t *view, int reg_name, int relative);

#ifdef TEST
#include "ops.h"
#endif
TSTATIC_DEFS(
	int merge_dirs(const char src[], const char dst[], ops_t *ops);
)

#endif /* VIFM__FOPS_PUT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
