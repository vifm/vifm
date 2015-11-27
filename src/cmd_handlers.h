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

#ifndef VIFM__CMD_HANDLERS_H__
#define VIFM__CMD_HANDLERS_H__

#include <stddef.h> /* size_t */

#include "engine/cmds.h"

/* Commands without completion, but for which we need to have an id. */
enum
{
	COM_FILTER = NO_COMPLETION_BOUNDARY,
	COM_SUBSTITUTE,
	COM_TR,
	COM_ELSE_STMT,
	COM_ENDIF_STMT,
	COM_CMAP,
	COM_CNOREMAP,
	COM_COMMAND,
	COM_FILETYPE,
	COM_FILEVIEWER,
	COM_FILEXTYPE,
	COM_MAP,
	COM_MMAP,
	COM_MNOREMAP,
	COM_NMAP,
	COM_NNOREMAP,
	COM_NORMAL,
	COM_QMAP,
	COM_QNOREMAP,
	COM_VMAP,
	COM_VNOREMAP,
	COM_NOREMAP,
};

/* List of command handlers. */
extern const cmd_add_t cmds_list[];
/* Number of elements in cmds_list. */
extern const size_t cmds_list_size;

#endif /* VIFM__CMD_HANDLERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
