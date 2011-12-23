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

#include <stdio.h>

#include "ui.h"

enum
{
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
	int hor_pos;
	int win_rows;
	int type;
	int match_dir;
	int matching_entries;
	int *matches;
	char *regexp;
	char *title;
	char *args;
	char **data;
	/* should return value > 0 to request menu window refresh and < 0 on invalid
	 * key */
	int (*key_handler)(struct menu_info *m, wchar_t *keys);
	int extra_data; /* for filetype background and mime flags */
}menu_info;

int show_bookmarks_menu(FileView *view, const char *marks);
int show_dirstack_menu(FileView *view);
void show_colorschemes_menu(FileView *view);
int show_commands_menu(FileView *view);
int show_history_menu(FileView *view);
int show_cmdhistory_menu(FileView *view);
int show_prompthistory_menu(FileView *view);
int show_fsearchhistory_menu(FileView *view);
int show_bsearchhistory_menu(FileView *view);
int show_vifm_menu(FileView *view);
int show_filetypes_menu(FileView *view, int background);
int run_with_filetype(FileView *view, const char *beginning, int background);
char * form_program_list(const char *filename);
int show_jobs_menu(FileView *view);
int show_locate_menu(FileView *view, const char *args);
int show_find_menu(FileView *view, int with_path, const char *args);
int show_grep_menu(FileView *view, const char *args, int invert);
void show_map_menu(FileView *view, const char *mode_str, wchar_t **list,
		const wchar_t *start);
void show_apropos_menu(FileView *view, char *args);
void show_user_menu(FileView *view, const char *command, int navigate);
int show_register_menu(FileView *view, const char *registers);
int show_undolist_menu(FileView *view, int with_details);
void show_volumes_menu(FileView *view);
void reset_popup_menu(menu_info *m);
void setup_menu(void);
void redraw_error_msg(const char *title_arg, const char *message_arg);
/* Returns not zero when user asked to skip error messages that left */
int show_error_msg(const char *title, const char *message);
int show_error_msgf(char *title, const char *format, ...);
int query_user_menu(char *title, char *message);
void clean_menu_position(menu_info *m);
void move_to_menu_pos(int pos, menu_info *m);
void redraw_menu(menu_info *m);
void draw_menu(menu_info *m);
/* Returns zero if menu mode should be leaved */
int execute_menu_cb(FileView *view, menu_info *m);
int print_errors(FILE *ef);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
