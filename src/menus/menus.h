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

#ifndef VIFM__MENUS__MENUS_H__
#define VIFM__MENUS__MENUS_H__

#include <stddef.h> /* wchar_t */

#include "../ui/ui.h"

/* Result of handling key sequence by menu-specific shortcut handler. */
typedef enum
{
	KHR_REFRESH_WINDOW, /* Menu window refresh is needed. */
	KHR_CLOSE_MENU,     /* Menu mode should be left. */
	KHR_MORPHED_MENU,   /* Menu was morphed menu_morph_into_cmdline. */
	KHR_UNHANDLED,      /* Passed key wasn't handled. */
}
KHandlerResponse;

typedef struct menu_info
{
	int top;
	int current; /* Cursor position on the menu_win. */
	int len;
	int pos; /* Menu item under the cursor. */
	int hor_pos;
	int win_rows;
	int backward_search; /* Search direction. */
	/* Number of menu entries that actually match the regexp. */
	int matching_entries;
	/* Whether search highlight matches are currently highlighted. */
	int search_highlight;
	/* Start and end positions of search match.  If there is no match, values are
	 * equal to -1. */
	short int (*matches)[2];
	char *regexp;
	char *title;
	/* Contains titles of all menu items. */
	char **items;
	/* Contains additional data, associated with each of menu items, can be
	 * NULL. */
	char **data;
	/* Menu-specific shortcut handler, can be NULL.  Returns code that specifies
	 * both taken actions and what should be done next. */
	KHandlerResponse (*key_handler)(struct menu_info *m, const wchar_t keys[]);
	int extra_data; /* For filetype background, mime flags and such. */
	/* Callback that is called when menu item is selected.  Should return non-zero
	 * to stay in menu mode. */
	int (*execute_handler)(FileView *view, struct menu_info *m);
	/* Text displayed by display_menu() function in case menu is empty, it can be
	 * NULL if this cannot happen and will be freed by reset_popup_menu(). */
	char *empty_msg;
	/* Number of times to repeat search. */
	int search_repeat;
}
menu_info;

/* Fills fields of menu_info structure with some safe values.  empty_msg is
 * text displayed by display_menu() function in case menu is empty, it can be
 * NULL if this cannot happen and will be freed by reset_popup_menu(). */
void init_menu_info(menu_info *m, char title[], char empty_msg[]);

/* Frees resources associated with the menu and clears menu window. */
void reset_popup_menu(menu_info *m);

void setup_menu(void);

/* Removes current menu item and redraws the menu. */
void remove_current_item(menu_info *m);

/* Erases current menu item in menu window. */
void menu_current_line_erase(menu_info *m);

void move_to_menu_pos(int pos, menu_info *m);

void redraw_menu(menu_info *m);

void draw_menu(menu_info *m);

/* Navigates to/open path specification.  Specification can contain colon
 * followed by a line number when try_open is not zero.  Returns zero on
 * successful parsing and performed try to handle the file otherwise non-zero is
 * returned. */
int goto_selected_file(FileView *view, const char spec[], int try_open);

/* Navigates to directory from a menu. */
void goto_selected_directory(FileView *view, const char path[]);

/* Forms list of target files/directories in the current view and possibly
 * changes working directory to use relative paths.  On success returns newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
char * prepare_targets(FileView *view);

/* Runs external command and puts its output to the m menu.  Returns non-zero if
 * status bar message should be saved. */
int capture_output_to_menu(FileView *view, const char cmd[], int user_sh,
		menu_info *m);

/* Prepares menu, draws it and switches to the menu mode.  Returns non-zero if
 * status bar message should be saved. */
int display_menu(menu_info *m, FileView *view);

/* Predefined key handler for processing keys on elements of file lists.
 * Returns code that specifies both taken actions and what should be done
 * next. */
KHandlerResponse filelist_khandler(menu_info *m, const wchar_t keys[]);

/* Moves menu items into custom view.  Returns zero on success, otherwise
 * non-zero is returned. */
int menu_to_custom_view(menu_info *m, FileView *view, int very);

/* Either makes a menu or custom view out of command output.  Returns non-zero
 * if status bar message should be saved. */
int capture_output(FileView *view, const char cmd[], int user_sh, menu_info *m,
		int custom_view, int very_custom_view);

/* Performs search in requested direction.  Either continues the previous one or
 * restarts it. */
void menus_search(menu_info *m, int backward);

/* Performs search of pattern among menu items.  NULL pattern requests use of
 * the last used pattern.  Returns new value for save_msg flag, but when
 * print_errors isn't requested can return -1 to indicate issues with the
 * pattern. */
int search_menu_list(const char pattern[], menu_info *m, int print_errors);

/* Prints results or error message about search operation to the user. */
void menu_print_search_msg(const menu_info *m);

/* Reset search highlight of a menu. */
void menus_reset_search_highlight(menu_info *m);

#endif /* VIFM__MENUS__MENUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
