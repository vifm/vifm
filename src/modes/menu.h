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

#ifndef VIFM__MODES__MENU_H__
#define VIFM__MODES__MENU_H__

#include "../menus/menus.h"
#include "../utils/test_helpers.h"
#include "cmdline.h"

struct view_t;

/* Initializes menu mode. */
void modmenu_init(void);

/* Enters menu mode. */
void modmenu_enter(menu_data_t *m, struct view_t *active_view);

/* Aborts menu mode. */
void modmenu_abort(void);

/* Replaces menu of the menu mode. */
void modmenu_set_data(menu_data_t *m);

/* Performs pre main loop actions for the menu mode, which is assumed to be
 * activated. */
void modmenu_pre(void);

/* Performs post-actions (at the end of input processing loop) for the menu
 * mode. */
void modmenu_post(void);

/* Redraws menu mode. */
void modmenu_full_redraw(void);

/* Redraws only the main part of menu (window with elements) assuming that size
 * and other elements are handled elsewhere. */
void modmenu_partial_redraw(void);

/* Saves information about position into temporary storage (can't store more
 * than one state). */
void modmenu_save_pos(void);

/* Restores previously saved position information. */
void modmenu_restore_pos(void);

/* Leaves menu and starts command-line mode.  external flag shows whether
 * cmd should be prepended with ":!".  To be used from keyboard handlers, which
 * in this case must return KHR_MORPHED_MENU. */
void modmenu_morph_into_cline(CmdLineSubmode submode, const char input[],
		int external);

/* Allows running regular command-line mode commands from menu mode. */
void modmenu_run_command(const char cmd[]);

/* Returns index of last visible line in the menu.  Value returned may be
 * greater than or equal to number of lines in the menu, which should be
 * treated correctly. */
int modmenu_last_line(const menu_data_t *m);

TSTATIC_DEFS(
	menu_data_t * menu_get_current(void);
)

#endif /* VIFM__MODES__MENU_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
