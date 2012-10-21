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

#include "status.h"
#include "utils/macros.h"
#include "utils/test_helpers.h"
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

void init_commands(void);
/* Executes one or more commands separated by a bar.  Returns zero on success if
 * no message should be save in the status bar, positive value to save message
 * on successful execution and negative value in case of error with error
 * message. */
int exec_commands(const char cmd[], FileView *view, int type);
int exec_command(const char cmd[], FileView *view, int type);
char * find_last_command(char *cmd);
void comm_quit(int write_info, int force);
void comm_only(void);
void comm_split(SPLIT orientation);
void save_command_history(const char *command);
void save_search_history(const char *pattern);
void save_prompt_history(const char *line);
void exec_startup_commands(int argc, char **argv);
/* Allocates memory, that should be freed by the caller. */
char * cmds_expand_envvars(const char *str);

#ifdef TEST
#include "engine/cmds.h"
#endif
TSTATIC_DEFS(
	int line_pos(const char begin[], const char end[], char sep, int regexp);
	void select_range(int id, const cmd_info_t *cmd_info);
	char * eval_echo(const char args[], const char **stop_ptr);
)

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
