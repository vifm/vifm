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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __MODES_H__
#define __MODES_H__

enum
{
	NORMAL_MODE,
	CMDLINE_MODE,
	VISUAL_MODE,
	MENU_MODE,
	SORT_MODE,
	PERMISSIONS_MODE,
	MODES_COUNT
};

void init_modes(void);
void modes_pre(void);
void modes_post(void);
void modes_redraw(void);
void modes_update(void);
void add_to_input_bar(wchar_t c);
void clear_input_bar(void);
/* returns current mode id */
int get_mode(void);
void print_selected_msg(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
