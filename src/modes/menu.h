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

#include "../ui/ui.h"
#include "../menus/menus.h"
#include "cmdline.h"

/* Initializes menu mode. */
void init_menu_mode(void);

void enter_menu_mode(menu_data_t *m, FileView *active_view);

/* Replaces menu of the menu mode. */
void reenter_menu_mode(menu_data_t *m);

void menu_pre(void);

/* Performs post-actions (at the end of input processing loop) for menus. */
void menu_post(void);

/* Redraws menu. */
void menu_redraw(void);

/* Redraws and refreshes menu window. */
void update_menu(void);

void save_menu_pos(void);

void load_menu_pos(void);

/* Leaves menu and starts command-line mode.  external flag shows whether
 * cmd should be prepended with ":!".  To be used from keyboard handlers, which
 * in this case must return KHR_MORPHED_MENU. */
void menu_morph_into_cmdline(CmdLineSubmode submode, const char input[],
		int external);

/* Allows running regular command-line mode commands from menu mode. */
void execute_cmdline_command(const char cmd[]);

/* Returns index of last visible line in the menu.  Value returned may be
 * greater than or equal to number of lines in the menu, which should be
 * treated correctly. */
int get_last_visible_line(const menu_data_t *m);

#endif /* VIFM__MODES__MENU_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
