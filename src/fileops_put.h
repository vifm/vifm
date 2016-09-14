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

#ifndef VIFM__FILEOPS_PUT_H__
#define VIFM__FILEOPS_PUT_H__

#include "ui/ui.h"
#include "utils/test_helpers.h"

/* Puts files from specified register into current directory.  at specifies
 * index of entry to be used to obtain destination path, -1 means current
 * position.  Returns new value for save_msg flag. */
int put_files(FileView *view, int at, int reg_name, int move);

/* Like put_files(), but makes absolute or relative symbolic links to files.
 * Returns new value for save_msg flag. */
int put_links(FileView *view, int reg_name, int relative);

#ifdef TEST
#include "ops.h"
#endif
TSTATIC_DEFS(
	int merge_dirs(const char src[], const char dst[], ops_t *ops);
)

#endif /* VIFM__FILEOPS_PUT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
