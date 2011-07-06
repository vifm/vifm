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

#include "macros.h"
#include "ui.h"

enum {
	GET_COMMAND,
	GET_FSEARCH_PATTERN,
	GET_BSEARCH_PATTERN,
	GET_VISUAL_COMMAND,
};

enum
{
	COM_EXECUTE,
	COM_APROPOS,
	COM_CD,
	COM_CHANGE,
	COM_CMAP,
	COM_CMDHISTORY,
	COM_COLO,
	COM_COLORSCHEME,
	COM_COM,
	COM_COMMAND,
	COM_D,
	COM_DELCOMMAND,
	COM_DELETE,
	COM_DIRS,
	COM_DISPLAY,
	COM_E,
	COM_EDIT,
	COM_EMPTY,
	COM_FILE,
	COM_FILTER,
	COM_H,
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
	COM_NOHLSEARCH,
	COM_ONLY,
	COM_POPD,
	COM_PUSHD,
	COM_PWD,
	COM_Q,
	COM_QUIT,
	COM_REGISTER,
	COM_RENAME,
	COM_SCREEN,
	COM_SET,
	COM_SHELL,
	COM_SORT,
	COM_SP,
	COM_SPLIT,
	COM_SYNC,
	COM_UNDOLIST,
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

command_t *command_list;

int exec_commands(char *cmd, FileView *view, int type, int save_hist);
int shellout(char *command, int pause);
void add_command(char *name, char *action);
int execute_command(FileView *view, char *action);
char * fast_run_complete(char *cmd);
int sort_this(const void *one, const void *two);
int get_buildin_id(const char *cmd_line);
char * command_completion(char *str, int users_only);
char * expand_macros(FileView *view, const char *command, const char *args,
		int *menu, int *split);
void remove_command(char *name);
void _gnuc_noreturn comm_quit(void);
void comm_only(void);
void comm_split(void);

#ifdef TEST
extern const char *reserved_commands[];

char * append_selected_files(FileView *view, char *expanded, int under_cursor);
char * edit_cmd_selection(FileView *view);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
