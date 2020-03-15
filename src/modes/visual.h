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

struct view_t;

/* Generic submodes of the visual mode. */
typedef enum
{
	VS_NORMAL,  /* With initial clearing of selection. */
	VS_RESTORE, /* With restoring of previous selection if available. */
	VS_AMEND,   /* Amending existing selection. */
}
VisualSubmodes;

/* Initializes visual mode. */
void modvis_init(void);

/* Starts visual mode in a given submode. */
void modvis_enter(VisualSubmodes sub_mode);

/* Leaves visual mode in various ways. */
void modvis_leave(int save_msg, int goto_top, int clear_selection);

/* Should be used to ask visual mode to redraw file list correctly.
 * Intended to be used after setting list position from side. */
void modvis_update(void);

/* Kind of callback to allow starting searches from the module and rely on other
 * modules.  Returns new value for status bar message flag, but when
 * print_errors isn't requested can return -1 to indicate issues with the
 * pattern. */
int modvis_find(struct view_t *view, const char pattern[], int backward,
		int print_errors);

/* Formats concise description of current visual mode state.  Returns pointer
 * to a statically allocated buffer. */
const char * modvis_describe(void);

#endif /* VIFM__MODES__VISUAL_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
