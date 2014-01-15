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

#include <stdio.h> /* FILE */

#include "../ui.h"

enum
{
	NONE,
	UP,
	DOWN
};

/* List of menu identifiers. */
enum
{
	APROPOS_MENU,
	BOOKMARK_MENU,
	CMDHISTORY_MENU,
	FSEARCHHISTORY_MENU,
	BSEARCHHISTORY_MENU,
	PROMPTHISTORY_MENU,
	FILTERHISTORY_MENU,
	COLORSCHEME_MENU,
	COMMANDS_MENU,
	DIRSTACK_MENU,
	FILETYPE_MENU,
	FIND_MENU,
	DIRHISTORY_MENU,
	JOBS_MENU,
	LOCATE_MENU,
	MAP_MENU,
	REGISTER_MENU,
	UNDOLIST_MENU,
	USER_MENU,
	USER_NAVIGATE_MENU,
	VIFM_MENU,
	GREP_MENU,
	TRASH_MENU,
	TRASHES_MENU,
#ifdef _WIN32
	VOLUMES_MENU,
#endif
};

typedef struct menu_info
{
	int top;
	int current;
	int len;
	int pos;
	int hor_pos;
	int win_rows;
	int type;
	int match_dir;
	/* Number of menu entries that actually match the regexp. */
	int matching_entries;
	int *matches;
	char *regexp;
	char *title;
	char *args;
	/* Contains titles of all menu items. */
	char **items;
	/* Contains additional data, associated with each of menu items, can be
	 * NULL. */
	char **data;
	/* Should return value > 0 to request menu window refresh and < 0 on invalid
	 * key. */
	int (*key_handler)(struct menu_info *m, wchar_t *keys);
	int extra_data; /* For filetype background and mime flags. */
	/* Callback that is called when menu item is selected.  Should return non-zero
	 * to stay in menu mode. */
	int (*execute_handler)(FileView *view, struct menu_info *m);
	/* Text displayed by display_menu() function in case menu is empty, it can be
	 * NULL if this cannot happen and will be freed by reset_popup_menu(). */
	char *empty_msg;
}menu_info;

/* Fills fields of menu_info structure with some safe values.  empty_msg is
 * text displayed by display_menu() function in case menu is empty, it can be
 * NULL if this cannot happen and will be freed by reset_popup_menu(). */
void init_menu_info(menu_info *m, int menu_type, char empty_msg[]);
void reset_popup_menu(menu_info *m);
void setup_menu(void);
/* Shows error message to a user. */
void show_error_msg(const char title[], const char message[]);
/* Same as show_error_msg(...), but with format. */
void show_error_msgf(const char title[], const char format[], ...);
/* Same as show_error_msg(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msg(const char title[], const char message[]);
/* Same as show_error_msgf(...), but asks about future errors.  Returns non-zero
 * when user asks to skip error messages that left. */
int prompt_error_msgf(const char title[], const char format[], ...);
int query_user_menu(char *title, char *message);
/* Redraws currently visible error message on the screen. */
void redraw_error_msg_window(void);
/* Removes current menu item and redraws the menu. */
void remove_current_item(menu_info *m);
void clean_menu_position(menu_info *m);
void move_to_menu_pos(int pos, menu_info *m);
void redraw_menu(menu_info *m);
void draw_menu(menu_info *m);
/* Nativates to selected menu item.  Supports multiple menu types. */
void goto_selected_file(FileView *view, menu_info *m);
/* Nativates to selected menu item. */
void goto_selected_directory(FileView *view, menu_info *m);
/* Closes ef. */
void print_errors(FILE *ef);
/* Gets list of target files/directories in the current view.  On success
 * returns newly allocated string, which should be freed by the caller,
 * otherwise NULL is returned. */
char * get_cmd_target(void);

/* Runs external command and puts its output to the m menu.  Returns non-zero if
 * status bar message should be saved. */
int capture_output_to_menu(FileView *view, const char cmd[], menu_info *m);
/* Prepares menu, draws it and switches to the menu mode.  Returns non-zero if
 * status bar message should be saved. */
int display_menu(menu_info *m, FileView *view);

#endif /* VIFM__MENUS__MENUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
