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

enum {
	CHANGE_WINDOWS,
	GET_COMMAND,
	GET_BOOKMARK,
	GET_FSEARCH_PATTERN,
	GET_BSEARCH_PATTERN,
	GET_VISUAL_COMMAND,
	MAPPED_COMMAND,
	MAPPED_SEARCH
};

enum
{
	COM_EXECUTE,
	COM_APROPOS,
	COM_CD,
	COM_CHANGE,
	COM_CMAP,
	COM_COLORSCHEME,
	COM_COMMAND,
	COM_DELCOMMAND,
	COM_DELETE,
	COM_DISPLAY,
	COM_EDIT,
	COM_EMPTY,
	COM_FILE,
	COM_FILTER,
	COM_HELP,
	COM_HISTORY,
	COM_INVERT,
	COM_JOBS,
	COM_LOCATE,
	COM_LS,
	COM_MAP,
	COM_MARKS,
	COM_NMAP,
	COM_NOH,
	COM_ONLY,
	COM_POPD,
	COM_PUSHD,
	COM_PWD,
	COM_QUIT,
	COM_REGISTER,
	COM_SCREEN,
	COM_SET,
	COM_SHELL,
	COM_SORT,
	COM_SPLIT,
	COM_SYNC,
	COM_UNMAP,
	COM_VIEW,
	COM_VIFM,
	COM_VMAP,
	COM_W,
	COM_WQ,
	COM_WRITE,
	COM_X,
	COM_YANK,
  RESERVED
};

typedef struct
{
	char *action;
	char *name;
}command_t;

extern char *reserved_commands[];

command_t *command_list;

int exec_commands(char *cmd, FileView *view, int type, void * ptr,
		int save_hist);
void shellout(char *command, int pause);
void add_command(char *name, char *action);
int execute_command(FileView *view, char *action);
int sort_this(const void *one, const void *two);
int is_user_command(char *command);
int command_is_reserved(char *command);
char * command_completion(char *str, int users_only);
char * expand_macros(FileView *view, char *command, char *args, int *menu, int *split);
void remove_command(char *name);
void comm_quit(void);
void comm_only(void);
void comm_split(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
