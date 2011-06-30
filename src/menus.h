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

#ifndef __MENUS_H__
#define __MENUS_H__

#include "ui.h"

enum {
	APROPOS,
	BOOKMARK,
	CMDHISTORY,
	COLORSCHEME,
	COMMAND,
	DIRSTACK,
	FILETYPE,
	HISTORY,
	JOBS,
	LOCATE,
	MAP,
	REGISTER,
	UNDOLIST,
	USER,
	VIFM,
};

enum {
	NONE,
	UP,
	DOWN
};

typedef struct menu_info
{
	int top;
	int current;
	int len;
	int pos;
	int win_rows;
	int type;
	int match_dir;
	int matching_entries;
	char *regexp;
	char *title;
	char *args;
	char **data;
	/* should return not zero value to request menu redraw */
	int (*key_handler)(struct menu_info *m, wchar_t c);
	int extra_data; /* for filetype background and mime flags */
	/* For user menus only */
	char *get_info_script; /* program + args to fill in menu. */
}menu_info;

void show_bookmarks_menu(FileView *view);
void show_dirstack_menu(FileView *view);
void show_colorschemes_menu(FileView *view);
void show_commands_menu(FileView *view);
void show_history_menu(FileView *view);
void show_cmdhistory_menu(FileView *view);
void show_vifm_menu(FileView *view);
void show_filetypes_menu(FileView *view, int background);
void show_jobs_menu(FileView *view);
void show_locate_menu(FileView *view, char *args);
void show_map_menu(FileView *view, const char *mode_str, wchar_t **list);
void show_apropos_menu(FileView *view, char *args);
void show_user_menu(FileView *view, char *command);
void show_register_menu(FileView *view);
void show_undolist_menu(FileView *view, int with_details);
void reset_popup_menu(menu_info *m);
void setup_menu(void);
void redraw_error_msg(char *title_arg, const char *message_arg);
/* Returns not zero when user asked to skip error messages that left */
int show_error_msg(char * title, const char *message);
int search_menu_list(char *command, menu_info *ptr);
int query_user_menu(char *title, char *message);
void clean_menu_position(menu_info *m);
void moveto_menu_pos(int pos, menu_info *m);
void redraw_menu(menu_info *m);
void draw_menu(menu_info *m);
void execute_menu_cb(FileView *view, menu_info *m);
void reload_command_menu_list(menu_info *m);
void reload_bookmarks_menu_list(menu_info *m);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
