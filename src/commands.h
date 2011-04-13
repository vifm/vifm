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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "ui.h"

typedef struct
{
	char *action;
	char *name;
}command_t;

extern char *reserved_commands[];

command_t *command_list;

int get_command(FileView *view, int type, void *ptr);
void shellout(char *command, int pause);
void add_command(char *name, char *action);
int execute_command(FileView *view, char *action);
int sort_this(const void *one, const void *two);
int is_user_command(char *command);
int command_is_reserved(char *command);
char * command_completion(char *str);
char * expand_macros(FileView *view, char *command, char *args, int *menu, int *split);
void remove_command(char *name);
void comm_quit();

#endif
