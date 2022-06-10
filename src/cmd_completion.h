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

#ifndef VIFM__CMD_COMPLETION_H__
#define VIFM__CMD_COMPLETION_H__

#include <stddef.h> /* size_t */

/* Identifiers for commands with completion.  Some of them are not directly used
 * in code, their purpose is to provide id so that completion is invoked for the
 * command (such commands are handled in the else clause). */
enum
{
	COM_ALINK,
	COM_AUTOCMD,
	COM_BMARKS,
	COM_CABBR,
	COM_CD,
	COM_CHOWN,
	COM_CLONE,
	COM_COLORSCHEME,
	COM_COMPARE,
	COM_COPY,
	COM_DELBMARKS,
	COM_DELSESSION,
	COM_ECHO,
	COM_EDIT,
	COM_ELSEIF_STMT,
	COM_EXE,
	COM_EXECUTE,
	COM_FILE,
	COM_FIND,
	COM_GOTO,
	COM_GOTO_PATH,
	COM_GREP,
	COM_HELP,
	COM_HIGHLIGHT,
	COM_HISTORY,
	COM_IF_STMT,
	COM_INVERT,
	COM_KEEPSEL,
	COM_LET,
	COM_MKDIR,
	COM_MOVE,
	COM_PLUGIN,
	COM_PUSHD,
	COM_RENAME,
	COM_RLINK,
	COM_SELECT,
	COM_SESSION,
	COM_SET,
	COM_SETLOCAL,
	COM_SOURCE,
	COM_SPLIT,
	COM_SYNC,
	COM_TABNEW,
	COM_TOUCH,
	COM_TREE,
	COM_UNLET,
	COM_VSPLIT,
	COM_WINCMD,
	COM_WINDO,
	COM_WINRUN,

	COM_MENU_WRITE,

	/* Single ID for all foreign commands, which are disambiguated by user
	 * data. */
	COM_FOREIGN,
};

/* Values of type argument for filename_completion() function. */
typedef enum
{
	CT_ALL,      /* All files and directories. */
	CT_ALL_WOS,  /* All files and directories without trailing slash. */
	CT_ALL_WOE,  /* All files and directories without escaping. */
	CT_FILE,     /* Only files in the current directory. */
	CT_FILE_WOE, /* Only files in the current directory without escaping. */
	CT_DIRONLY,  /* Only directories. */
	CT_EXECONLY, /* Only executable files. */
	CT_DIREXEC   /* Directories and executable files. */
}
CompletionType;

/* Possible values for the extra argument of complete_args(). */
typedef enum
{
	CPP_NONE,             /* Do not pre-process input completion arg. */
	CPP_PERCENT_UNESCAPE, /* Replace "%%" with "%" in the arg. */
	CPP_SQUOTES_UNESCAPE, /* Perform single quotes expansion on the arg. */
	CPP_DQUOTES_UNESCAPE, /* Perform double quotes expansion on the arg. */
}
CompletionPreProcessing;

struct cmd_info_t;

/* Completes whole command-line.  Returns completion offset. */
int complete_line(const char cmd_line[], void *extra_arg);

/* Completes arguments of a command.  Returns completion offset. */
int complete_args(int id, const struct cmd_info_t *cmd_info, int arg_pos,
		void *extra_arg);

/* Completes name of an executable after extracting it from the cmd.  Returns
 * NULL and sets status bar error message when command is ambiguous, otherwise
 * newly allocated string, which should be returned by caller, is returned. */
char * fast_run_complete(const char cmd[]);

/* Completes file names in a requested manner.  If skip_canonicalization is set,
 * "../" will be resolved to actual parent directories of target files.  Returns
 * completion start offset. */
int filename_completion(const char str[], CompletionType type,
		int skip_canonicalization);

/* Completes expressions.  Sets *start to position at which completion
 * happens. */
void complete_expr(const char str[], const char **start);

void complete_user_name(const char *str);

void complete_group_name(const char *str);

/* Checks whether program with given name is an executable that present in the
 * $PATH environment variable or can be found by full path. */
int external_command_exists(const char cmd[]);

/* Gets path to an executable expanding command name using $PATH if needed.
 * Might not include extension of a command on Windows.  Returns zero on
 * success, otherwise non-zero is returned. */
int get_cmd_path(const char cmd[], size_t path_len, char path[]);

#endif /* VIFM__CMD_COMPLETION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
