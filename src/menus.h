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

void show_bookmarks_menu(FileView *view);
void show_commands_menu(FileView *view);
void show_history_menu(FileView *view);
void show_vifm_menu(FileView *view);
void show_filetypes_menu(FileView *view);
void show_jobs_menu(FileView *view);
void show_locate_menu(FileView *view, char *args);
void show_apropos_menu(FileView *view, char *args);
void show_user_menu(FileView *view, char *command); 
void show_register_menu(FileView *view);
void reset_popup_menu(void);
void setup_menu(FileView *view);
void show_error_msg(char * title, char *message);
int search_menu_list(FileView *view, char * command, void * ptr);
int execute_menu_command(FileView *view, char * command, void * ptr);
int query_user_menu(char *title, char *message);

#endif
