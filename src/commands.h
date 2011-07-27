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
	COM_CM_ALIAS_TO_CMAP,
	COM_CMAP,
	COM_CMDHISTORY,
	COM_COLO_ALIAS_TO_COLORSCHEME,
	COM_COLORSCHEME,
	COM_COM_ALIAS_TO_COMMAND,
	COM_COMMAND,
	COM_D_ALIAS_TO_DELETE,
	COM_DELC_ALIAS_TO_DELCOMMAND,
	COM_DELCOMMAND,
	COM_DELETE,
	COM_DI_ALIAS_TO_DISPLAY,
	COM_DIRS,
	COM_DISPLAY,
	COM_E_ALIAS_TO_EDIT,
	COM_EDIT,
	COM_EMPTY,
	COM_FILE,
	COM_FILTER,
	COM_H_ALIAS_TO_HELP,
	COM_HELP,
	COM_HIS_ALIAS_TO_HISTORY,
	COM_HISTORY,
	COM_INVERT,
	COM_JOBS,
	COM_LOCATE,
	COM_LS,
	COM_MAP,
	COM_MARKS,
	COM_NM_ALIAS_TO_NMAP,
	COM_NMAP,
	COM_NOH_ALIAS_TO_NOHLSEARCH,
	COM_NOHLSEARCH,
	COM_ON_ALIAS_TO_ONLY,
	COM_ONLY,
	COM_POPD,
	COM_PUSHD,
	COM_PWD,
	COM_Q_ALIAS_TO_QUIT,
	COM_QUIT,
	COM_REG_ALIAS_TO_REGISTERS,
	COM_REGISTERS,
	COM_RENAME,
	COM_SCREEN,
	COM_SE_ALIAS_TO_SET,
	COM_SET,
	COM_SH_ALIAS_TO_SHELL,
	COM_SHELL,
	COM_SOR_ALIAS_TO_SORT,
	COM_SORT,
	COM_SP_ALIAS_TO_SPLIT,
	COM_SPLIT,
	COM_SYNC,
	COM_UNDOL_ALIAS_TO_UNDOLIST,
	COM_UNDOLIST,
	COM_UNMAP,
	COM_VIEW,
	COM_VIFM,
	COM_VM_ALIAS_TO_VMAP,
	COM_VMAP,
	COM_W_ALIAS_TO_WRITE,
	COM_WQ,
	COM_WRITE,
	COM_X_ALIAS_TO_XIT,
	COM_XIT,
	COM_Y_ALIAS_TO_YANK,
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
int exec_command(char *cmd, FileView *view, int type);
char * find_last_command(char *cmd);
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
typedef struct current_command
{
	int start_range;
	int end_range;
	int count;
	char *cmd_name;
	char *args;
	char **argv;
	int argc;
	char *curr_files; /* holds %f macro files */
	char *other_files; /* holds %F macro files */
	char *user_args; /* holds %a macro string */
	char *order; /* holds the order of macros command %a %f or command %f %a */
	int background;
	int split;
	int builtin;
	int is_user;
	int pos;
	int pause;
}cmd_params;

struct rescmd_info {
	const char *name;
	int alias;
	int id;
};
extern const struct rescmd_info reserved_cmds[];

char * append_selected_files(FileView *view, char *expanded, int under_cursor);
char * edit_cmd_selection(FileView *view);
void initialize_command_struct(cmd_params *cmd);
int select_files_in_range(FileView *view, cmd_params *cmd);
char ** dispatch_line(const char *args, int *count);
int command_is_reserved(const char *name);
int line_pos(const char *begin, const char *end, char sep, int regexp);
int parse_command(FileView *view, char *command, cmd_params *cmd);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
