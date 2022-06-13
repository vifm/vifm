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

#ifndef VIFM__MODES__VIEW_H__
#define VIFM__MODES__VIEW_H__

#include "../utils/test_helpers.h"
#include "../macros.h"

struct view_t;

/* Holds state of a single view mode window. */
typedef struct modview_info_t modview_info_t;

/* Initializes view mode. */
void modview_init(void);

/* Enters view mode when possible.  View is the expected output area. */
void modview_enter(struct view_t *view, int explore);

/* Leaves view mode. */
void modview_leave(void);

/* Quits view from explore mode.  Assumes the view is not an active one.
 * Automatically redraws view. */
void modview_quit_exploring(struct view_t *view);

/* Activates the mode if current pane is in explore mode. */
void modview_try_activate(void);

/* Performs pre main loop actions for the view mode, which is assumed to be
 * activated. */
void modview_pre(void);

/* Performs post main loop actions for the view mode, which is assumed to be
 * activated. */
void modview_post(void);

/* Displays view mode specific position information.  Assumes that view mode is
 * active. */
void modview_ruler_update(void);

/* Redraws view mode. */
void modview_redraw(void);

/* Performs search for the pattern in specified direction inside active view.
 * Returns positive number zero if there is something on the status bar to
 * preserve, otherwise zero is returned. */
int modview_find(const char pattern[], int backward);

/* Callback-like routine that handles swapping of panes. */
void modview_panes_swapped(void);

/* Checks whether contents of either view should be updated. */
void modview_check_for_updates(void);

/* Hides graphics that needs special care (doesn't disappear on UI redraw). */
void modview_hide_graphics(void);

/* Detached views.  These are the views which were either created in detached
 * state or were detached from, but their state (position, etc.) is still
 * maintained. */

/* Creates detached view in the view using output of passed command as a
 * source, but doesn't enter the view mode.  The command is assumed to not have
 * any unexpanded macros. */
void modview_detached_make(struct view_t *view, const char cmd[],
		MacroFlags flags);

/* Tries to draw an detached view mode and updates internal state if needed.
 * Returns non-zero on success, otherwise zero is returned. */
int modview_detached_draw(void);

/* Retrieves viewer command associated with preview created with
 * modview_detached_make().  Returns pointer to the viewer command or NULL, if
 * there is none. */
const char * modview_detached_get_viewer(void);

/* Frees view info.  The parameter can be NULL. */
void modview_info_free(modview_info_t *vi);

TSTATIC_DEFS(
	int modview_is_raw(modview_info_t *vi);
	int modview_is_detached(modview_info_t *vi);
	const char * modview_current_viewer(modview_info_t *vi);
	int modview_current_line(modview_info_t *vi);
	struct strlist_t;
	struct strlist_t modview_lines(modview_info_t *vi);
)

#endif /* VIFM__MODES__VIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
