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

#ifndef VIFM__OPS_H__
#define VIFM__OPS_H__

typedef enum
{
	OP_NONE,
	OP_USR,
	OP_REMOVE,   /* rm -rf */
	OP_REMOVESL, /* cl */
	OP_COPY,     /* copy and clone */
	OP_COPYF,    /* copy with file overwrite */
	OP_MOVE,     /* move, rename and substitute */
	OP_MOVEF,    /* move with file overwrite */
	OP_MOVETMP1, /* multiple files rename */
	OP_MOVETMP2, /* multiple files rename */
	OP_MOVETMP3, /* multiple files rename */
	OP_MOVETMP4, /* multiple files rename */
	OP_CHOWN,
	OP_CHGRP,
#ifndef _WIN32
	OP_CHMOD,
	OP_CHMODR,
#else
	OP_ADDATTR,
	OP_SUBATTR,
#endif
	OP_SYMLINK,
	OP_SYMLINK2,
	OP_MKDIR,
	OP_RMDIR,
	OP_MKFILE,
	OP_COUNT
}OPS;

int perform_operation(OPS op, void *data, const char *src, const char *dst);

#endif /* VIFM__OPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
