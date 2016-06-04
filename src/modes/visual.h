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

#ifndef VIFM__MODES__VISUAL_H__
#define VIFM__MODES__VISUAL_H__

#include "../ui/ui.h"

/* Generic submodes of the visual mode. */
typedef enum
{
	VS_NORMAL,  /* With initial clearing of selection. */
	VS_RESTORE, /* With restoring of previous selection if available. */
	VS_AMEND,   /* Amending existing selection. */
}
VisualSubmodes;

/* Initializes visual mode. */
void init_visual_mode(void);

/* Starts visual mode in a given submode. */
void enter_visual_mode(VisualSubmodes sub_mode);

void leave_visual_mode(int save_msg, int goto_top, int clean_selection);

/* Should be used to ask visual mode to redraw file list correctly.
 * Intended to be used after setting list position from side. */
void update_visual_mode(void);

int find_vpattern(FileView *view, const char *pattern, int backward,
		int interactive);

/* Formats concise description of current visual mode state.  Returns pointer
 * to a statically allocated buffer. */
const char * describe_visual_mode(void);

#endif /* VIFM__MODES__VISUAL_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
