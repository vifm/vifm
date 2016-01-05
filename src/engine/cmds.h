/* vifm
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

#ifndef VIFM__ENGINE__CMDS_H__
#define VIFM__ENGINE__CMDS_H__

#include <stddef.h> /* size_t */

#include "../utils/test_helpers.h"

/* Error codes. */
enum
{
	/* Error codes are negative integers. */
	CMDS_ERR_LOOP = -128,
	CMDS_ERR_NO_MEM,
	CMDS_ERR_TOO_FEW_ARGS,
	CMDS_ERR_TRAILING_CHARS,
	CMDS_ERR_INCORRECT_NAME,
	CMDS_ERR_NEED_BANG,
	CMDS_ERR_NO_BUILTIN_REDEFINE,
	CMDS_ERR_INVALID_CMD,
	CMDS_ERR_NO_BANG_ALLOWED,
	CMDS_ERR_NO_RANGE_ALLOWED,
	CMDS_ERR_NO_QMARK_ALLOWED,
	CMDS_ERR_INVALID_RANGE,
	CMDS_ERR_NO_SUCH_UDF,
	CMDS_ERR_UDF_IS_AMBIGUOUS,
	CMDS_ERR_ZERO_COUNT,
	CMDS_ERR_INVALID_ARG,
	CMDS_ERR_CUSTOM,
};

/* Constants related to Command ids. */
enum
{
	/* Builtin commands have negative ids. */
	USER_CMD_ID = -256,
	COMCLEAR_CMD_ID,
	COMMAND_CMD_ID,
	DELCOMMAND_CMD_ID,

	/* Commands with ids in range [NO_COMPLETION_BOUNDARY; 0) are not
	 * completed. */
	NO_COMPLETION_BOUNDARY,
};

/* Special values. */
enum
{
	/* Undefined maximum number of arguments for a command. */
	NOT_DEF = -8192,
};

/* Detailed information resulted from command parsing, which is passed to
 * command handler (cmd_handler). */
typedef struct cmd_info_t
{
	int begin, end; /* Parsed range of the command. */
	int count;      /* Parsed [count] of the command. */
	int emark, qmark, bg;
	char sep;
	int usr1, usr2;

	char *raw_args;  /* Arguments as they were passed in. */
	char *args;      /* Arguments after macro and envvar expansions. */
	int argc;        /* Number of arguments. */
	char **argv;     /* Values of arguments. */
	int (*argvp)[2]; /* Start/end positions of arguments in args. */

	const char *cmd; /* For user defined commands. */
}
cmd_info_t;

typedef int (*cmd_handler)(const cmd_info_t *cmd_info);

typedef struct
{
	const char *name, *abbr;
	int id; /* -1 here means that this command don't require completion of args */
	cmd_handler handler;
	int range;
	int cust_sep; /* custom separator of arguments */
	int emark;
	int qmark; /* 1 - no args after qmark, other value - args are allowed */
	int min_args, max_args;
	/* 0x01 - expand macros, 0x02 - expand envvars, 0x04 - expand for shell */
	int expand;
	int regexp;
	int select; /* select files in range */
	int bg; /* background */
	int quote; /* whether need to take care of single and double quotes in args */
	int comment; /* Whether trailing comment is allowed for the command. */
}
cmd_add_t;

typedef struct
{
	void *inner; /* should be NULL on first call of init_cmds() */

	int begin;   /* The lowest valid number of the range. */
	int current; /* Current position between [begin; end]. */
	int end;     /* The highest valid number of the range. */

	/* Argument completion function.  arg is user supplied value, which is passed
	 * through.  The functions must not modify any strings passed to it. */
	int (*complete_args)(int id, const cmd_info_t *cmd_info, int arg_pos,
			void *arg);

	int (*swap_range)(void);
	int (*resolve_mark)(char mark); /* should return value < 0 on error */
	/* Should allocate memory. */
	char *(*expand_macros)(const char str[], int for_shell, int *usr1, int *usr2);
	/* Should allocate memory. */
	char *(*expand_envvars)(const char str[]);
	void (*post)(int id); /* called after successful processing command */
	void (*select_range)(int id, const cmd_info_t *cmd_info);
	/* should return < 0 to do nothing, x to skip command name and x chars */
	int (*skip_at_beginning)(int id, const char args[]);
}
cmds_conf_t;

/* cmds_conf_t should be filled before calling this function */
void init_cmds(int udf, cmds_conf_t *cmds_conf);

void reset_cmds(void);

/* Returns one of CMDS_ERR_* codes or code returned by command handler. */
int execute_cmd(const char cmd[]);

/* Returns -1 on error and USER_CMD_ID for user defined commands. */
int get_cmd_id(const char cmd[]);

/* Parses cmd to find beginning of arguments.  Returns pointer within the cmd or
 * NULL if command is unknown or command-line is invalid. */
const char * get_cmd_args(const char cmd[]);

/* Returns command id */
int get_cmd_info(const char cmd[], cmd_info_t *info);

/* Returns offset in cmd, where completion elements should be pasted. */
int complete_cmd(const char cmd[], void *arg);

void add_builtin_commands(const cmd_add_t *cmds, int count);

/* Returns pointer to the first character of the last argument in cmd. */
char * get_last_argument(const char cmd[], size_t *len);

/* Last element is followed by a NULL */
char ** list_udf(void);

char * list_udf_content(const char beginning[]);

/* Skips at most one argument of the string.  Returns pointer to the next
 * character after that argument, if any, otherwise pointer to
 * null-character. */
char * vle_cmds_past_arg(const char args[]);

/* Skips argument separators in the string.  Returns pointer to the first
 * character of the first argument, if any, otherwise pointer to
 * null-character. */
char * vle_cmds_at_arg(const char args[]);

/* Advances to the next argument in the string.  Returns pointer to the next
 * argument, if any, otherwise pointer to null-character. */
char * vle_cmds_next_arg(const char args[]);

TSTATIC_DEFS(
	int add_builtin_cmd(const char name[], int abbr, const cmd_add_t *conf);
	char ** dispatch_line(const char args[], int *count, char sep, int regexp,
			int quotes, int comments, int *last_arg, int (**positions)[2]);
)

#endif /* VIFM__ENGINE__CMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
