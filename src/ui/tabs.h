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

#include "../status.h"

/* Implementation of combination of global and pane tabs. */

/* Information about UI configuration of a tab.  For pane tabs preview is the
 * only field that can differ from current state. */
typedef struct
{
	int active_pane;       /* 0 -- left, 1 -- right. */
	int only_mode;         /* Whether in single-pane mode. */
	SPLIT split;           /* State of window split. */
	int splitter_pos;      /* Position of the splitter. */
	double splitter_ratio; /* Relative position of the splitter. */
	int preview;           /* Whether preview mode is active. */
}
tab_layout_t;

/* Information about a tab. */
typedef struct tab_info_t tab_info_t;
struct tab_info_t
{
	struct view_t *view; /* View associated with the tab. */
	const char *name;    /* Name of the view or NULL. */
	tab_layout_t layout; /* UI configuration information. */
	unsigned int id;     /* Unique id of this tab. */
	int last;            /* This is the last tab. */
};

/* Performs initialization of the unit. */
void tabs_init(void);

/* Re-initializes the unit in preparation for restarting. */
void tabs_reinit(void);

/* Creates a new tab and switches to it.  Name can be NULL.  Path specifies
 * location of active pane and can be NULL.  Returns zero on success, otherwise
 * non-zero is returned. */
int tabs_new(const char name[], const char path[]);

/* Sets name of the current tab.  Name can be NULL. */
void tabs_rename(struct view_t *side, const char name[]);

/* Switches to tab specified by its zero-based index if it's valid. */
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

/* Fills *tab_info for the tab specified by its index and side.  Returns
 * non-zero on success, otherwise zero is returned. */
int tabs_get(struct view_t *side, int idx, tab_info_t *tab_info);

/* tabs_get() equivalent that returns left or right pane for global tabs
 * depending on value of the side parameter. */
int tabs_enum(struct view_t *side, int idx, tab_info_t *tab_info);

/* Like tabs_enum(), but enumerates all tabs (first all left ones and then the
 * right ones). */
int tabs_enum_all(int idx, tab_info_t *tab_info);

/* Retrieves index of the current tab.  Returns the index. */
int tabs_current(const struct view_t *side);

/* Retrieves number of tabs.  Returns the count. */
int tabs_count(const struct view_t *side);

/* Closes all tabs except for the current one. */
void tabs_only(struct view_t *side);

/* Moves current tab to a different position.  Zero position signifies "before
 * first tab" and non-zero position means "after Nth tab".  Position is
 * normalized to be in allowed bounds. */
void tabs_move(struct view_t *side, int where_to);

/* Counts how many tabs are in subtree defined by the path. */
int tabs_visitor_count(const char path[]);

/* Swaps pane tabs and does nothing for global tabs. */
void tabs_switch_panes(void);

/* Fills layout structure with current information. */
void tabs_layout_fill(tab_layout_t *layout);

/* Appends new incompletely configured global tab and sets input pointers to its
 * views.  Name can be NULL.  Returns zero on success, otherwise non-zero is
 * returned. */
int tabs_setup_gtab(const char name[], const tab_layout_t *layout,
		struct view_t **left, struct view_t **right);

/* Appends new incompletely configured pane tab on the specified side.  Name can
 * be NULL.  Returns pointer to view on the new tab or NULL on error. */
struct view_t * tabs_setup_ptab(struct view_t *view, const char name[],
		int preview);

#endif /* VIFM__UI__TABS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
