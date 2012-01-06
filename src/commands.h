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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "macros.h"
#include "ui.h"

enum
{
	GET_COMMAND,
	GET_MENU_COMMAND,
	GET_FSEARCH_PATTERN,
	GET_BSEARCH_PATTERN,
	GET_VFSEARCH_PATTERN,
	GET_VBSEARCH_PATTERN,
	GET_VWFSEARCH_PATTERN,
	GET_VWBSEARCH_PATTERN,
};

/* values of type argument for filename_completion() function */
enum
{
	FNC_ALL,      /* all files and directories */
	FNC_ALL_WOE,  /* all files and directories without escaping */
	FNC_FILE_WOE, /* only files in the current directory without escaping */
	FNC_DIRONLY,  /* only directories */
	FNC_EXECONLY, /* only executable files */
	FNC_DIREXEC   /* directories and executable files */
};

void init_commands(void);
int exec_commands(char *cmd, FileView *view, int save_hist, int type);
int exec_command(char *cmd, FileView *view, int type);
char * find_last_command(char *cmd);
int shellout(const char *command, int pause, int allow_screen);
char * fast_run_complete(char *cmd);
char * expand_macros(FileView *view, const char *command, const char *args,
		int *menu, int *split);
void comm_quit(int write_info, int force);
void comm_only(void);
void comm_split(int vertical);
void save_command_history(const char *command);
void save_search_history(const char *pattern);
void save_prompt_history(const char *line);
char * edit_selection(FileView *view, int *bg);
void complete_user_name(const char *str);
void complete_group_name(const char *str);
void filename_completion(const char *str, int type);
void exec_startup_commands(int argc, char **argv);

#ifdef TEST
#include "cmds.h"

char * append_selected_files(FileView *view, char *expanded, int under_cursor,
		int quotes, const char *mod);
int line_pos(const char *begin, const char *end, char sep, int regexp);
void select_range(int id, const cmd_info_t *cmd_info);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
