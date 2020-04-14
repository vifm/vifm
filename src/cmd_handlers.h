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

/* This file exports some information about command handlers that is needed by
 * cmd_core.c and cmd_completion.c. */

#include <stddef.h> /* size_t */

#include "engine/cmds.h"
#include "utils/test_helpers.h"

/* Commands without completion, but for which we need to have an id. */
enum
{
	COM_CDS = NO_COMPLETION_BOUNDARY,
	COM_CMAP,
	COM_CNOREMAP,
	COM_COMMAND,
	COM_DMAP,
	COM_DNOREMAP,
	COM_ELSE_STMT,
	COM_ENDIF_STMT,
	COM_FILETYPE,
	COM_FILEVIEWER,
	COM_FILEXTYPE,
	COM_FILTER,
	COM_MAP,
	COM_MMAP,
	COM_MNOREMAP,
	COM_NMAP,
	COM_NNOREMAP,
	COM_NOREMAP,
	COM_NORMAL,
	COM_QMAP,
	COM_QNOREMAP,
	COM_SUBSTITUTE,
	COM_TR,
	COM_VMAP,
	COM_VNOREMAP,
};

/* Specification of builtin commands. */
extern const cmd_add_t cmds_list[];
/* Number of elements in cmds_list. */
extern const size_t cmds_list_size;

TSTATIC_DEFS(
	void cmds_drop_state(void);
)

#endif /* VIFM__CMD_HANDLERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
