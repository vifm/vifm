/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2012 xaizek.
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

#ifndef __COMMANDS_COMPLETION_H__
#define __COMMANDS_COMPLETION_H__

/* identifiers for commands with completion */
enum
{
	COM_CD,
	COM_CHOWN,
	COM_COLORSCHEME,
	COM_EDIT,
	COM_EXECUTE,
	COM_FILE,
	COM_FIND,
	COM_GOTO,
	COM_GREP,
	COM_HELP,
	COM_HIGHLIGHT,
	COM_HISTORY,
	COM_PUSHD,
	COM_SET,
	COM_SOURCE,
	COM_SYNC,
	COM_LET,
	COM_UNLET,
	COM_WINDO,
	COM_WINRUN,
	COM_TOUCH,
	COM_CLONE,
	COM_COPY,
	COM_MOVE,
	COM_ALINK,
	COM_RLINK,
	COM_MKDIR,
};

/* values of type argument for filename_completion() function */
enum
{
	FNC_ALL,      /* all files and directories */
	FNC_ALL_WOS,  /* all files and directories without trailing slash */
	FNC_ALL_WOE,  /* all files and directories without escaping */
	FNC_FILE_WOE, /* only files in the current directory without escaping */
	FNC_DIRONLY,  /* only directories */
	FNC_EXECONLY, /* only executable files */
	FNC_DIREXEC   /* directories and executable files */
};

void init_commands_completion(void);
int complete_args(int id, const char *args, int argc, char **argv, int arg_pos);
char * fast_run_complete(const char *cmd);
void exec_completion(const char *str);
void filename_completion(const char *str, int type);
void complete_user_name(const char *str);
void complete_group_name(const char *str);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
