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

#ifndef VIFM__FOPS_CPMV_H__
#define VIFM__FOPS_CPMV_H__

#include "ui/ui.h"

/* Type of copy/move-like operation. */
typedef enum
{
	CMLO_COPY,     /* Copy file. */
	CMLO_MOVE,     /* Move file. */
	CMLO_LINK_REL, /* Make relative symbolic link. */
	CMLO_LINK_ABS, /* Make absolute symbolic link. */
}
CopyMoveLikeOp;

/* Performs copy/moves-like operation on marked files.  Returns new value for
 * save_msg flag. */
int fops_cpmv(FileView *view, char *list[], int nlines, CopyMoveLikeOp op,
		int force);

/* Replaces file specified by dst with a copy of the current file of the
 * view. */
void fops_replace(FileView *view, const char dst[], int force);

/* Copies or moves marked files to the other view in background.  Returns new
 * value for save_msg flag. */
int fops_cpmv_bg(FileView *view, char *list[], int nlines, int move, int force);

#endif /* VIFM__FOPS_CPMV_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
