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

#ifndef VIFM__MENUS__FILETYPES_MENU_H__
#define VIFM__MENUS__FILETYPES_MENU_H__

struct view_t;

/* Displays file handler picking menu.  Returns non-zero if status bar message
 * should be saved. */
int show_file_menu(struct view_t *view, int background);

/* Displays menu with list of programs registered for specified file name.
 * Returns non-zero if status bar message should be saved. */
int show_fileprograms_menu(struct view_t *view, const char fname[]);

/* Displays menu with list of viewers registered for specified file name.
 * Returns non-zero if status bar message should be saved. */
int show_fileviewers_menu(struct view_t *view, const char fname[]);

#endif /* VIFM__MENUS__FILETYPES_MENU_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
