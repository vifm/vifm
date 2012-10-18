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

#ifndef __MENUS_H__
#define __MENUS_H__

#include <stdio.h> /* FILE */

#include "../ui.h"

enum
{
	NONE,
	UP,
	DOWN
};

enum
{
	APROPOS,
	BOOKMARK,
	CMDHISTORY,
	PROMPTHISTORY,
	FSEARCHHISTORY,
	BSEARCHHISTORY,
	COLORSCHEME,
	COMMAND,
	DIRSTACK,
	FILETYPE,
	FIND,
	DIRHISTORY,
	JOBS,
	LOCATE,
	MAP,
	REGISTER,
	UNDOLIST,
	USER,
	USER_NAVIGATE,
	VIFM,
	GREP,
#ifdef _WIN32
	VOLUMES,
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
	int matching_entries;
	int *matches;
	char *regexp;
	char *title;
	char *args;
	/* contains titles of all menu items */
	char **items;
	/* contains additional data, associated with each of menu items, can be
	 * NULL */
	char **data;
	/* should return value > 0 to request menu window refresh and < 0 on invalid
	 * key */
	int (*key_handler)(struct menu_info *m, wchar_t *keys);
	int extra_data; /* for filetype background and mime flags */
	int (*execute_handler)(FileView *view, struct menu_info *m);
}menu_info;

/* Fills fields of menu_info structure with some safe values. */
void init_menu_info(menu_info *m, int menu_type);
void reset_popup_menu(menu_info *m);
void setup_menu(void);
/* Shows error message to a user. */
void show_error_msg(const char title[], const char message[]);
/* Same as show_error_msg(...), but with format. */
void show_error_msgf(const char title[], const char format[], ...);
/* Same as show_error_msg(...), but asks about future errors.  Returns not zero
 * when user asked to skip error messages that left. */
int prompt_error_msg(const char title[], const char message[]);
/* Same as show_error_msgf(...), but asks about future errors.  Returns not zero
 * when user asked to skip error messages that left. */
int prompt_error_msgf(const char title[], const char format[], ...);
int query_user_menu(char *title, char *message);
/* Redraws currently visible error message on the screen. */
void redraw_error_msg_window(void);
void clean_menu_position(menu_info *m);
void move_to_menu_pos(int pos, menu_info *m);
void redraw_menu(menu_info *m);
void draw_menu(menu_info *m);
/* Returns zero if menu mode should be leaved */
int execute_menu_cb(FileView *view, menu_info *m);
int print_errors(FILE *ef);

/* Runs external command and puts its output to the m menu.  Returns non-zero
 * on errors and calls reset_popup_menu() in such case. */
int capture_output_to_menu(FileView *view, const char cmd[], menu_info *m);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
