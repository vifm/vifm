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

/* Opaque declaration of structure describing menu state. */
typedef struct menu_state_t menu_state_t;

/* Menu data related to specific menu rather than to state of menu mode or its
 * UI. */
typedef struct menu_data_t
{
	int top;      /* Index of first visible item. */
	int len;      /* Number of menu items. */
	int pos;      /* Menu item under the cursor. */
	int hor_pos;  /* Horizontal offset. */

	char *title;  /* Title of the menu. */
	char **items; /* Contains titles of all menu items. */

	/* Contains additional string data, associated with each of menu items, can be
	 * NULL. */
	char **data;

	/* Contains additional pointers for each menu entry, can be NULL. */
	void **void_data;

	/* Menu-specific shortcut handler, can be NULL.  Returns code that specifies
	 * both taken actions and what should be done next. */
	KHandlerResponse (*key_handler)(FileView *view, struct menu_data_t *m,
			const wchar_t keys[]);

	/* Callback that is called when menu item is selected.  Should return non-zero
	 * to stay in menu mode. */
	int (*execute_handler)(FileView *view, struct menu_data_t *m);

	/* Text displayed by display_menu() function in case menu is empty, it can be
	 * NULL if this cannot happen. */
	char *empty_msg;

	/* Base for relative paths for navigation. */
	char *cwd;

	/* For filetype background, mime flags and such. */
	int extra_data;

	/* Whether this menu when non-empty should be saved for future use on closing
	 * menu. */
	int stashable;

	menu_state_t *state; /* Opaque pointer to menu mode state. */
	int initialized;     /* Marker that shows whether menu data needs freeing. */
}
menu_data_t;

/* Fills fields of menu_data_t structure with some safe values.  empty_msg is
 * text displayed by display_menu() function in case menu is empty, it can be
 * NULL if this cannot happen and will be freed by reset_menu_data(). */
void init_menu_data(menu_data_t *m, FileView *view, char title[],
		char empty_msg[]);

/* Frees resources associated with the menu and clears menu window. */
void reset_menu_data(menu_data_t *m);

void setup_menu(void);

/* Removes current menu item and redraws the menu. */
void remove_current_item(menu_state_t *ms);

/* Erases current menu item in menu window. */
void menu_current_line_erase(menu_state_t *m);

void move_to_menu_pos(int pos, menu_state_t *m);

void redraw_menu(menu_state_t *m);

void draw_menu(menu_state_t *m);

/* Navigates to/open path specification.  Specification can contain colon
 * followed by a line number when try_open is not zero.  Returns zero on
 * successful parsing and performed try to handle the file otherwise non-zero is
 * returned. */
int goto_selected_file(menu_data_t *m, FileView *view, const char spec[],
		int try_open);

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
		menu_state_t *m);

/* Prepares menu, draws it and switches to the menu mode.  Returns non-zero if
 * status bar message should be saved. */
int display_menu(menu_state_t *m, FileView *view);

/* Restore previously saved menu.  Returns non-zero if status bar message should
 * be saved. */
int unstash_menu(FileView *view);

/* Predefined key handler for processing keys on elements of file lists.
 * Returns code that specifies both taken actions and what should be done
 * next. */
KHandlerResponse filelist_khandler(FileView *view, menu_data_t *m,
		const wchar_t keys[]);

/* Moves menu items into custom view.  Returns zero on success, otherwise
 * non-zero is returned. */
int menu_to_custom_view(menu_state_t *m, FileView *view, int very);

/* Either makes a menu or custom view out of command output.  Returns non-zero
 * if status bar message should be saved. */
int capture_output(FileView *view, const char cmd[], int user_sh,
		menu_data_t *m, int custom_view, int very_custom_view);

/* Performs search in requested direction.  Either continues the previous one or
 * restarts it. */
void menus_search(menu_state_t *m, int backward);

/* Performs search of pattern among menu items.  NULL pattern requests use of
 * the last used pattern.  Returns new value for save_msg flag, but when
 * print_errors isn't requested can return -1 to indicate issues with the
 * pattern. */
int search_menu_list(const char pattern[], menu_data_t *m, int print_errors);

/* Prints results or error message about search operation to the user. */
void menu_print_search_msg(const menu_state_t *m);

/* Reset search highlight of a menu. */
void menus_reset_search_highlight(menu_state_t *m);

/* Retrieves number of search matches in the menu.  Returns the number. */
int menu_get_matches(menu_state_t *m);

/* Resets search state of the menu according to specified parameters. */
void menu_new_search(menu_state_t *m, int backward, int new_repeat_count);

/* Changes active menu data. */
void menus_replace_menu(menu_data_t *m);

#endif /* VIFM__MENUS__MENUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
