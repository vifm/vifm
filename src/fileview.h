/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__FILEVIEW_H__
#define VIFM__FILEVIEW_H__

#include "ui/ui.h"

/* Initialization/termination functions. */

void fview_init(void);

/* Appearance related functions. */

void draw_dir_list(FileView *view);

/* Redraws directory list without any extra actions that are performed in
 * draw_dir_list(). */
void draw_dir_list_only(FileView *view);

void erase_current_line_bar(FileView *view);

/* Returns non-zero if redraw is needed. */
int move_curr_line(FileView *view);

/* Viewport related functions. */

/* Returns non-zero if all files are visible, so no scrolling is needed. */
int all_files_visible(const FileView *view);

/* Adds inactive cursor mark to the view. */
void put_inactive_mark(FileView *view);

void move_to_list_pos(FileView *view, int pos);

/* Scrolls view up at least by specified number of files.  Updates both top and
 * cursor positions. */
void scroll_up(FileView *view, size_t by);

/* Scrolls view down at least by specified number of files.  Updates both top
 * and cursor positions. */
void scroll_down(FileView *view, size_t by);

/* Layout related functions. */

/* Enables/disables ls-like style of the view. */
void fview_set_lsview(FileView *view, int enabled);

/* Returns number of columns in the view. */
size_t calculate_columns_count(FileView *view);

/* Callback-like function which triggers some view-specific updates. */
void fview_list_updated(FileView *view);

#endif /* VIFM__FILEVIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
