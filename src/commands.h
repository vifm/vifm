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

#ifndef VIFM__COMMANDS_H__
#define VIFM__COMMANDS_H__

#include <stddef.h> /* size_t */

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
	GET_FILTER_PATTERN,
	GET_PROMPT_INPUT,
};

void init_commands(void);

/* Executes one or more commands separated by a bar.  Returns zero on success if
 * no message should be saved in the status bar, positive value to save message
 * on successful execution and negative value in case of error with error
 * message. */
int exec_commands(const char cmd[], FileView *view, int type);

/* Executes command of specified kind.  Returns zero on success if no message
 * should be saved in the status bar, positive value to save message on
 * successful execution and negative value in case of error with error
 * message. */
int exec_command(const char cmd[], FileView *view, int type);

/* An event like function, which should be used to inform commands unit that
 * set of a command has come to its end.  Allows for performing some of internal
 * checks.  Returns non-zero when there were errors, otherwise zero is
 * returned. */
int commands_block_finished(void);

char * find_last_command(char *cmd);

void comm_quit(int write_info, int force);

void exec_startup_commands(int argc, char **argv);

/* Expands all environment variables in the str.  Allocates and returns memory
 * that should be freed by the caller. */
char * cmds_expand_envvars(const char str[]);

/* Opens the editor with the line at given column, gets entered command and
 * executes it in the way dependent on the type of command. */
void get_and_execute_command(const char line[], size_t line_pos, int type);

/* Opens the editor with the beginning at the line_pos column.  Type is used to
 * provide useful context.  On success returns entered command as a newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
char * get_ext_command(const char beginning[], size_t line_pos, int type);

/* Checks whether command should be stored in command-line history.  Returns
 * non-zero if it should be stored, otherwise zero is returned. */
int is_history_command(const char command[]);

#ifdef TEST
#include "engine/cmds.h"
#endif
TSTATIC_DEFS(
	int line_pos(const char begin[], const char end[], char sep, int regexp);
	void select_range(int id, const cmd_info_t *cmd_info);
	char * eval_arglist(const char args[], const char **stop_ptr);
)

#endif /* VIFM__COMMANDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
