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

#include "cmds.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isalnum() isalpha() isdigit() isspace() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h>
#include <stdlib.h> /* calloc() malloc() free() realloc() */
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../utils/darray.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utils.h"
#include "completion.h"

#define MAX_CMD_RECURSION 16
#define MAX_CMD_NAME_LEN 256
#define INVALID_MARK -4096

typedef struct
{
	cmd_t head;
	cmd_add_t user_cmd_handler;
	cmd_handler command_handler;
	int custom_cmd_count;        /* Number of non-builtin commands. */
}
inner_t;

/* List of characters, which are treated as range separators. */
static const char *RANGE_SEPARATORS = ",;";

static inner_t *inner;
static cmds_conf_t *cmds_conf;

static const char * correct_limit(const char cmd[], cmd_info_t *cmd_info);
static int udf_is_ambiguous(const char name[]);
static const char * parse_tail(cmd_t *cur, const char cmd[],
		cmd_info_t *cmd_info);
static const char * get_cmd_name(const char cmd[], char buf[], size_t buf_len);
static void init_cmd_info(cmd_info_t *cmd_info);
static const char * skip_prefix_commands(const char cmd[]);
static cmd_t * find_cmd(const char name[]);
static cmd_t * find_cmd_advance(cmd_t *cmd, const char name[]);
static int find_cmd_match(cmd_t *cmd, const char name[]);
static const char * parse_range(const char cmd[], cmd_info_t *cmd_info);
static const char * parse_range_elem(const char cmd[], cmd_info_t *cmd_info,
		char last_sep);
static int complete_cmd_args(cmd_t *cur, const char args[],
		cmd_info_t *cmd_info, void *arg);
static void complete_cmd_name(const char cmd_name[], int user_only);
TSTATIC int add_builtin_cmd(const char name[], CMD_TYPE type,
		const cmd_add_t *conf);
static int is_builtin_like_name_ok(const char name[]);
static int comclear_cmd(const cmd_info_t *cmd_info);
static void remove_commands(CMD_TYPE type);
static int command_cmd(const cmd_info_t *cmd_info);
static void init_command_flags(cmd_t *cmd, int flags);
static const char * get_user_cmd_name(const char cmd[], char buf[],
		size_t buf_len);
static int is_valid_udc_name(const char name[]);
static cmd_t * insert_cmd(cmd_t *after);
static int delcommand_cmd(const cmd_info_t *cmd_info);
TSTATIC char ** dispatch_line(const char args[], int *count, char sep,
		int regexp, int quotes, int noescaping, int comments, int *last_arg,
		int (**positions)[2]);
static int is_separator(char c, char sep);

void
vle_cmds_init(int udf, cmds_conf_t *conf)
{
	static cmd_add_t commands[] = {
		{
			.name = "comclear",        .abbr = "comc", .handler = comclear_cmd,
			.id = COMCLEAR_CMD_ID,
			.descr = "remove all user-defined :commands",
			.flags = 0,
			.min_args = 0,             .max_args = 0,
		}, {
			.name = "command",         .abbr = "com",  .handler = command_cmd,
			.id = COMMAND_CMD_ID,
			.descr = "display/define user-defined :command",
			.flags = HAS_EMARK,
			.min_args = 0,             .max_args = NOT_DEF,
		}, {
			.name = "delcommand", .abbr = "delc", .handler = delcommand_cmd,
			.id = DELCOMMAND_CMD_ID,
			.descr = "undefine user-defined :command",
			.flags = HAS_EMARK,
			.min_args = 1,             .max_args = 1,
		}
	};

	cmds_conf = conf;
	inner = conf->inner;

	if(inner == NULL)
	{
		assert(conf->complete_line != NULL);
		assert(conf->complete_args != NULL);
		assert(conf->swap_range != NULL);
		assert(conf->resolve_mark != NULL);
		assert(conf->expand_macros != NULL);
		assert(conf->expand_envvars != NULL);
		assert(conf->post != NULL);
		assert(conf->select_range != NULL);
		assert(conf->skip_at_beginning != NULL);
		conf->inner = calloc(1, sizeof(inner_t));
		assert(conf->inner != NULL);
		inner = conf->inner;

		if(udf)
			vle_cmds_add(commands, ARRAY_LEN(commands));
	}
}

void
vle_cmds_reset(void)
{
	if(inner == NULL)
	{
		return;
	}

	cmd_t *cur = inner->head.next;

	while(cur != NULL)
	{
		cmd_t *next = cur->next;
		free(cur->cmd);
		free(cur->name);
		free(cur);
		cur = next;
	}

	inner->head.next = NULL;
	inner->user_cmd_handler.handler = NULL;

	free(inner);
	inner = NULL;
	cmds_conf->inner = NULL;
}

void
vle_cmds_clear(void)
{
	remove_commands(USER_CMD);
	remove_commands(FOREIGN_CMD);
}

int
vle_cmds_run(const char cmd[])
{
	cmd_info_t cmd_info;
	char cmd_name[MAX_CMD_NAME_LEN + 1];
	cmd_t *cur;
	const char *args;
	int execution_code;
	size_t last_arg_len;
	char *last_arg;
	int last_end = 0;
	cmds_conf_t *cc = cmds_conf;

	init_cmd_info(&cmd_info);
	cmd = parse_range(cmd, &cmd_info);
	if(cmd == NULL)
	{
		if(cmd_info.end == INVALID_MARK)
			return CMDS_ERR_INVALID_RANGE;
		else
			return CMDS_ERR_INVALID_CMD;
	}

	if(*cmd != '\0' && cmd_info.end < cmd_info.begin)
	{
		int t;

		if(!cc->swap_range())
			return CMDS_ERR_INVALID_RANGE;

		t = cmd_info.end;
		cmd_info.end = cmd_info.begin;
		cmd_info.begin = t;
	}

	cmd = get_cmd_name(cmd, cmd_name, sizeof(cmd_name));
	if(udf_is_ambiguous(cmd_name))
		return CMDS_ERR_UDF_IS_AMBIGUOUS;

	cur = find_cmd(cmd_name);
	if(cur == NULL)
		return CMDS_ERR_INVALID_CMD;

	args = parse_tail(cur, cmd, &cmd_info);

	cmd_info.raw_args = strdup(args);

	/* Set background flag and remove background mark from raw arguments, when
	 * command supports backgrounding. */
	last_arg = vle_cmds_last_arg(cmd_info.raw_args, cur->quote, &last_arg_len);
	if(cur->bg && *last_arg == '&' && *vle_cmds_at_arg(last_arg + 1) == '\0')
	{
		cmd_info.bg = 1;
		*last_arg = '\0';
	}

	if(cur->select)
	{
		cc->select_range(cur->id, &cmd_info);
	}

	if(cur->macros_for_cmd || cur->macros_for_shell)
	{
		cmd_info.args = cc->expand_macros(cmd_info.raw_args, cur->macros_for_shell,
				&cmd_info.usr1, &cmd_info.usr2);
	}
	if(cur->envvars)
	{
		char *const p = cmd_info.args;
		cmd_info.args = cc->expand_envvars(p ? p : cmd_info.raw_args);
		free(p);
	}
	if(cmd_info.args == NULL)
	{
		cmd_info.args = strdup(cmd_info.raw_args);
	}

	cmd_info.argv = dispatch_line(cmd_info.args, &cmd_info.argc, cmd_info.sep,
			cur->regexp, cur->quote, cur->noescaping, cur->comment, NULL,
			&cmd_info.argvp);
	if(cmd_info.argc > 0)
	{
		last_end = cmd_info.argvp[cmd_info.argc - 1][1];
	}
	cmd_info.args[last_end] = '\0';

	/* TODO: extract a method that would check all these error conditions. */
	if((cmd_info.begin != NOT_DEF || cmd_info.end != NOT_DEF) && !cur->range)
	{
		execution_code = CMDS_ERR_NO_RANGE_ALLOWED;
	}
	else if(cmd_info.argc < 0)
	{
		execution_code = CMDS_ERR_INVALID_ARG;
	}
	else if(cmd_info.emark && !cur->emark)
	{
		execution_code = CMDS_ERR_NO_BANG_ALLOWED;
	}
	else if(cmd_info.qmark && !cur->qmark)
	{
		execution_code = CMDS_ERR_NO_QMARK_ALLOWED;
	}
	else if(cmd_info.qmark && !cur->args_after_qmark && *cmd_info.args != '\0')
	{
		execution_code = CMDS_ERR_TRAILING_CHARS;
	}
	else if(cmd_info.argc < cur->min_args)
	{
		execution_code = CMDS_ERR_TOO_FEW_ARGS;
	}
	else if(cmd_info.argc > cur->max_args && cur->max_args != NOT_DEF)
	{
		execution_code = CMDS_ERR_TRAILING_CHARS;
	}
	else if(cur->passed > MAX_CMD_RECURSION)
	{
		execution_code = CMDS_ERR_LOOP;
	}
	else
	{
		++cur->passed;

		if(cur->type == USER_CMD)
		{
			cmd_info.user_cmd = cur->name;
			cmd_info.user_action = cur->cmd;
			execution_code = inner->user_cmd_handler.handler(&cmd_info);
		}
		else
		{
			cmd_info.user_data = cur->user_data;
			execution_code = cur->handler(&cmd_info);
		}

		cc->post(cur->id);
		if(--cur->passed == 0 && cur->deleted)
		{
			free(cur);
		}
	}

	free(cmd_info.raw_args);
	free(cmd_info.args);
	free_string_array(cmd_info.argv, cmd_info.argc);
	free(cmd_info.argvp);

	return execution_code;
}

/* Applies limit shifts and ensures that its value is not out of bounds.
 * Returns pointer to part of string after parsed piece. */
static const char *
correct_limit(const char cmd[], cmd_info_t *cmd_info)
{
	cmd_info->count = (cmd_info->end == NOT_DEF) ? 1 : (cmd_info->end + 1);

	while(*cmd == '+' || *cmd == '-')
	{
		int n = 1;
		int plus = *cmd == '+';
		cmd++;
		if(isdigit(*cmd))
		{
			char *p;
			n = strtol(cmd, &p, 10);
			cmd = p;
		}

		if(plus)
		{
			cmd_info->end += n;
			cmd_info->count += n;
		}
		else
		{
			cmd_info->end -= n;
			cmd_info->count -= n;
		}
	}

	if(cmd_info->end < 0)
		cmd_info->end = 0;
	if(cmd_info->end > cmds_conf->end)
		cmd_info->end = cmds_conf->end;

	return cmd;
}

static int
udf_is_ambiguous(const char name[])
{
	size_t len;
	int count;
	cmd_t *cur;

	len = strlen(name);
	count = 0;
	cur = inner->head.next;
	while(cur != NULL)
	{
		int cmp;

		cmp = strncmp(cur->name, name, len);
		if(cmp == 0)
		{
			if(cur->name[len] == '\0')
				return 0;
			if(cur->type == USER_CMD)
			{
				char c = cur->name[strlen(cur->name) - 1];
				if(c != '!' && c != '?')
					count++;
			}
		}
		else if(cmp > 0)
		{
			break;
		}

		cur = cur->next;
	}
	return (count > 1);
}

static const char *
parse_tail(cmd_t *cur, const char cmd[], cmd_info_t *cmd_info)
{
	if(*cmd == '!' && (!cur->cust_sep || cur->emark))
	{
		cmd_info->emark = 1;
		cmd++;
	}
	else if(*cmd == '?' && (!cur->cust_sep || cur->qmark))
	{
		cmd_info->qmark = 1;
		cmd++;
	}

	if(*cmd != '\0' && !isspace(*cmd))
	{
		if(cur->cust_sep)
		{
			cmd_info->sep = *cmd;
		}
		return cmd;
	}

	while(is_separator(*cmd, cmd_info->sep))
	{
		++cmd;
	}

	return cmd;
}

int
vle_cmds_identify(const char cmd[])
{
	cmd_info_t info;
	const cmd_t *const c = vle_cmds_parse(cmd, &info);
	return (c == NULL ? -1 : c->id);
}

const char *
vle_cmds_args(const char cmd[])
{
	cmd_info_t info = {};
	(void)vle_cmds_parse(cmd, &info);
	return info.raw_args;
}

/* Initializes command info structure with reasonable defaults. */
static void
init_cmd_info(cmd_info_t *cmd_info)
{
	cmd_info->begin = NOT_DEF;
	cmd_info->end = NOT_DEF;
	cmd_info->count = NOT_DEF;
	cmd_info->emark = 0;
	cmd_info->qmark = 0;
	cmd_info->raw_args = NULL;
	cmd_info->args = NULL;
	cmd_info->argc = 0;
	cmd_info->argv = NULL;
	cmd_info->argvp = NULL;
	cmd_info->user_cmd = NULL;
	cmd_info->user_action = NULL;
	cmd_info->user_data = NULL;
	cmd_info->sep = ' ';
	cmd_info->bg = 0;
	cmd_info->usr1 = 0;
	cmd_info->usr2 = 0;
}

const cmd_t *
vle_cmds_parse(const char cmd[], cmd_info_t *info)
{
	cmd_info_t cmd_info;
	char cmd_name[MAX_CMD_NAME_LEN + 1];
	cmd_t *c;

	init_cmd_info(&cmd_info);

	cmd = parse_range(cmd, &cmd_info);
	if(cmd == NULL)
	{
		return NULL;
	}

	cmd = get_cmd_name(cmd, cmd_name, sizeof(cmd_name));
	c = find_cmd(cmd_name);
	if(c == NULL)
	{
		return NULL;
	}

	cmd_info.raw_args = (char *)parse_tail(c, cmd, &cmd_info);

	*info = cmd_info;
	return c;
}

int
vle_cmds_complete(const char cmd[], void *arg)
{
	cmd_info_t cmd_info;
	const char *begin, *cmd_name_pos;
	size_t prefix_len;

	begin = cmd;
	cmd = skip_prefix_commands(begin);
	prefix_len = cmd - begin;

	init_cmd_info(&cmd_info);

	cmd_name_pos = parse_range(cmd, &cmd_info);
	if(cmd_name_pos != NULL)
	{
		char cmd_name[MAX_CMD_NAME_LEN + 1];
		const char *args;
		cmd_t *cur;

		args = get_cmd_name(cmd_name_pos, cmd_name, sizeof(cmd_name));
		cur = find_cmd(cmd_name);

		if(*args == '\0' && strcmp(cmd_name, "!") != 0)
		{
			complete_cmd_name(cmd_name, 0);

			if(vle_compl_get_count() == 0)
			{
				prefix_len += cmds_conf->complete_line(cmd_name, arg);
			}
			else
			{
				vle_compl_add_last_match(cmd_name);
				prefix_len += cmd_name_pos - cmd;
			}
		}
		else if(cur == NULL || cur->name[0] == '\0')
		{
			/* Handle empty command specially here. */
			prefix_len += cmds_conf->complete_line(cmd, arg);
		}
		else
		{
			prefix_len += args - cmd;
			cmd_info.user_data = cur->user_data;
			prefix_len += complete_cmd_args(cur, args, &cmd_info, arg);
		}
	}

	return prefix_len;
}

/* Skips prefix commands (which can be followed by an arbitrary command) at the
 * beginning of command-line. */
static const char *
skip_prefix_commands(const char cmd[])
{
	cmd_info_t cmd_info;
	const char *cmd_name_pos;

	init_cmd_info(&cmd_info);

	cmd_name_pos = parse_range(cmd, &cmd_info);
	if(cmd_name_pos != NULL)
	{
		char cmd_name[MAX_CMD_NAME_LEN + 1];
		const char *args;
		cmd_t *cur;

		args = get_cmd_name(cmd_name_pos, cmd_name, sizeof(cmd_name));
		cur = find_cmd(cmd_name);
		while(cur != NULL && *args != '\0')
		{
			int offset = cmds_conf->skip_at_beginning(cur->id, args);
			if(offset >= 0)
			{
				int delta = (args - cmd) + offset;
				cmd += delta;
				init_cmd_info(&cmd_info);
				cmd_name_pos = parse_range(cmd, &cmd_info);
				if(cmd_name_pos == NULL)
				{
					break;
				}
				args = get_cmd_name(cmd_name_pos, cmd_name, sizeof(cmd_name));
			}
			else
			{
				break;
			}
			cur = find_cmd(cmd_name);
		}
	}
	return cmd;
}

/* Looks up a command by its name. */
static cmd_t *
find_cmd(const char name[])
{
	cmd_t *const cmd = find_cmd_advance(inner->head.next, name);
	return (find_cmd_match(cmd, name) ? cmd : NULL);
}

/* Advances to the first command whose name is not less than the parameter.
 * Returns advanced values (could be unchanged or NULL). */
static cmd_t *
find_cmd_advance(cmd_t *cmd, const char name[])
{
	while(cmd != NULL && strcmp(cmd->name, name) < 0)
	{
		cmd = cmd->next;
	}
	return cmd;
}

/* Checks that command search was a success.  Returns non-zero if so, otherwise
 * zero is returned. */
static int
find_cmd_match(cmd_t *cmd, const char name[])
{
	if(cmd == NULL)
	{
		return 0;
	}

	if(name[0] == '\0')
	{
		return (cmd->name[0] == '\0');
	}

	return (strncmp(name, cmd->name, strlen(name)) == 0);
}

/* Parses whole command range (e.g. "<val>;+<val>,,-<val>").  Returns advanced
 * value of cmd when parsing is successful, otherwise NULL is returned. */
static const char *
parse_range(const char cmd[], cmd_info_t *cmd_info)
{
	char last_sep;

	cmd = vle_cmds_at_arg(cmd);

	if(isalpha(*cmd) || *cmd == '!' || *cmd == '\0')
	{
		return cmd;
	}

	last_sep = '\0';
	while(*cmd != '\0')
	{
		cmd_info->begin = cmd_info->end;

		cmd = parse_range_elem(cmd, cmd_info, last_sep);
		if(cmd == NULL)
		{
			return NULL;
		}

		cmd = correct_limit(cmd, cmd_info);

		if(cmd_info->begin == NOT_DEF)
		{
			cmd_info->begin = cmd_info->end;
		}

		cmd = vle_cmds_at_arg(cmd);

		if(!char_is_one_of(RANGE_SEPARATORS, *cmd))
		{
			break;
		}

		last_sep = *cmd;
		cmd++;

		cmd = vle_cmds_at_arg(cmd);
	}

	return cmd;
}

/* Parses single element of a command range (e.g. any of <el> in
 * "<el>;<el>,<el>").  Returns advanced value of cmd when parsing is successful,
 * otherwise NULL is returned. */
static const char *
parse_range_elem(const char cmd[], cmd_info_t *cmd_info, char last_sep)
{
	if(cmd[0] == '%')
	{
		cmd_info->begin = cmds_conf->begin;
		cmd_info->end = cmds_conf->end;
		cmd++;
	}
	else if(cmd[0] == '$')
	{
		cmd_info->end = cmds_conf->end;
		cmd++;
	}
	else if(cmd[0] == '.')
	{
		cmd_info->end = cmds_conf->current;
		cmd++;
	}
	else if(char_is_one_of(RANGE_SEPARATORS, *cmd))
	{
		cmd_info->end = cmds_conf->current;
	}
	else if(isalpha(*cmd))
	{
		cmd_info->end = cmds_conf->current;
	}
	else if(isdigit(*cmd))
	{
		char *p;
		cmd_info->end = strtol(cmd, &p, 10) - 1;
		if(cmd_info->end < cmds_conf->begin)
			cmd_info->end = cmds_conf->begin;
		cmd = p;
	}
	else if(*cmd == '\'')
	{
		char mark;
		cmd++;
		mark = *cmd++;
		cmd_info->end = cmds_conf->resolve_mark(mark);
		if(cmd_info->end < 0)
		{
			cmd_info->end = INVALID_MARK;
			return NULL;
		}
	}
	else if(*cmd == '+' || *cmd == '-')
	{
		/* Do nothing after semicolon, because in this case +/- are adjusting not
		 * base current cursor position, but base end of the range. */
		if(last_sep != ';')
		{
			cmd_info->end = cmds_conf->current;
		}
	}
	else
	{
		return NULL;
	}

	return cmd;
}

static const char *
get_cmd_name(const char cmd[], char buf[], size_t buf_len)
{
	assert(buf_len != 0 && "The buffer is expected to be of size > 0.");

	if(cmd[0] == '!')
	{
		strcpy(buf, "!");
		cmd = vle_cmds_at_arg(cmd + 1);
		return cmd;
	}

	cmd_t *c = inner->head.next;

	const char *t = cmd;
	if(isalpha(*t))
		++t;
	while(isalnum(*t))
	{
		if(isdigit(*t))
		{
			size_t len = MIN((size_t)(t - cmd), buf_len - 1);
			copy_str(buf, len + 1, cmd);

			c = find_cmd_advance(c, buf);
			if(find_cmd_match(c, buf) && c->name[len] != *t)
			{
				break;
			}
		}
		++t;
	}

	size_t len = MIN((size_t)(t - cmd), buf_len - 1);
	strncpy(buf, cmd, len);
	buf[len] = '\0';
	if(*t == '?' || *t == '!')
	{
		int cmp;
		cmd_t *cur;

		cur = inner->head.next;
		while(cur != NULL && (cmp = strncmp(cur->name, buf, len)) <= 0)
		{
			if(cmp == 0)
			{
				/* Complete match for a builtin with a custom separator. */
				if(cur->cust_sep && cur->name[len] == '\0')
				{
					strncpy(buf, cur->name, buf_len);
					break;
				}
				/* Check for user-defined command that ends with the char ('!' or
				 * '?', see above). */
				if(cur->type == USER_CMD && cur->name[strlen(cur->name) - 1] == *t)
				{
					strncpy(buf, cur->name, buf_len);
					break;
				}
				/* Or builtin abbreviation that supports the mark. */
				if(cur->type == BUILTIN_ABBR &&
						((*t == '!' && cur->emark) || (*t == '?' && cur->qmark)))
				{
					strncpy(buf, cur->name, buf_len);
					break;
				}
			}
			cur = cur->next;
		}

		if(cur != NULL && cur->type == USER_CMD &&
				strncmp(cur->name, buf, len) == 0)
		{
			/* For user-defined commands, the char is part of the name. */
			++t;
		}
	}

	return t;
}

/* Completes command arguments.  Returns offset at which completion was done. */
static int
complete_cmd_args(cmd_t *cur, const char args[], cmd_info_t *cmd_info,
		void *arg)
{
	const char *tmp_args = args;
	int result = 0;

	if(cur->id >= NO_COMPLETION_BOUNDARY && cur->id < 0)
		return 0;

	args = parse_tail(cur, tmp_args, cmd_info);
	args = vle_cmds_at_arg(args);
	result += args - tmp_args;

	if(cur->id == COMMAND_CMD_ID || cur->id == DELCOMMAND_CMD_ID)
	{
		const char *arg;

		arg = strrchr(args, ' ');
		arg = (arg == NULL) ? args : (arg + 1);

		complete_cmd_name(arg, 1);
		vle_compl_add_last_match(arg);

		result += arg - args;
	}
	else
	{
		int argc;
		char **argv;
		int (*argvp)[2];
		int last_arg = 0;

		argv = dispatch_line(args, &argc, ' ', 0, 1, 0, 0, &last_arg, &argvp);

		cmd_info->args = (char *)args;
		cmd_info->argc = argc;
		cmd_info->argv = argv;
		result += cmds_conf->complete_args(cur->id, cmd_info, last_arg, arg);

		free_string_array(argv, argc);
		free(argvp);
	}
	return result;
}

static void
complete_cmd_name(const char cmd_name[], int user_only)
{
	cmd_t *cur;
	size_t len;

	cur = inner->head.next;
	while(cur != NULL && strcmp(cur->name, cmd_name) < 0)
		cur = cur->next;

	len = strlen(cmd_name);
	while(cur != NULL && strncmp(cur->name, cmd_name, len) == 0)
	{
		if(cur->type == BUILTIN_ABBR)
			;
		else if(cur->type != USER_CMD && user_only)
			;
		else if(cur->name[0] == '\0')
			;
		else if(cur->type == USER_CMD && cur->descr == NULL)
			vle_compl_add_match(cur->name, cur->cmd);
		else
			vle_compl_add_match(cur->name, cur->descr);
		cur = cur->next;
	}
}

void
vle_cmds_add(const cmd_add_t cmds[], int count)
{
	int i;
	for(i = 0; i < count; ++i)
	{
		int ret_code;
		assert(cmds[i].min_args >= 0);
		assert(cmds[i].max_args == NOT_DEF ||
				cmds[i].min_args <= cmds[i].max_args);
		ret_code = add_builtin_cmd(cmds[i].name, BUILTIN_CMD, &cmds[i]);
		assert(ret_code == 0);
		if(cmds[i].abbr != NULL)
		{
			size_t full_len, short_len;
			char buf[strlen(cmds[i].name) + 1];
			assert(starts_with(cmds[i].name, cmds[i].abbr) &&
					"Abbreviation is consistent with full command");
			strcpy(buf, cmds[i].name);
			full_len = strlen(buf);
			short_len = strlen(cmds[i].abbr);
			while(full_len > short_len)
			{
				buf[--full_len] = '\0';
				ret_code = add_builtin_cmd(buf, BUILTIN_ABBR, &cmds[i]);
				assert(ret_code == 0);
			}
		}
		(void)ret_code;
	}
}

int
vle_cmds_add_foreign(const cmd_add_t *cmd)
{
	if(cmd->min_args < 0)
	{
		return 1;
	}

	if(cmd->max_args != NOT_DEF && cmd->min_args > cmd->max_args)
	{
		return 1;
	}

	if(!is_valid_udc_name(cmd->name))
	{
		return CMDS_ERR_INCORRECT_NAME;
	}

	int failure = (add_builtin_cmd(cmd->name, FOREIGN_CMD, cmd) != 0);
	if(!failure)
	{
		++inner->custom_cmd_count;
	}
	return failure;
}

/* Returns non-zero on error */
TSTATIC int
add_builtin_cmd(const char name[], CMD_TYPE type, const cmd_add_t *conf)
{
	int cmp;
	cmd_t *new;
	cmd_t *cur = &inner->head;

	if(strcmp(name, "<USERCMD>") == 0)
	{
		if(inner->user_cmd_handler.handler != NULL)
			return -1;
		inner->user_cmd_handler = *conf;
		return 0;
	}

	if(!is_builtin_like_name_ok(name))
	{
		return -1;
	}

	cmp = -1;
	while(cur->next != NULL && (cmp = strcmp(cur->next->name, name)) < 0)
	{
		cur = cur->next;
	}

	/* Command with the same name already exists. */
	if(cmp == 0)
	{
		if(strncmp(name, "command", strlen(name)) == 0)
		{
			inner->command_handler = conf->handler;
			return 0;
		}
		return -1;
	}

	new = insert_cmd(cur);
	if(new == NULL)
	{
		return -1;
	}

	new->name = strdup(name);
	new->descr = conf->descr;
	new->id = conf->id;
	new->user_data = conf->user_data;
	new->handler = conf->handler;
	new->type = type;
	new->passed = 0;
	new->cmd = NULL;
	new->min_args = conf->min_args;
	new->max_args = conf->max_args;
	new->deleted = 0;
	init_command_flags(new, conf->flags);

	return 0;
}

/* Checks validity of a name for a builtin-like command.  Returns non-zero if
 * it's valid and zero otherwise. */
static int
is_builtin_like_name_ok(const char name[])
{
	if(name[0] == '\0' || strcmp(name, "!") == 0)
	{
		return 1;
	}

	if(!isalpha(name[0]))
	{
		return 0;
	}

	int i;
	for(i = 1; name[i] != '\0'; ++i)
	{
		if(!isalnum(name[i]))
		{
			return 0;
		}
	}

	return 1;
}

/* Implements :comclear builtin command provided by this unit. */
static int
comclear_cmd(const cmd_info_t *cmd_info)
{
	remove_commands(USER_CMD);
	return 0;
}

/* Removes commands of the specified type. */
static void
remove_commands(CMD_TYPE type)
{
	cmd_t *cur = &inner->head;

	while(cur->next != NULL)
	{
		if(cur->next->type == type)
		{
			cmd_t *this = cur->next;
			cur->next = this->next;

			free(this->cmd);
			free(this->name);
			if(this->passed == 0)
			{
				free(this);
			}
			else
			{
				this->deleted = 1;
			}
		}
		else
		{
			cur = cur->next;
		}
	}
	inner->custom_cmd_count = 0;
}

/* Implements :command builtin command mostly provided by this unit. */
static int
command_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc < 2)
	{
		if(inner->command_handler == NULL)
		{
			return CMDS_ERR_TOO_FEW_ARGS;
		}
		return inner->command_handler(cmd_info);
	}

	char name[MAX_CMD_NAME_LEN + 1];
	const char *body = get_user_cmd_name(cmd_info->args, name, sizeof(name));
	body = vle_cmds_at_arg(body);

	return vle_cmds_add_user(name, body, NULL, cmd_info->emark);
}

int
vle_cmds_add_user(const char name[], const char body[], const char descr[],
		int overwrite)
{
	if(body[0] == '\0')
	{
		return CMDS_ERR_TOO_FEW_ARGS;
	}
	if(!is_valid_udc_name(name))
	{
		return CMDS_ERR_INCORRECT_NAME;
	}

	size_t len = strlen(name);
	int has_emark = (len > 0 && name[len - 1] == '!');
	int has_qmark = (len > 0 && name[len - 1] == '?');

	int cmp = -1;
	cmd_t *cur = &inner->head;
	while(cur->next != NULL && (cmp = strcmp(cur->next->name, name)) < 0)
	{
		int builtin_like = (cur->next->type == BUILTIN_CMD)
		                || (cur->next->type == FOREIGN_CMD);
		if(has_emark && builtin_like && cur->next->emark &&
				strncmp(name, cur->next->name, len - 1) == 0)
		{
			cmp = 0;
			break;
		}
		if(has_qmark && builtin_like && cur->next->qmark &&
				strncmp(name, cur->next->name, len - 1) == 0)
		{
			cmp = 0;
			break;
		}
		cur = cur->next;
	}

	cmd_t *new;
	if(cmp == 0)
	{
		cur = cur->next;
		if(cur->type == BUILTIN_CMD || cur->type == FOREIGN_CMD)
			return CMDS_ERR_NO_BUILTIN_REDEFINE;
		if(!overwrite)
			return CMDS_ERR_NEED_BANG;
		free(cur->name);
		free(cur->cmd);
		new = cur;
	}
	else
	{
		if((new = insert_cmd(cur)) == NULL)
		{
			return CMDS_ERR_NO_MEM;
		}
	}

	new->name = strdup(name);
	new->descr = descr;
	new->id = USER_CMD_ID;
	new->type = USER_CMD;
	new->passed = 0;
	new->cmd = strdup(body);
	new->min_args = inner->user_cmd_handler.min_args;
	new->max_args = inner->user_cmd_handler.max_args;
	new->deleted = 0;
	init_command_flags(new, inner->user_cmd_handler.flags);

	++inner->custom_cmd_count;
	return 0;
}

/* Initializes flag fields of *cmd from set of HAS_* flags. */
static void
init_command_flags(cmd_t *cmd, int flags)
{
	assert((flags & (HAS_RAW_ARGS | HAS_REGEXP_ARGS)) !=
			(HAS_RAW_ARGS | HAS_REGEXP_ARGS) && "Wrong flags combination.");
	assert((flags & (HAS_RAW_ARGS | HAS_QUOTED_ARGS)) !=
			(HAS_RAW_ARGS | HAS_QUOTED_ARGS) && "Wrong flags combination.");
	assert((flags & (HAS_QMARK_NO_ARGS | HAS_QMARK_WITH_ARGS)) !=
			(HAS_QMARK_NO_ARGS | HAS_QMARK_WITH_ARGS) && "Wrong flags combination.");
	assert((flags & (HAS_MACROS_FOR_CMD | HAS_MACROS_FOR_SHELL)) !=
			(HAS_MACROS_FOR_CMD | HAS_MACROS_FOR_SHELL) &&
			"Wrong flags combination.");

	cmd->range = ((flags & HAS_RANGE) != 0);
	cmd->cust_sep = ((flags & HAS_CUST_SEP) != 0);
	cmd->emark = ((flags & HAS_EMARK) != 0);
	cmd->envvars = ((flags & HAS_ENVVARS) != 0);
	cmd->select = ((flags & HAS_SELECTION_SCOPE) != 0);
	cmd->bg = ((flags & HAS_BG_FLAG) != 0);
	cmd->regexp = ((flags & HAS_REGEXP_ARGS) != 0);
	cmd->quote = ((flags & HAS_QUOTED_ARGS) != 0);
	cmd->noescaping = ((flags & HAS_RAW_ARGS) != 0);
	cmd->comment = ((flags & HAS_COMMENT) != 0);
	cmd->qmark = ((flags & (HAS_QMARK_NO_ARGS | HAS_QMARK_WITH_ARGS)) != 0);
	cmd->args_after_qmark = ((flags & HAS_QMARK_WITH_ARGS) != 0);
	cmd->macros_for_cmd = ((flags & HAS_MACROS_FOR_CMD) != 0);
	cmd->macros_for_shell = ((flags & HAS_MACROS_FOR_SHELL) != 0);
}

static const char *
get_user_cmd_name(const char cmd[], char buf[], size_t buf_len)
{
	const char *t;
	size_t len;

	t = vle_cmds_past_arg(cmd);

	len = MIN((size_t)(t - cmd), buf_len - 1);
	strncpy(buf, cmd, len);
	buf[len] = '\0';
	return t;
}

/* Checks that the name is a valid name for a user-defined command. */
static int
is_valid_udc_name(const char name[])
{
	assert(name[0] != '\0' && "Command name can't be empty");

	if(strcmp(name, "!") == 0)
		return 0;
	if(strcmp(name, "?") == 0)
		return 0;
	if(!isalpha(name[0]))
		return 0;
	if(strlen(name) >= MAX_CMD_NAME_LEN)
		return 0;

	char cmd_name[MAX_CMD_NAME_LEN];
	char *p = cmd_name;

	cmd_t *cmd = inner->head.next;

	while(name[0] != '\0')
	{
		*p++ = *name;

		if(!isalnum(name[0]))
		{
			if(name[1] != '\0')
				return 0;
			else if(name[0] != '!' && name[0] != '?')
				return 0;
		}
		else if(isdigit(name[1]))
		{
			*p = '\0';
			cmd = find_cmd_advance(cmd, cmd_name);

			if(find_cmd_match(cmd, cmd_name))
			{
				if(cmd->type == BUILTIN_CMD || cmd->type == BUILTIN_ABBR)
				{
					return 0;
				}

				const int name_len = p - cmd_name;
				if(cmd->name[name_len] == '\0' || isalpha(cmd->name[name_len]))
				{
					return 0;
				}
			}
		}

		++name;
	}

	*p = '\0';

	/* Builtins with custom separator have higher priority.  Disallow registering
	 * user-defined commands which will never be called. */
	if(name[-1] == '!' || name[-1] == '?')
	{
		cmd_name[strlen(cmd_name) - 1] = '\0';
	}

	cmd = find_cmd_advance(cmd, cmd_name);
	if(find_cmd_match(cmd, cmd_name))
	{
		if(cmd->cust_sep && strcmp(cmd->name, cmd_name) == 0)
			return 0;
		if(isdigit(cmd->name[strlen(cmd_name)]))
			return 0;
	}

	return 1;
}

/* Allocates new command node and inserts it after the specified one.  Returns
 * new uninitialized node or NULL on error. */
static cmd_t *
insert_cmd(cmd_t *after)
{
	cmd_t *const new = malloc(sizeof(*new));
	if(new == NULL)
	{
		return NULL;
	}

	new->next = after->next;
	after->next = new;
	return new;
}

/* Implements :command builtin command provided by this unit. */
static int
delcommand_cmd(const cmd_info_t *cmd_info)
{
	return vle_cmds_del_user(cmd_info->argv[0]);
}

int
vle_cmds_del_user(const char name[])
{
	int cmp;
	cmd_t *cur;
	cmd_t *cmd;

	cmp = -1;
	cur = &inner->head;
	while(cur->next != NULL && (cmp = strcmp(cur->next->name, name)) < 0)
		cur = cur->next;

	if(cur->next == NULL || cmp != 0)
		return CMDS_ERR_NO_SUCH_UDF;

	cmd = cur->next;
	cur->next = cmd->next;
	free(cmd->name);
	free(cmd->cmd);
	free(cmd);

	inner->custom_cmd_count--;
	return 0;
}

char *
vle_cmds_last_arg(const char cmd[], int quotes, size_t *len)
{
	int argc;
	char **argv;
	int (*argvp)[2];
	int last_start = 0;
	int last_end = 0;

	argv = dispatch_line(cmd, &argc, ' ', 0, quotes, 0, 0, NULL, &argvp);

	if(argc > 0)
	{
		last_start = argvp[argc - 1][0];
		last_end = argvp[argc - 1][1];
	}

	*len = last_end - last_start;
	free_string_array(argv, argc);
	free(argvp);
	return (char *)cmd + last_start;
}

/* Splits argument string into array of strings.  Non-zero noescaping means that
 * unquoted arguments don't have escaping.  Returns NULL if no arguments are
 * found or an error occurred.  Always sets *count (to negative value on
 * unmatched quotes and to zero on all other errors). */
TSTATIC char **
dispatch_line(const char args[], int *count, char sep, int regexp, int quotes,
		int noescaping, int comments, int *last_pos, int (**positions)[2])
{
	int len;
	int i;
	int st;
	const char *args_beg;
	char **params;
	int (*argvp)[2] = NULL;
	DA_INSTANCE(argvp);

	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING, ARG, QARG } state;

	if(last_pos != NULL)
		*last_pos = 0;
	*positions = NULL;

	*count = 0;
	params = NULL;

	args_beg = args;
	if(sep == ' ')
	{
		args = vle_cmds_at_arg(args);
	}

	char *cmdstr = strdup(args);
	if(cmdstr == NULL)
	{
		return NULL;
	}

	len = strlen(cmdstr);
	for(i = 0, st = 0, state = BEGIN; i <= len; ++i)
	{
		const int prev_state = state;
		switch(state)
		{
			case BEGIN:
				if(sep == ' ' && cmdstr[i] == '\'' && quotes)
				{
					st = i + 1;
					state = S_QUOTING;
				}
				else if(cmdstr[i] == '"' && ((sep == ' ' && quotes) ||
							(comments && strchr(&cmdstr[i + 1], '"') == NULL)))
				{
					st = i + 1;
					state = D_QUOTING;
				}
				else if(sep == ' ' && cmdstr[i] == '/' && regexp)
				{
					st = i + 1;
					state = R_QUOTING;
				}
				else if(!is_separator(cmdstr[i], sep))
				{
					st = i;
					state = NO_QUOTING;

					/* Omit escaped character from processign to account for possible case
					 * when it's a separator, which would lead to breaking argument into
					 * pieces. */
					if(!noescaping && cmdstr[i] == '\\' && cmdstr[i + 1] != '\0')
					{
						++i;
					}
				}
				else if(sep != ' ' && i > 0 && is_separator(cmdstr[i - 1], sep))
				{
					st = i--;
					state = NO_QUOTING;
				}
				if(state != BEGIN && cmdstr[i] != '\0')
				{
					if(DA_EXTEND(argvp) != NULL)
					{
						DA_COMMIT(argvp);
						argvp[DA_SIZE(argvp) - 1U][0] = i;
					}
				}
				break;
			case NO_QUOTING:
				if(cmdstr[i] == '\0' || is_separator(cmdstr[i], sep))
				{
					state = ARG;
				}
				else if(!noescaping && cmdstr[i] == '\\' && cmdstr[i + 1] != '\0')
				{
					++i;
				}
				break;
			case S_QUOTING:
				if(cmdstr[i] == '\'')
				{
					if(cmdstr[i + 1] == '\'')
					{
						++i;
					}
					else
					{
						state = QARG;
					}
				}
				break;
			case D_QUOTING:
				if(cmdstr[i] == '"')
				{
					state = QARG;
				}
				else if(!noescaping && cmdstr[i] == '\\' && cmdstr[i + 1] != '\0')
				{
					++i;
				}
				break;
			case R_QUOTING:
				if(cmdstr[i] == '/')
				{
					state = QARG;
				}
				else if(!noescaping && cmdstr[i] == '\\' && cmdstr[i + 1] == '/')
				{
					++i;
				}
				break;

			case ARG:
			case QARG:
				assert(0 && "Dispatch line state machine is broken");
				break;
		}
		if(state == ARG || state == QARG)
		{
			/* Found another argument. */

			const int end = (args - args_beg) + ((state == ARG) ? i : (i + 1));
			char *last_arg;
			const char c = cmdstr[i];
			cmdstr[i] = '\0';

			if(DA_SIZE(argvp) != 0U)
			{
				argvp[DA_SIZE(argvp) - 1U][1] = end;
			}

			*count = add_to_string_array(&params, *count, &cmdstr[st]);
			if(*count == 0)
			{
				break;
			}
			last_arg = params[*count - 1];

			cmdstr[i] = c;
			switch(prev_state)
			{
				case NO_QUOTING:
					if(!noescaping)
					{
						unescape(last_arg, sep != ' ');
					}
					break;
				case  S_QUOTING: expand_squotes_escaping(last_arg); break;
				case  D_QUOTING: expand_dquotes_escaping(last_arg); break;
				case  R_QUOTING: unescape(last_arg, 1);             break;
			}
			state = BEGIN;
		}
	}

	if(comments && state == D_QUOTING && strchr(&cmdstr[st], '"') == NULL)
	{
		state = BEGIN;
		--st;
		if(DA_SIZE(argvp) != 0U)
		{
			DA_REMOVE(argvp, &argvp[DA_SIZE(argvp) - 1U]);
		}
	}

	free(cmdstr);

	if(*count == 0 || (size_t)*count != DA_SIZE(argvp) ||
			(state != BEGIN && state != NO_QUOTING) ||
			put_into_string_array(&params, *count, NULL) != *count + 1)
	{
		free(argvp);
		free_string_array(params, *count);
		*count = (state == S_QUOTING || state == D_QUOTING || state == R_QUOTING)
		       ? -1 : 0;
		return NULL;
	}

	if(last_pos != NULL)
	{
		*last_pos = (args - args_beg) + st;
	}

	*positions = argvp;
	return params;
}

char **
vle_cmds_list_udcs(void)
{
	char **p;
	cmd_t *cur;

	char **const list = reallocarray(NULL, inner->custom_cmd_count*2 + 1,
			sizeof(*list));
	if(list == NULL)
	{
		return NULL;
	}

	p = list;

	cur = inner->head.next;
	while(cur != NULL)
	{
		if(cur->type == USER_CMD)
		{
			*p++ = strdup(cur->name);
			*p++ = strdup(cur->descr == NULL ? cur->cmd : cur->descr);
		}
		else if(cur->type == FOREIGN_CMD)
		{
			*p++ = strdup(cur->name);
			*p++ = strdup(cur->descr);
		}
		cur = cur->next;
	}

	*p = NULL;

	return list;
}

char *
vle_cmds_print_udcs(const char beginning[])
{
	size_t len;
	cmd_t *cur;
	char *content = NULL;
	size_t content_len = 0;

	cur = inner->head.next;
	len = strlen(beginning);
	while(cur != NULL)
	{
		void *ptr;
		size_t new_size;

		if(strncmp(cur->name, beginning, len) != 0 || cur->type != USER_CMD)
		{
			cur = cur->next;
			continue;
		}

		if(content == NULL)
		{
			content = strdup("Command -- Action");
			if(content == NULL)
			{
				return NULL;
			}

			content_len = strlen(content);
		}
		new_size = content_len + 1 + strlen(cur->name) + 10 + strlen(cur->cmd) + 1;
		ptr = realloc(content, new_size);
		if(ptr != NULL)
		{
			content = ptr;
			content_len += sprintf(content + content_len, "\n%-*s %s", 10, cur->name,
					cur->cmd);
		}
		cur = cur->next;
	}

	return content;
}

char *
vle_cmds_past_arg(const char args[])
{
	while(!is_separator(*args, ' ') && *args != '\0')
	{
		++args;
	}
	return (char *)args;
}

char *
vle_cmds_at_arg(const char args[])
{
	while(is_separator(*args, ' '))
	{
		++args;
	}
	return (char *)args;
}

char *
vle_cmds_next_arg(const char args[])
{
	args = vle_cmds_past_arg(args);
	return vle_cmds_at_arg(args);
}

/* Checks whether character is command separator.  Special case is space as a
 * separator, which means that any reasonable whitespace can be used as
 * separators (we don't won't to treat line feeds or similar characters as
 * separators to allow their appearance as part of arguments).  Returns non-zero
 * if c is counted to be a separator, otherwise zero is returned. */
static int
is_separator(char c, char sep)
{
	return (sep == ' ') ? (c == ' ' || c == '\t') : (c == sep);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
