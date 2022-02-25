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

struct view_t;

/* Type of copy/move-like operation. */
typedef enum
{
	CMLO_COPY,     /* Copy file. */
	CMLO_MOVE,     /* Move file. */
	CMLO_LINK_REL, /* Make relative symbolic link. */
	CMLO_LINK_ABS, /* Make absolute symbolic link. */
}
CopyMoveLikeOp;

/* Fine tuning of fops_cpmv() behaviour. */
typedef enum
{
	CMLF_NONE  = 0x00, /* None of the other options. */
	CMLF_FORCE = 0x01, /* Remove destination if it already exists. */
	CMLF_SKIP  = 0x02, /* Skip paths that already exist at destination. */
}
CopyMoveLikeFlags;

/* Performs copy/moves-like operation on marked files.  Flags is a combination
 * of CMLF_* values.  Returns new value for save_msg flag. */
int fops_cpmv(struct view_t *view, char *list[], int nlines, CopyMoveLikeOp op,
		int flags);

/* Replaces file specified by dst with a copy of the current file of the
 * view. */
void fops_replace(struct view_t *view, const char dst[], int force);

/* Copies or moves marked files to the other view in background.  Flags is a
 * combination of CMLF_* values.  Returns new value for save_msg flag. */
int fops_cpmv_bg(struct view_t *view, char *list[], int nlines, int move,
		int flags);

#endif /* VIFM__FOPS_CPMV_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
