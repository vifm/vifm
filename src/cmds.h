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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __CMDS_H__
#define __CMDS_H__

enum
{
	CMDS_ERR_LOOP = -1,
	CMDS_ERR_NO_MEM = -2,
	CMDS_ERR_TOO_FEW_ARGS = -3,
	CMDS_ERR_TRAILING_CHARS = -4,
	CMDS_ERR_INCORRECT_NAME = -5,
	CMDS_ERR_NEED_BANG = -6,
	CMDS_ERR_NO_BUILDIN_REDEFINE = -7,
	CMDS_ERR_INVALID_CMD = -8,
	CMDS_ERR_NO_BANG_ALLOWED = -9,
	CMDS_ERR_NO_RANGE_ALLOWED = -10,
	CMDS_ERR_NO_QMARK_ALLOWED = -11,
	CMDS_ERR_INVALID_RANGE = -12,
	CMDS_ERR_NO_SUCH_UDF = -13,
	CMDS_ERR_UDF_IS_AMBIGUOUS = -14,
	CMDS_ERR_ZERO_COUNT = -15,
	CMDS_ERR_INVALID_ARG = -16,
};

enum
{
	USER_CMD_ID = -128,
	COMCLEAR_CMD_ID = -129,
	COMMAND_CMD_ID = -130,
	DELCOMMAND_CMD_ID = -131,
	NOT_DEF = -8192,
};

struct cmd_info
{
	int begin, end; /* range */
	int emark, qmark, bg;
	char sep;

	char *raw_args, *args;
	int argc;
	char **argv;

	const char *cmd; /* for user defined commands */
};

typedef int (*cmd_handler)(const struct cmd_info *cmd_info);

struct cmd_add
{
	const char *name, *abbr;
	int id; /* -1 here means that this command don't require completion of args */
	cmd_handler handler;
	int range;
	int cust_sep; /* custom separator of arguments */
	int emark, qmark;
	int min_args, max_args;
	int expand; /* expand macros */
	int regexp;
	int select; /* select files in range */
	int bg; /* background */
	int quote; /* whether need to take care of single and double quotes in args */
};

struct {
	int begin;
	int current;
	int end;

	int (*complete_args)(int id, const char *args, int argc, char **argv,
			int arg_pos);
	int (*swap_range)(void);
	int (*resolve_mark)(char mark); /* should return value < 0 on error */
	char *(*expand_macros)(const char *str); /* should allocate memory */
	void (*post)(int id); /* called after successful processing command */
	void (*select_range)(const struct cmd_info *cmd_info);
} cmds_conf;

/* cmds_conf should be filled before calling this function */
void init_cmds(void);

void reset_cmds(void);

/* Returns one of CMDS_ERR_* codes. */
int execute_cmd(const char *cmd);

/* Returns -1 on error and USER_CMD_ID for user defined commands. */
int get_cmd_id(const char *cmd);

char get_cmd_sep(const char *cmd);

/* Returns offset in cmd, where completion elements should be pasted */
int complete_cmd(const char *cmd);

void add_buildin_commands(const struct cmd_add *cmds, int count);

/* Last element is followed by a NULL */
char ** list_udf(void);

char * list_udf_content(const char *beginning);

#ifdef TEST
int add_buildin_cmd(const char *name, int abbr, const struct cmd_add *conf);
char ** dispatch_line(const char *args, int *count, char sep, int regexp,
		int quotes, int *last_arg, int *last_end);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
