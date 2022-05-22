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

#ifndef VIFM__IO__PRIVATE__TRAVERSER_H__
#define VIFM__IO__PRIVATE__TRAVERSER_H__

#include "../ioc.h"

/* Reason why file system traverse visitor is called. */
typedef enum
{
	VA_DIR_ENTER, /* After opening a directory. */
	VA_FILE,      /* For a file. */
	VA_DIR_LEAVE, /* After closing a directory. */
}
VisitAction;

/* Result of calling file system traverse visitor. */
typedef enum
{
	VR_OK,             /* Everything is OK, continue traversal. */
	VR_ERROR,          /* Unrecoverable error, abort traversal. */
	VR_SKIP_DIR_LEAVE, /* Valid only for VA_DIR_ENTER.  Prevents VA_DIR_LEAVE. */
	VR_CANCELLED,      /* Operation was cancelled by the user. */
}
VisitResult;

/* Generic handler for file system traversing algorithm.  Must return 0 on
 * success, otherwise directory traverse will be stopped. */
typedef VisitResult (*subtree_visitor)(const char full_path[],
		VisitAction action, void *param);

/* A generic recursive file system traversing entry point.  Returns zero on
 * success, otherwise non-zero is returned. */
IoRes traverse(const char path[], subtree_visitor visitor, void *param);

#endif /* VIFM__IO__PRIVATE__TRAVERSER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
