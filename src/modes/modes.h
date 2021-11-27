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

#ifndef VIFM__MODES__MODES_H__
#define VIFM__MODES__MODES_H__

#include <stddef.h>

enum
{
	NORMAL_MODE,
	CMDLINE_MODE,
	VISUAL_MODE,
	MENU_MODE,
	SORT_MODE,
	ATTR_MODE,
	CHANGE_MODE,
	VIEW_MODE,
	FILE_INFO_MODE,
	MSG_MODE,
	MORE_MODE,
	MODES_COUNT
};

void init_modes(void);

void modes_pre(void);

/* Executes poll-based requests for any of the active modes. */
void modes_periodic(void);

void modes_post(void);

void modes_redraw(void);

void modes_update(void);

void modupd_input_bar(wchar_t *str);

void clear_input_bar(void);

/* Returns non-zero if current mode is a menu like one. */
int is_in_menu_like_mode(void);

/* Checks if current mode is a dialog.  Returns non-zero if so. */
int modes_is_dialog_like(void);

/* Aborts one of menu-like modes if any of them is currently active. */
void abort_menu_like_mode(void);

void print_selected_msg(void);

#endif /* VIFM__MODES__MODES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
