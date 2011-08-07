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
	GET_VFSEARCH_PATTERN,
	GET_VBSEARCH_PATTERN,
};

void init_commands(void);
int exec_commands(char *cmd, FileView *view, int save_hist);
int exec_command(char *cmd, FileView *view, int type);
char * find_last_command(char *cmd);
int shellout(const char *command, int pause);
int execute_command(FileView *view, char *action);
char * fast_run_complete(char *cmd);
char * expand_macros(FileView *view, const char *command, const char *args,
		int *menu, int *split);
void _gnuc_noreturn comm_quit(int write_info);
void comm_only(void);
void comm_split(void);
void save_command_history(const char *command);
void save_search_history(const char *pattern);
char * edit_selection(FileView *view);

#ifdef TEST
char * append_selected_files(FileView *view, char *expanded, int under_cursor);
int line_pos(const char *begin, const char *end, char sep, int regexp);
void select_range(const struct cmd_info *cmd_info);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
