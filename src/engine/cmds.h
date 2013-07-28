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

enum
{
	CMDS_ERR_LOOP = -1,
	CMDS_ERR_NO_MEM = -2,
	CMDS_ERR_TOO_FEW_ARGS = -3,
	CMDS_ERR_TRAILING_CHARS = -4,
	CMDS_ERR_INCORRECT_NAME = -5,
	CMDS_ERR_NEED_BANG = -6,
	CMDS_ERR_NO_BUILTIN_REDEFINE = -7,
	CMDS_ERR_INVALID_CMD = -8,
	CMDS_ERR_NO_BANG_ALLOWED = -9,
	CMDS_ERR_NO_RANGE_ALLOWED = -10,
	CMDS_ERR_NO_QMARK_ALLOWED = -11,
	CMDS_ERR_INVALID_RANGE = -12,
	CMDS_ERR_NO_SUCH_UDF = -13,
	CMDS_ERR_UDF_IS_AMBIGUOUS = -14,
	CMDS_ERR_ZERO_COUNT = -15,
	CMDS_ERR_INVALID_ARG = -16,
	CMDS_ERR_CUSTOM = -1000,
};

enum
{
	/* Commands with ids in range [NO_COMPLETION_BOUNDARY; 0) not completed. */
	NO_COMPLETION_BOUNDARY = -127,
	USER_CMD_ID = -128,
	COMCLEAR_CMD_ID = -129,
	COMMAND_CMD_ID = -130,
	DELCOMMAND_CMD_ID = -131,
	NOT_DEF = -8192,
};

typedef struct
{
	int begin, end; /* range */
	int emark, qmark, bg;
	char sep;
	int usr1, usr2;

	char *raw_args, *args;
	int argc;
	char **argv;

	const char *cmd; /* for user defined commands */
}cmd_info_t;

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
}cmd_add_t;

typedef struct
{
	void *inner; /* should be NULL on first call of init_cmds() */

	int begin;
	int current;
	int end;

	int (*complete_args)(int id, const char args[], int argc, char *argv[],
			int arg_pos);
	int (*swap_range)(void);
	int (*resolve_mark)(char mark); /* should return value < 0 on error */
	/* should allocate memory */
	char *(*expand_macros)(const char str[], int for_shell, int *usr1, int *usr2);
	/* should allocate memory */
	char *(*expand_envvars)(const char str[]);
	void (*post)(int id); /* called after successful processing command */
	void (*select_range)(int id, const cmd_info_t *cmd_info);
	/* should return < 0 to do nothing, x to skip command name and x chars */
	int (*skip_at_beginning)(int id, const char args[]);
}cmds_conf_t;

/* cmds_conf_t should be filled before calling this function */
void init_cmds(int udf, cmds_conf_t *cmds_conf);

void reset_cmds(void);

/* Returns one of CMDS_ERR_* codes or code returned by command handler. */
int execute_cmd(const char cmd[]);

/* Returns -1 on error and USER_CMD_ID for user defined commands. */
int get_cmd_id(const char cmd[]);

/* Returns command id */
int get_cmd_info(const char cmd[], cmd_info_t *info);

/* Returns offset in cmd, where completion elements should be pasted. */
int complete_cmd(const char cmd[]);

void add_builtin_commands(const cmd_add_t *cmds, int count);

/* Returns pointer to the first character of the last argument in cmd. */
char * get_last_argument(const char cmd[], size_t *len);

/* Last element is followed by a NULL */
char ** list_udf(void);

char * list_udf_content(const char beginning[]);

TSTATIC_DEFS(
	int add_builtin_cmd(const char name[], int abbr, const cmd_add_t *conf);
	char ** dispatch_line(const char args[], int *count, char sep, int regexp,
			int quotes, int *last_arg, int *last_begin, int *last_end);
	void unescape(char s[], int regexp);
)

#endif /* VIFM__ENGINE__CMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
