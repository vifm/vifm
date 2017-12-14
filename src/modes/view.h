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

struct view_t;

/* Holds state of a single view mode window. */
typedef struct view_info_t view_info_t;

/* Initializes view mode. */
void view_init_mode(void);

/* Enters view mode when possible.  View is the expected output area. */
void view_enter_mode(struct view_t *view, int explore);

/* Leaves view mode. */
void view_leave_mode(void);

/* Quits view from explore mode.  Assumes the view is not an active one.
 * Automatically redraws view. */
void view_quit_explore_mode(struct view_t *view);

/* Activates the mode if current pane is in explore mode. */
void view_try_activate_mode(void);

/* Performs pre main loop actions for the view mode, which is assumed to be
 * activated. */
void view_pre(void);

/* Performs post main loop actions for the view mode, which is assumed to be
 * activated. */
void view_post(void);

/* Displays view mode specific position information.  Assumes that view mode is
 * active. */
void view_ruler_update(void);

/* Redraws view mode. */
void view_redraw(void);

/* Performs search for the pattern in specified direction inside active view.
 * Returns positive number zero if there is something on the status bar to
 * preserve, otherwise zero is returned. */
int view_find_pattern(const char pattern[], int backward);

/* Callback-like routine that handles swapping of panes. */
void view_panes_swapped(void);

/* Checks whether contents of either view should be updated. */
void view_check_for_updates(void);

/* Detached views.  These are the views which were either created in detached
 * state or were detached from, but their state (position, etc.) is still
 * maintained. */

/* Creates detached view in the view using output of passed command as a
 * source, but doesn't enter the view mode. */
void view_detached_make(struct view_t *view, const char cmd[]);

/* Tries to draw an detached view mode and updates internal state if needed.
 * Returns non-zero on success, otherwise zero is returned. */
int view_detached_draw(void);

/* Retrieves viewer command associated with preview created with
 * view_detached_make().  Returns pointer to the viewer command or NULL, if
 * there is none. */
const char * view_detached_get_viewer(void);

/* Frees view info.  The parameter can be NULL. */
void view_info_free(view_info_t *vi);

#endif /* VIFM__MODES__VIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
