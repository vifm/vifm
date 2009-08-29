/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#ifndef __KEYS_H__
#define __KEYS_H__

#include "ui.h"

enum {
	CHANGE_WINDOWS,
	GET_COMMAND,
	GET_BOOKMARK,
	GET_SEARCH_PATTERN,
	GET_VISUAL_COMMAND,
	START_VISUAL_MODE,
	MAPPED_COMMAND,
	MAPPED_SEARCH,
	MENU_SEARCH,
	MENU_COMMAND
};


void main_key_press_cb(FileView *view);
void update_all_windows(void);
void show_dot_files(FileView *view);
void remove_filename_filter(FileView *view);
void rename_file(FileView *view);
void switch_views(void);
#endif


