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

#ifndef VIFM__MENUS__USERS_MENU_H__
#define VIFM__MENUS__USERS_MENU_H__

#include "../macros.h"

struct strlist_t;
struct view_t;

/* Creates menu out of output of the command.  Returns non-zero if status bar
 * message should be saved. */
int show_user_menu(struct view_t *view, const char command[],
		const char title[], MacroFlags flags);

/* Creates menu from a list of items.  Takes ownership of the items (including
 * freeing them on error).  Returns zero on success. */
int show_custom_menu(struct view_t *view, const char title[],
		struct strlist_t items, int with_navigation);

#endif /* VIFM__MENUS__USERS_MENU_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
