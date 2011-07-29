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

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "completion.h"
#include "log.h"
#include "macros.h"

#include "cmds.h"

#define MAX_CMD_RECURSION 16
#define INVALID_MARK -4096

enum CMD_TYPE
{
	BUILDIN_ABBR,
	BUILDIN_CMD,
	USER_CMD,
};

struct cmd_t
{
	char *name;
	int id;
	enum CMD_TYPE type;
	int passed;

	cmd_handler handler;
	char *cmd;

	int range;
	int cust_sep;
	int emark;
	int qmark;
	int expand;
	int min_args, max_args;
	int regexp;
	int select;
	int bg;

	struct cmd_t *next;
};

static struct cmd_t head;
static struct cmd_add user_cmd_handler;
static cmd_handler command_handler;

static const char * parse_range(const char *cmd, struct cmd_info *cmd_info);
static const char * parse_limit(const char *cmd, struct cmd_info *cmd_info);
static const char * parse_tail(struct cmd_t *cur,
		const char *cmd, struct cmd_info *cmd_info);
static char ** dispatch_line(const char *args, int *count, char sep,
		int regexp);
static int get_args_count(const char *cmdstr, char sep, int regexp);
static void unescape(char *s, int regexp);
static void replace_esc(char *s);
static int get_cmd_info(const char *cmd, struct cmd_info *info);
static int name_is_ambiguous(const char *name);
static const char *get_cmd_name(const char *cmd, char *buf, size_t buf_len);
static void init_cmd_info(struct cmd_info *cmd_info);
static void complete_cmd_name(const char *cmd_name, int user_only);
#ifndef TEST
static
#endif
int add_buildin_cmd(const char *name, int abbr, const struct cmd_add *conf);
static int command_cmd(const struct cmd_info *cmd_info);
static const char * get_user_cmd_name(const char *cmd, char *buf, size_t buf_len);
static int is_correct_name(const char *name);
static struct cmd_t * insert_cmd(struct cmd_t *after);
static int delcommand_cmd(const struct cmd_info *cmd_info);

void
init_cmds(void)
{
	struct cmd_add commands[] = {
		{
			.name = "command",    .abbr = "com",  .handler = command_cmd,    .id = COMMAND_CMD_ID,
			.range = 0,           .emark = 1,     .qmark = 0,                .regexp = 0,             .select = 0,
			.expand = 0,          .bg = 0,        .min_args = 0,             .max_args = NOT_DEF,
		}, {
			.name = "delcommand", .abbr = "delc", .handler = delcommand_cmd, .id = DELCOMMAND_CMD_ID,
			.range = 0,           .emark = 1,     .qmark = 0,                .regexp = 0,             .select = 0,
			.expand = 0,          .bg = 0,        .min_args = 1,             .max_args = 1,
		}
	};

	assert(cmds_conf.complete_args != NULL);
	assert(cmds_conf.swap_range != NULL);
	assert(cmds_conf.resolve_mark != NULL);
	assert(cmds_conf.expand_macros != NULL);
	assert(cmds_conf.post != NULL);
	assert(cmds_conf.select_range != NULL);

	add_buildin_commands(commands, 2);
}

void
reset_cmds(void)
{
	struct cmd_t *cur = head.next;

	while(cur != NULL)
	{
		struct cmd_t *next = cur->next;
		free(cur->name);
		free(cur);
		cur = next;
	}

	head.next = NULL;
	user_cmd_handler.handler = NULL;
}

int
execute_cmd(const char *cmd)
{
	struct cmd_info cmd_info;
	char cmd_name[256];
	struct cmd_t *cur;
	size_t len;
	const char *args;
	int result;
	int i;

	init_cmd_info(&cmd_info);
	cmd = parse_range(cmd, &cmd_info);
	if(cmd == NULL)
	{
		if(cmd_info.end == INVALID_MARK)
			return 0;
		else
			return CMDS_ERR_INVALID_CMD;
	}

	if(*cmd != '\0' && cmd_info.end < cmd_info.begin)
	{
		int t;

		if(!cmds_conf.swap_range())
			return CMDS_ERR_INVALID_RANGE;

		t = cmd_info.end;
		cmd_info.end = cmd_info.begin;
		cmd_info.begin = t;
	}

	cmd = get_cmd_name(cmd, cmd_name, sizeof(cmd_name));
	cur = head.next;
	while(cur != NULL && strcmp(cur->name, cmd_name) < 0)
		cur = cur->next;

	len = strlen(cmd_name);
	if(cur == NULL || strncmp(cmd_name, cur->name, len) != 0)
		return CMDS_ERR_INVALID_CMD;

	args = parse_tail(cur, cmd, &cmd_info);

	cmd_info.raw_args = strdup(args);
	len = strlen(cmd_info.raw_args);
	if(cur->bg && len >= 2 && cmd_info.raw_args[len - 1] == '&' &&
			cmd_info.raw_args[len - 2] == ' ')
	{
		cmd_info.bg = 1;
		cmd_info.raw_args[len - 2] = '\0';
	}
	if(cur->expand)
		cmd_info.args = cmds_conf.expand_macros(cmd_info.raw_args);
	else
		cmd_info.args = strdup(cmd_info.raw_args);
	cmd_info.argv = dispatch_line(cmd_info.args, &cmd_info.argc, cmd_info.sep,
			cur->regexp);

	if((cmd_info.begin != NOT_DEF || cmd_info.end != NOT_DEF) &&
			!cur->range)
	{
		result = CMDS_ERR_NO_RANGE_ALLOWED;
	}
	else if(cmd_info.argc < cur->min_args)
	{
		result = CMDS_ERR_TOO_FEW_ARGS;
	}
	else if(cmd_info.argc > cur->max_args && cur->max_args != NOT_DEF)
	{
		result = CMDS_ERR_TRAILING_CHARS;
	}
	else if(cmd_info.emark && !cur->emark)
	{
		result = CMDS_ERR_NO_BANG_ALLOWED;
	}
	else if(cmd_info.qmark && !cur->qmark)
	{
		result = CMDS_ERR_NO_QMARK_ALLOWED;
	}
	else if(cmd_info.qmark && *args != '\0')
	{
		result = CMDS_ERR_TRAILING_CHARS;
	}
	else if(cur->passed > MAX_CMD_RECURSION)
	{
		result = CMDS_ERR_LOOP;
	}
	else
	{
		cur->passed++;
		if(cur->select)
			cmds_conf.select_range(&cmd_info);

		if(cur->type != BUILDIN_CMD && cur->type != BUILDIN_ABBR)
		{
			cmd_info.cmd = cur->cmd;
			result = user_cmd_handler.handler(&cmd_info);
		}
		else
		{
			result = cur->handler(&cmd_info);
		}

		cmds_conf.post(cur->id);
		cur->passed--;
	}

	free(cmd_info.raw_args);
	free(cmd_info.args);
	for(i = 0; i < cmd_info.argc; i++)
		free(cmd_info.argv[i]);
	free(cmd_info.argv);

	return result;
}

/* Returns NULL on invalid range */
static const char *
parse_range(const char *cmd, struct cmd_info *cmd_info)
{
	while(isspace(*cmd))
		cmd++;

	if(isalpha(*cmd) || *cmd == '!' || *cmd == '\0')
		return cmd;

	for(;;)
	{
		cmd_info->begin = cmd_info->end;

		cmd = parse_limit(cmd, cmd_info);

		if(cmd == NULL)
			return NULL;

		if(cmd_info->begin == NOT_DEF)
			cmd_info->begin = cmd_info->end;

		while(isspace(*cmd))
			cmd++;

		if(*cmd != ',')
			break;

		cmd++;

		while(isspace(*cmd))
			cmd++;
	}

	return cmd;
}

static const char *
parse_limit(const char *cmd, struct cmd_info *cmd_info)
{
	if(cmd[0] == '%')
	{
		cmd_info->begin = cmds_conf.begin;
		cmd_info->end = cmds_conf.end;
		cmd++;
	}
	else if(cmd[0] == '$')
	{
		cmd_info->end = cmds_conf.end;
		cmd++;
	}
	else if(cmd[0] == '.')
	{
		cmd_info->end = cmds_conf.current;
		cmd++;
	}
	else if(*cmd == ',')
	{
		cmd_info->end = cmds_conf.current;
	}
	else if(isalpha(*cmd))
	{
		cmd_info->end = cmds_conf.current;
	}
	else if(isdigit(*cmd))
	{
		char *p;
		cmd_info->end = strtol(cmd, &p, 10) - 1;
		if(cmd_info->end < cmds_conf.begin)
			cmd_info->end = cmds_conf.begin;
		cmd = p;
	}
	else if(*cmd == '\'')
	{
		char mark;
		cmd++;
		mark = *cmd++;
		cmd_info->end = cmds_conf.resolve_mark(mark);
		if(cmd_info->end < 0)
		{
			cmd_info->end = INVALID_MARK;
			return NULL;
		}
	}
	else
	{
		return NULL;
	}

	return cmd;
}

static const char *
parse_tail(struct cmd_t *cur, const char *cmd, struct cmd_info *cmd_info)
{
	if(*cmd == '!')
	{
		cmd_info->emark = 1;
		cmd++;
	}
	else if(*cmd == '?')
	{
		cmd_info->qmark = 1;
		cmd++;
	}
	else if(*cmd != '\0' && !isspace(*cmd))
	{
		if(cur->cust_sep)
			cmd_info->sep = *cmd;
		else
			return cmd;
	}
	while(*cmd == cmd_info->sep)
		cmd++;
	return cmd;
}

static char **
dispatch_line(const char *args, int *count, char sep, int regexp)
{
	char *cmdstr;
	int len;
	int i, j;
	int state, st;
	char** params;

	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING, ARG };

	*count = get_args_count(args, sep, regexp);
	if(*count == 0)
		return NULL;

	params = malloc(sizeof(char*)*(*count + 1));
	if(params == NULL)
		return NULL;

	while(args[0] == sep)
		args++;
	cmdstr = strdup(args);
	len = strlen(cmdstr);
	for(i = 0, st, j = 0, state = BEGIN; i <= len; ++i)
	{
		int prev_state = state;
		switch(state)
		{
			case BEGIN:
				if(sep == ' ' && cmdstr[i] == '\'')
				{
					st = i + 1;
					state = S_QUOTING;
				}
				else if(sep == ' ' && cmdstr[i] == '"')
				{
					st = i + 1;
					state = D_QUOTING;
				}
				else if(sep == ' ' && cmdstr[i] == '/' && regexp)
				{
					st = i + 1;
					state = R_QUOTING;
				}
				else if(cmdstr[i] != sep)
				{
					st = i;
					state = NO_QUOTING;
				}
				break;
			case NO_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == sep)
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case S_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '\'')
					state = ARG;
			case D_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '"')
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case R_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '/')
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] == '/')
						i++;
				}
				break;
		}
		if(state == ARG)
		{
			/* found another argument */
			cmdstr[i] = '\0';
			params[j] = strdup(&cmdstr[st]);
			if(prev_state == NO_QUOTING)
				unescape(params[j], 0);
			else if(prev_state == D_QUOTING)
				replace_esc(params[j]);
			else if(prev_state == R_QUOTING)
				unescape(params[j], 1);
			j++;
			state = BEGIN;
		}
	}

	params[*count] = NULL;

	free(cmdstr);
	return params;
}

static int
get_args_count(const char *cmdstr, char sep, int regexp)
{
	int i, state;
	int result = 0;
	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING };

	state = BEGIN;
	for(i = 0; cmdstr[i] != '\0'; i++)
		switch(state)
		{
			case BEGIN:
				if(sep == ' ' && cmdstr[i] == '\'')
					state = S_QUOTING;
				else if(sep == ' ' && cmdstr[i] == '"')
					state = D_QUOTING;
				else if(sep == ' ' && cmdstr[i] == '/' && regexp)
					state = R_QUOTING;
				else if(cmdstr[i] != sep)
					state = NO_QUOTING;
				break;
			case NO_QUOTING:
				if(cmdstr[i] == sep)
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case S_QUOTING:
				if(cmdstr[i] == '\'')
				{
					result++;
					state = BEGIN;
				}
				break;
			case D_QUOTING:
				if(cmdstr[i] == '"')
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case R_QUOTING:
				if(cmdstr[i] == '/')
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
		}
	if(state == NO_QUOTING)
		result++;
	else if(state != BEGIN)
		return 0; /* error: no closing quote */

	return result;
}

static void
unescape(char *s, int regexp)
{
	char *p;

	p = s;
	while(s[0] != '\0')
	{
		if(s[0] == '\\' && (!regexp || s[1] == '/'))
			s++;
		*p++ = *s++;
	}
	*p = '\0';
}

static void
replace_esc(char *s)
{
	static const char table[] =
						/* 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
	/* 00 */	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	/* 10 */	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	/* 20 */	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	/* 30 */	"\x00\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	/* 40 */	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	/* 50 */	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	/* 60 */	"\x60\x07\x0b\x63\x64\x65\x0c\x67\x68\x69\x6a\x6b\x6c\x6d\x0a\x6f"
	/* 70 */	"\x70\x71\x0d\x73\x09\x75\x0b\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
	/* 80 */	"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
	/* 90 */	"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
	/* a0 */	"\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
	/* b0 */	"\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
	/* c0 */	"\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
	/* d0 */	"\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
	/* e0 */	"\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
	/* f0 */	"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

	char *str = s;
	char *p;

	p = s;
	while(*s != '\0')
	{
		if(*s != '\\')
		{
			*p++ = *s++;
			continue;
		}
		s++;
		if(*s == '\0')
		{
			LOG_ERROR_MSG("Escaped eol in \"%s\"", str);
			break;
		}
		*p++ = table[(int)*s++];
	}
	*p = '\0';
}

int
get_cmd_id(const char *cmd)
{
	struct cmd_info info;
	init_cmd_info(&info);
	return get_cmd_info(cmd, &info);
}

char
get_cmd_sep(const char *cmd)
{
	struct cmd_info info;
	init_cmd_info(&info);
	if(get_cmd_info(cmd, &info) == -1)
		return ' ';
	return info.sep;
}

static void
init_cmd_info(struct cmd_info *cmd_info)
{
	cmd_info->begin = NOT_DEF;
	cmd_info->end = NOT_DEF;
	cmd_info->emark = 0;
	cmd_info->qmark = 0;
	cmd_info->raw_args = NULL;
	cmd_info->args = NULL;
	cmd_info->argc = 0;
	cmd_info->argv = NULL;
	cmd_info->cmd = NULL;
	cmd_info->sep = ' ';
	cmd_info->bg = 0;
}

/* Returns command id */
static int
get_cmd_info(const char *cmd, struct cmd_info *info)
{
	struct cmd_info cmd_info;
	char cmd_name[256];
	struct cmd_t *cur;
	size_t len;

	cmd = parse_range(cmd, &cmd_info);
	if(cmd == NULL)
		return CMDS_ERR_INVALID_CMD;

	cmd = get_cmd_name(cmd, cmd_name, sizeof(cmd_name));
	cur = head.next;
	while(cur != NULL && strcmp(cur->name, cmd_name) < 0)
		cur = cur->next;

	len = strlen(cmd_name);
	if(cur == NULL || strncmp(cmd_name, cur->name, len) != 0)
		return -1;

	if(parse_tail(cur, cmd, &cmd_info) == NULL)
		return -1;

	*info = cmd_info;
	return cur->id;
}

int
complete_cmd(const char *cmd)
{
	struct cmd_info cmd_info;
	char cmd_name[256];
	const char *args;
	const char *cmd_name_pos;
	size_t prefix_len;

	cmd_name_pos = parse_range(cmd, &cmd_info);
	args = get_cmd_name(cmd_name_pos, cmd_name, sizeof(cmd_name));

	if(*args == '\0' && name_is_ambiguous(cmd_name))
	{
		complete_cmd_name(cmd_name, 0);
		prefix_len = cmd_name_pos - cmd;
	}
	else
	{
		int id;

		id = get_cmd_id(cmd_name);
		if(id == -1)
			return 0;

		prefix_len = args - cmd;

		if(id == COMMAND_CMD_ID || id == DELCOMMAND_CMD_ID)
		{
			const char *arg;

			arg = strrchr(args, ' ');
			if(arg == NULL)
				arg = args;
			else
				arg++;

			complete_cmd_name(arg, 1);
			prefix_len += arg - args;
		}
		else
		{
			prefix_len += cmds_conf.complete_args(id, args);
		}
	}

	return prefix_len;
}

static int
name_is_ambiguous(const char *name)
{
	size_t len;
	int count;
	struct cmd_t *cur;

	len = strlen(name);
	count = 0;
	cur = head.next;
	while(cur != NULL)
	{
		int cmp;

		cmp = strncmp(cur->name, name, len);
		if(cmp == 0)
		{
			if(++count == 2)
				return 1;
		}
		else if(cmp > 0)
			return (count == 1) ? 0 : 1;

		cur = cur->next;
	}
	return 0;
}

static const char *
get_cmd_name(const char *cmd, char *buf, size_t buf_len)
{
	const char *t;
	size_t len;

	if(cmd[0] == '!')
	{
		strcpy(buf, "!");
		do
			cmd++;
		while(isspace(*cmd));
		return cmd;
	}

	t = cmd;
	while(isalpha(*t))
		t++;

	len = MIN(t - cmd, buf_len);
	strncpy(buf, cmd, len);
	if(*t == '?' || *t == '!')
	{
		struct cmd_t *cur;

		buf[len] = *t;
		buf[len + 1] = '\0';
		cur = head.next;
		while(cur != NULL && strcmp(cur->name, buf) < 0)
			cur = cur->next;
		if(cur != NULL && strncmp(cur->name, buf, len + 1) == 0)
			return t + 1;
	}
	buf[len] = '\0';

	while(isspace(*t))
		t++;
	return t;
}

static void
complete_cmd_name(const char *cmd_name, int user_only)
{
	struct cmd_t *cur;
	size_t len;

	cur = head.next;
	while(cur != NULL && strcmp(cur->name, cmd_name) < 0)
		cur = cur->next;

	len = strlen(cmd_name);
	while(cur != NULL && strncmp(cur->name, cmd_name, len) == 0)
	{
		if(cur->type == BUILDIN_ABBR)
			;
		else if(cur->type != USER_CMD && user_only)
			;
		else if(cur->name[0] == '\0')
			;
		else
			add_completion(cur->name);
		cur = cur->next;
	}
}

void
add_buildin_commands(const struct cmd_add *cmds, int count)
{
	int i;
	for(i = 0; i < count; i++)
	{
		assert(cmds[i].min_args >= 0);
		assert(cmds[i].max_args == NOT_DEF ||
				cmds[i].min_args <= cmds[i].max_args);
		assert(add_buildin_cmd(cmds[i].name, 0, &cmds[i]) == 0);
		if(cmds[i].abbr != NULL)
			assert(add_buildin_cmd(cmds[i].abbr, 1, &cmds[i]) == 0);
	}
}

/* Returns non-zero on error */
#ifndef TEST
static
#endif
int
add_buildin_cmd(const char *name, int abbr, const struct cmd_add *conf)
{
	int i;
	int cmp;
	struct cmd_t *new;
	struct cmd_t *cur = &head;

	if(strcmp(name, "<USERCMD>") == 0)
	{
		if(user_cmd_handler.handler != NULL)
			return -1;
		user_cmd_handler = *conf;
		return 0;
	}

	if(strcmp(name, "!") != 0)
		for(i = 0; name[i] != '\0'; i++)
			if(!isalpha(name[i]))
				return -1;

	cmp = -1;
	while(cur->next != NULL && (cmp = strcmp(cur->next->name, name)) < 0)
		cur = cur->next;

	/* command with the same name already exists */
	if(cmp == 0)
	{
		if(strcmp(name, "command") == 0 || strcmp(name, "com") == 0)
		{
			command_handler = conf->handler;
			return 0;
		}
		return -1;
	}

	if((new = insert_cmd(cur)) == NULL)
		return -1;
	new->name = strdup(name);
	new->id = conf->id;
	new->handler = conf->handler;
	new->type = abbr ? BUILDIN_ABBR : BUILDIN_CMD;
	new->passed = 0;
	new->range = conf->range;
	new->cust_sep = conf->cust_sep;
	new->emark = conf->emark;
	new->qmark = conf->qmark;
	new->expand = conf->expand;
	new->min_args = conf->min_args;
	new->max_args = conf->max_args;
	new->regexp = conf->regexp;
	new->select = conf->select;
	new->bg = conf->bg;

	return 0;
}

static int
command_cmd(const struct cmd_info *cmd_info)
{
	int cmp;
	char cmd_name[256];
	const char *args;
	struct cmd_t *new, *cur;

	if(cmd_info->argc < 2)
	{
		if(command_handler != NULL)
			return command_handler(cmd_info);
		else
			return CMDS_ERR_TOO_FEW_ARGS;
	}

	args = get_user_cmd_name(cmd_info->args, cmd_name, sizeof(cmd_name));
	while(isspace(args[0]))
		args++;
	if(args[0] == '\0')
		return CMDS_ERR_TOO_FEW_ARGS;
	else if(!is_correct_name(cmd_name))
		return CMDS_ERR_INCORRECT_NAME;

	cmp = -1;
	cur = &head;
	while(cur->next != NULL && (cmp = strcmp(cur->next->name, cmd_name)) < 0)
		cur = cur->next;

	if(cmp == 0)
	{
		cur = cur->next;
		if(!cmd_info->emark)
			return CMDS_ERR_NEED_BANG;
		if(cur->type == BUILDIN_CMD)
			return CMDS_ERR_NO_BUILDIN_REDEFINE;
		free(cur->name);
		free(cur->cmd);
    new = cur;
	}
	else
	{
		new = insert_cmd(cur);
	}

	new->name = strdup(cmd_name);
	new->id = USER_CMD_ID;
	new->type = USER_CMD;
	new->passed = 0;
	new->cmd = strdup(args);
	new->range = user_cmd_handler.range;
	new->cust_sep = user_cmd_handler.cust_sep;
	new->emark = user_cmd_handler.emark;
	new->qmark = user_cmd_handler.qmark;
	new->expand = user_cmd_handler.expand;
	new->min_args = user_cmd_handler.min_args;
	new->max_args = user_cmd_handler.max_args;
	new->regexp = user_cmd_handler.regexp;
	new->select = user_cmd_handler.select;
	new->bg = user_cmd_handler.bg;

	return 0;
}

static const char *
get_user_cmd_name(const char *cmd, char *buf, size_t buf_len)
{
	const char *t;
	size_t len;

	t = cmd;
	while(!isspace(*t) && *t != '\0')
		t++;

	len = MIN(t - cmd, buf_len);
	strncpy(buf, cmd, len);
	buf[len] = '\0';
	return t;
}

static int
is_correct_name(const char *name)
{
	if(strcmp(name, "!") == 0)
		return 0;

	if(strcmp(name, "?") == 0)
		return 0;

	while(name[0] != '\0')
	{
		if(!isalpha(name[0]))
		{
			if(name[1] != '\0')
				return 0;
			else if(name[0] != '!' && name[0] != '?')
				return 0;
		}
		name++;
	}
	return 1;
}

static struct cmd_t *
insert_cmd(struct cmd_t *after)
{
	struct cmd_t *new;
  new = malloc(sizeof(*new));
	if(new == NULL)
		return NULL;
	new->next = after->next;
	after->next = new;
	return new;
}

static int
delcommand_cmd(const struct cmd_info *cmd_info)
{
	int cmp;
	struct cmd_t *cur;
	struct cmd_t *cmd;

	cmp = -1;
	cur = &head;
	while(cur->next != NULL &&
			(cmp = strcmp(cur->next->name, cmd_info->argv[0])) < 0)
		cur = cur->next;

	if(cur->next == NULL || cmp != 0)
		return CMDS_ERR_NO_SUCH_UDF;

	cmd = cur->next;
	cur->next = cmd->next;
	free(cmd->name);
	free(cmd->cmd);
	free(cmd);

	/* TODO write function body */
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
