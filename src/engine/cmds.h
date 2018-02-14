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

/* Constants related to command ids. */
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

/* Command type. */
typedef enum
{
	BUILTIN_ABBR, /* Abbreviated version of a builtin command (like `:com`). */
	BUILTIN_CMD,  /* Builtin command. */
	USER_CMD,     /* User-defined command. */
}
CMD_TYPE;

/* Detailed information resulted from command parsing, which is passed to
 * command handler (cmd_handler). */
typedef struct cmd_info_t
{
	int begin, end; /* Parsed range of the command.  They are either valid and
	                   define range with both boundaries included or equal to
	                   NOT_DEF if no range was given. */
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

/* Type of command handler.  Shouldn't return negative numbers unless it's one
 * of CMDS_ERR_* constants.  Either way the return value will be the return
 * value of execute_cmd() function. */
typedef int (*cmd_handler)(const cmd_info_t *cmd_info);

/* Description of a command or an abbreviation. */
typedef struct cmd_t
{
	char *name;        /* Name of the command. */
	const char *descr; /* Brief description of the command. */
	int id;            /* Numeric identifier, positive, USER_CMD_ID or -1. */
	CMD_TYPE type;     /* Type of command described by this structure. */
	int passed;        /* Number of times this command was recursively called. */

	cmd_handler handler; /* Handler for builtin commands. */
	char *cmd;           /* Command-line for user-defined commands. */

	int min_args, max_args; /* Min and max number of arguments, can be NOT_DEF. */

	unsigned int range : 1;            /* Handles ranges. */
	unsigned int cust_sep : 1;         /* Custom separator of arguments. */
	unsigned int emark : 1;            /* Supports emark flag. */
	unsigned int envvars : 1;          /* Expand environment variables. */
	unsigned int select : 1;           /* Select files in a range. */
	unsigned int bg : 1;               /* Bg (can have " &" at the end). */
	unsigned int noescaping : 1;       /* Don't process \-escaping in unquoted
	                                      args. */
	unsigned int regexp : 1;           /* Process /.../-arguments. */
	unsigned int quote : 1;            /* Process '- and "-quoted args. */
	unsigned int comment : 1;          /* Trailing comment is allowed. */
	unsigned int qmark : 1;            /* No args after qmark. */
	unsigned int args_after_qmark : 1; /* Args after qmark are allowed. */
	unsigned int macros_for_cmd : 1;   /* Expand macros w/o special escaping. */
	unsigned int macros_for_shell : 1; /* Expand macros with shell escaping. */
	unsigned int : 0;                  /* Padding. */

	struct cmd_t *next; /* Pointer to the next structure or NULL. */
}
cmd_t;

/* Possible flags for cmd_add_t::flags field. */
enum
{
	HAS_RANGE            = 0x0001, /* Handles ranges. */
	HAS_CUST_SEP         = 0x0002, /* Custom separator of arguments. */
	HAS_EMARK            = 0x0004, /* Supports emark flag. */
	HAS_ENVVARS          = 0x0008, /* Expand environment variables. */
	HAS_SELECTION_SCOPE  = 0x0010, /* Select files in a range. */
	HAS_BG_FLAG          = 0x0020, /* Background (can have " &" at the end). */
	HAS_COMMENT          = 0x0040, /* Trailing comment is allowed. */

	/* HAS_RAW_ARGS flag can't be combined with the other two, but they can be
	 * specified at the same time. */
	HAS_RAW_ARGS         = 0x0080, /* No special processing of arguments. */
	HAS_REGEXP_ARGS      = 0x0100, /* Process /.../-arguments. */
	HAS_QUOTED_ARGS      = 0x0200, /* Process '- and "-quoted args. */

	/* Must be at most one of these. */
	HAS_QMARK_NO_ARGS    = 0x0400, /* No args after qmark. */
	HAS_QMARK_WITH_ARGS  = 0x0800, /* Args after qmark are allowed. */

	/* Must be at most one of these. */
	HAS_MACROS_FOR_CMD   = 0x1000, /* Expand macros without special escaping. */
	HAS_MACROS_FOR_SHELL = 0x2000, /* Expand macros with shell escaping. */
};

/* New commands specification for add_builtin_commands(). */
typedef struct
{
	const char *name;        /* Full command name. */
	const char *abbr;        /* Command prefix (can be NULL). */
	const char *descr;       /* Brief description (stored as a pointer). */
	int id;                  /* Command id.  Doesn't need to be unique.  Negative
	                            value means absence of arg completion.  Use, for
	                            example, -1 for all commands without
	                            completion. */
	cmd_handler handler;     /* Function invoked to run the command. */
	int min_args, max_args;  /* Minimum and maximum bounds on number of args. */
	int flags;               /* Set of HAS_* flags. */
}
cmd_add_t;

typedef struct
{
	void *inner; /* Should be NULL on first call of init_cmds(). */

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

/* Breaks down passed command into its constituent parts.  Returns pointer to
 * command's description or NULL on error. */
const cmd_t * get_cmd_info(const char cmd[], cmd_info_t *info);

/* Returns offset in cmd, where completion elements should be pasted. */
int complete_cmd(const char cmd[], void *arg);

/* Registers all commands in the array pointed to by cmds of length at least
 * count. */
void add_builtin_commands(const cmd_add_t cmds[], int count);

/* Returns pointer to the first character of the last argument in cmd. */
char * get_last_argument(const char cmd[], int quotes, size_t *len);

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
			int quotes, int noescaping, int comments, int *last_arg,
			int (**positions)[2]);
)

#endif /* VIFM__ENGINE__CMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
