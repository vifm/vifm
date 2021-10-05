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

/* Error codes returned by vle_cmds_run(). */
enum
{
	/* Error codes are negative integers. */
	CMDS_ERR_LOOP = -128,         /* Too deep recursion of commands. */
	CMDS_ERR_NO_MEM,              /* Not enough memory. */
	CMDS_ERR_TOO_FEW_ARGS,        /* Not enough arguments. */
	CMDS_ERR_TRAILING_CHARS,      /* Too many arguments. */
	CMDS_ERR_INCORRECT_NAME,      /* Bad name for a user-defined command. */
	CMDS_ERR_NEED_BANG,           /* Need enforcing to succeed. */
	CMDS_ERR_NO_BUILTIN_REDEFINE, /* Can't shadow builtin command. */
	CMDS_ERR_INVALID_CMD,         /* Unknown command name. */
	CMDS_ERR_NO_BANG_ALLOWED,     /* The command doesn't support "!". */
	CMDS_ERR_NO_RANGE_ALLOWED,    /* The command doesn't take range. */
	CMDS_ERR_NO_QMARK_ALLOWED,    /* The command doesn't support "?". */
	CMDS_ERR_INVALID_RANGE,       /* Bad range. */
	CMDS_ERR_NO_SUCH_UDF,         /* Unknown name of a user-defined command. */
	CMDS_ERR_UDF_IS_AMBIGUOUS,    /* Calling user-defined command is ambiguous. */
	CMDS_ERR_INVALID_ARG,         /* Missing closing quote. */
	CMDS_ERR_CUSTOM,              /* Error defined by the client. */
};

/* Constants related to command ids. */
enum
{
	/* Builtin commands have negative ids. */
	USER_CMD_ID = -256, /* <USERCMD> */
	COMCLEAR_CMD_ID,    /* :comc[lear] */
	COMMAND_CMD_ID,     /* :com[mand] */
	DELCOMMAND_CMD_ID,  /* :delc[ommand] */

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
	FOREIGN_CMD,  /* Externally registered builtin-like command. */
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

	/* For user defined commands. */
	const char *user_cmd;    /* Name of user defined command. */
	const char *user_action; /* Body of user defined command. */

	void *user_data; /* User data associated with the command or NULL. */
}
cmd_info_t;

/* Type of command handler.  Shouldn't return negative numbers unless it's one
 * of CMDS_ERR_* constants.  Either way the return value will be the return
 * value of vle_cmds_run() function. */
typedef int (*cmd_handler)(const cmd_info_t *cmd_info);

/* Description of a command or an abbreviation. */
typedef struct cmd_t
{
	char *name;        /* Name of the command. */
	const char *descr; /* Brief description of the command. */
	int id;            /* Numeric identifier, positive, USER_CMD_ID or -1. */
	CMD_TYPE type;     /* Type of command described by this structure. */
	int passed;        /* Number of times this command was recursively called. */

	void *user_data;     /* User data associated with the command or NULL. */
	cmd_handler handler; /* Handler for builtin commands. */
	char *cmd;           /* Command-line for user-defined commands. */

	int min_args, max_args; /* Min and max number of arguments, can be NOT_DEF. */

	unsigned int deleted : 1;          /* Whether this command was deleted. */
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

	/* HAS_RAW_ARGS flag can't be combined with either of the other two, but those
	 * two can be specified at the same time. */
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

/* New commands specification for vle_cmds_add(). */
typedef struct
{
	const char *name;       /* Full command name. */
	const char *abbr;       /* Command prefix (can be NULL). */
	const char *descr;      /* Brief description (stored as a pointer). */
	void *user_data;        /* User data for the handler or NULL. */
	int id;                 /* Command id.  Doesn't need to be unique.  Negative
	                           value means absence of arg completion.  Use, for
	                           example, -1 for all commands without completion. */
	cmd_handler handler;    /* Function invoked to run the command. */
	int min_args, max_args; /* Minimum and maximum bounds on number of args. */
	int flags;              /* Set of HAS_* flags. */
}
cmd_add_t;

/* Configuration structure that's passed in from the outside. */
typedef struct
{
	void *inner; /* Should be NULL on first call of vle_cmds_init().  This is a
	                pointer to internal data. */

	int begin;   /* The lowest valid number of the range. */
	int current; /* Current position between [begin; end]. */
	int end;     /* The highest valid number of the range. */

	/* Whole line completion function.  arg is a user supplied value, which is
	 * passed through.  The functions should return completion offset. */
	int (*complete_line)(const char cmd_line[], void *arg);

	/* Argument completion function.  arg is a user supplied value, which is
	 * passed through.  The functions must not modify any strings passed to it.
	 * The functions should return completion offset. */
	int (*complete_args)(int id, const cmd_info_t *cmd_info, int arg_pos,
			void *arg);

	/* Asks user whether bounds of an inverted range should be swapped.  Should
	 * return non-zero if so and zero otherwise. */
	int (*swap_range)(void);
	/* Resolves name of the mark to a position.  Should return corresponding
	 * position or value < 0 on error. */
	int (*resolve_mark)(char mark);
	/* Expands macros in the passed string.  Should return newly allocated
	 * memory. */
	char * (*expand_macros)(const char str[], int for_shell, int *usr1,
			int *usr2);
	/* Expands environment variables in the passed string.  Should return newly
	 * allocated memory. */
	char * (*expand_envvars)(const char str[]);
	/* Called after successful processing of a command. */
	void (*post)(int id);
	/* Called for commands with HAS_SELECTION_SCOPE flag. */
	void (*select_range)(int id, const cmd_info_t *cmd_info);
	/* Called to determine whether a command at the front should be skipped for
	 * the purposes of completion.  Should return < 0 to do nothing, x to skip
	 * command name and x chars. */
	int (*skip_at_beginning)(int id, const char args[]);
}
cmds_conf_t;

/* Initializes previously uninitialized instance of the unit and sets it as the
 * current one.  The udf argument specifies whether user-defined commands are
 * allowed.  The structure pointed to by cmds_conf_t should be filled before
 * calling this function. */
void vle_cmds_init(int udf, cmds_conf_t *cmds_conf);

/* Resets state of the unit. */
void vle_cmds_reset(void);

/* Clears (partially resets) state of the unit. */
void vle_cmds_clear(void);

/* Executes a command.  Returns one of CMDS_ERR_* codes or code returned by the
 * command handler. */
int vle_cmds_run(const char cmd[]);

/* Parses command to fetch command and retrieve id associated with it.  Returns
 * the id, -1 on error and USER_CMD_ID for all user defined commands. */
int vle_cmds_identify(const char cmd[]);

/* Parses cmd to find beginning of arguments.  Returns pointer within the cmd or
 * NULL if command is unknown or command-line is invalid. */
const char * vle_cmds_args(const char cmd[]);

/* Breaks down passed command into its constituent parts.  Returns pointer to
 * command's description or NULL on error. */
const cmd_t * vle_cmds_parse(const char cmd[], cmd_info_t *info);

/* Performs completion of the command, either just command name or arguments of
 * some command.  Returns offset in cmd, where completion elements should be
 * pasted. */
int vle_cmds_complete(const char cmd[], void *arg);

/* Registers all commands in the array pointed to by cmds of length at least
 * count. */
void vle_cmds_add(const cmd_add_t cmds[], int count);

/* Registers a foreign builtin-like command.  Returns non-zero on error,
 * otherwise zero is returned. */
int vle_cmds_add_foreign(const cmd_add_t *cmd);

/* Adds a new or updates an existing user command.  Non-zero overwrite parameter
 * enables updating of existing command.  Returns error code or zero on
 * success. */
int vle_cmds_add_user(const char name[], const char body[], const char descr[],
		int overwrite);

/* Removes a user command if one exists.  Returns error code or zero on
 * success. */
int vle_cmds_del_user(const char name[]);

/* Finds the first character of the last argument in cmd.  Returns pointer to
 * it. */
char * vle_cmds_last_arg(const char cmd[], int quotes, size_t *len);

/* Lists user-defined commands as name-value pairs each in separate item of the
 * array.  Last pair element is followed by a NULL. */
char ** vle_cmds_list_udcs(void);

/* Prints a table that includes commands that start with the given prefix into
 * a multiline string (all lines except for the last one has new line
 * character).  Returns the string or NULL if there are no command with that
 * prefix. */
char * vle_cmds_print_udcs(const char beginning[]);

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
	int add_builtin_cmd(const char name[], CMD_TYPE type, const cmd_add_t *conf);
	char ** dispatch_line(const char args[], int *count, char sep, int regexp,
			int quotes, int noescaping, int comments, int *last_arg,
			int (**positions)[2]);
)

#endif /* VIFM__ENGINE__CMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
