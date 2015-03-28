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

#include "ui/ui.h"
#include "utils/macros.h"
#include "utils/test_helpers.h"
#include "status.h"

/* Kinds of command-line alike input. */
typedef enum
{
	CIT_COMMAND,           /* Regular command-line command. */
	CIT_MENU_COMMAND,      /* Menu command-line command. */
	CIT_FSEARCH_PATTERN,   /* Forward search in normal mode. */
	CIT_BSEARCH_PATTERN,   /* Backward search in normal mode. */
	CIT_VFSEARCH_PATTERN,  /* Forward search in visual mode. */
	CIT_VBSEARCH_PATTERN,  /* Backward search in visual mode. */
	CIT_VWFSEARCH_PATTERN, /* Forward search in view mode. */
	CIT_VWBSEARCH_PATTERN, /* Backward search in view mode. */
	CIT_FILTER_PATTERN,    /* Filter value. */
	CIT_PROMPT_INPUT,      /* Response to an arbitrary input request. */
}
CmdInputType;

/* Type of location on the command-line string. */
typedef enum
{
	CLL_OUT_OF_ARG, /* Not inside an argument. */
	CLL_NO_QUOTING, /* Inside escaped argument. */
	CLL_S_QUOTING,  /* Inside single quoted argument. */
	CLL_D_QUOTING,  /* Inside double quoted argument. */
	CLL_R_QUOTING,  /* Inside regexp quoted argument. */
}
CmdLineLocation;

void init_commands(void);

/* Executes one or more commands separated by a bar.  Returns zero on success if
 * no message should be saved in the status bar, positive value to save message
 * on successful execution and negative value in case of error with error
 * message. */
int exec_commands(const char cmd[], FileView *view, CmdInputType type);

/* Executes command of specified kind.  Returns zero on success if no message
 * should be saved in the status bar, positive value to save message on
 * successful execution and negative value in case of error with error
 * message. */
int exec_command(const char cmd[], FileView *view, CmdInputType type);

/* Should precede new command execution scope (e.g. before start of sourced
 * script). */
void commands_scope_start(void);

/* Should terminate command execution scope (e.g. end of sourced script).
 * Performs some of internal checks.  Returns non-zero when there were errors,
 * otherwise zero is returned. */
int commands_scope_finish(void);

/* Find start of the last command in pipe-separated list of command-line
 * commands.  Accounts for pipe escaping.  Returns pointer to start of the last
 * command. */
const char * find_last_command(const char cmds[]);

/* Expands all environment variables in the str.  Allocates and returns memory
 * that should be freed by the caller. */
char * cmds_expand_envvars(const char str[]);

/* Opens the editor with the line at given column, gets entered command and
 * executes it in the way dependent on the type of command. */
void get_and_execute_command(const char line[], size_t line_pos,
		CmdInputType type);

/* Opens the editor with the beginning at the line_pos column.  Type is used to
 * provide useful context.  On success returns entered command as a newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
char * get_ext_command(const char beginning[], size_t line_pos,
		CmdInputType type);

/* Checks whether command should be stored in command-line history.  Returns
 * non-zero if it should be stored, otherwise zero is returned. */
int is_history_command(const char command[]);

/* Checks whether command accepts exception as its argument(s).  Returns
 * non-zero if so, otherwise zero is returned. */
int command_accepts_expr(int cmd_id);

/* Analyzes command line at given position and escapes str accordingly.  Returns
 * escaped string or NULL when no escaping is needed. */
char * commands_escape_for_insertion(const char cmd_line[], int pos,
		const char str[]);

/* Analyzes position on the command-line.  pos should point to some position of
 * cmd.  Returns where current position in the command line is. */
CmdLineLocation get_cmdline_location(const char cmd[], const char pos[]);

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
