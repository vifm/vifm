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

#include "../ui/ui.h"

/* Initializes view mode. */
void init_view_mode(void);

/* Enters view mode when possible.  View is the expected output area. */
void enter_view_mode(FileView *view, int explore);

/* Creates abandoned view in the view using output of passed command as a
 * source, but doesn't enter the view mode. */
void make_abandoned_view(FileView *view, const char cmd[]);

void leave_view_mode(void);

/* Quits view from explore mode.  Assumes the view is not an active one.
 * Automatically redraws view. */
void view_explore_mode_quit(FileView *view);

/* In case current pane is in explore mode, activate the mode. */
void try_activate_view_mode(void);

void view_pre(void);

/* Performs post main loop actions for the view mode, which is assumed to be
 * activated. */
void view_post(void);

/* Displays view mode specific position information.  Assumes that view mode is
 * active. */
void view_ruler_update(void);

void view_redraw(void);

int find_vwpattern(const char *pattern, int backward);

/* Handles switch of panes. */
void view_switch_views(void);

/* Tries to draw an abandoned view mode and updates internal state if needed.
 * Returns non-zero on success, otherwise zero is returned. */
int draw_abandoned_view_mode(void);

/* Checks whether contents of either view should be updated. */
void view_check_for_updates(void);

#endif /* VIFM__MODES__VIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
