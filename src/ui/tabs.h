/* vifm
 * Copyright (C) 2018 xaizek.
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

#ifndef VIFM__UI__TABS_H__
#define VIFM__UI__TABS_H__

/* Implementation of combination of global and pane tabs. */

/* Information about a tab. */
typedef struct
{
	struct view_t *view; /* View associated with the tab. */
	const char *name;    /* Name of the view or NULL. */
	int last;            /* This is the last tab. */
}
tab_info_t;

/* Performs initialization of the unit. */
void tabs_init(void);

/* Creates a new tab and switches to it.  Name can be NULL.  Path specifies
 * location of active pane and can be NULL.  Returns zero on success, otherwise
 * non-zero is returned. */
int tabs_new(const char name[], const char path[]);

/* Sets name of the current tab.  Name can be NULL. */
void tabs_rename(struct view_t *view, const char name[]);

/* Switches to tab specified by its index if the index is valid. */
void tabs_goto(int idx);

/* Checks whether closing a tab should result in closing the application.
 * Returns non-zero if so, otherwise zero is returned. */
int tabs_quit_on_close(void);

/* Closes active tab unless it's the only tab. */
void tabs_close(void);

/* Switches to the nth next tab wrapping at the end of list of tabs. */
void tabs_next(int n);

/* Switches to the nth previous tab wrapping at the beginning of list of
 * tabs. */
void tabs_previous(int n);

/* Fills *tab_info for the tab specified by its index and side (view parameter).
 * Returns non-zero on success, otherwise zero is returned. */
int tabs_get(struct view_t *view, int idx, tab_info_t *tab_info);

/* tabs_get() equivalent that returns left or right pane for global tabs
 * depending on value of the view parameter. */
int tabs_enum(struct view_t *view, int idx, tab_info_t *tab_info);

/* Retrieves index of the current tab.  Returns the index. */
int tabs_current(const struct view_t *view);

/* Retrieves number of tabs.  Returns the count. */
int tabs_count(const struct view_t *view);

/* Closes all tabs except for the current one. */
void tabs_only(struct view_t *view);

/* Moves current tab to a different position.  Zero position signifies "before
 * first tab" and non-zero position means "after Nth tab".  Position is
 * normalized to be in allowed bounds. */
void tabs_move(struct view_t *view, int where_to);

/* Counts how many tabs are in subtree defined by the path. */
int tabs_visitor_count(const char path[]);

/* Swaps pane tabs and does nothing for global tabs. */
void tabs_switch_panes(void);

/* Reloads non-current tabs by copying state of lwin and rwin into corresponding
 * views, but preserving their current locations. */
void tabs_reload(void);

#endif /* VIFM__UI__TABS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
