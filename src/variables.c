/* vifm
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "completion.h"
#include "macros.h"
#include "utils.h"

#include "variables.h"

#define VAR_NAME_MAX 64
#define VAL_LEN_MAX 2048

typedef struct {
	char *name;
	char *val;
	char *initial;
	int from_parent;
	int removed;
}var_t;

static void init_var(const char *env);
static const char *parse_val(const char *str);
static void append_envvar(const char *name, const char *val);
static void set_envvar(const char *name, const char *val);
static var_t * get_record(const char *name);
static var_t * find_record(const char *name);
static void free_record(var_t *record);
static void clear_record(var_t *record);
static void print_msg(int error, const char *msg, const char *description);

static var_print print_func;
static int initialized;
static var_t *vars;
static size_t nvars;

void init_variables(var_print handler)
{
	int i;
	extern char **environ;

	print_func = handler;

	if(nvars > 0)
		clear_variables();

	/* count environment variables */
	i = 0;
	while(environ[i] != NULL)
		i++;

	/* allocate memory for environment variables */
	vars = malloc(sizeof(*vars)*i);
	assert(vars != NULL);

	/* initialize variable list */
	i = 0;
	while(environ[i] != NULL)
	{
		init_var(environ[i]);
		i++;
	}

	initialized = 1;
}

static void
init_var(const char *env)
{
	var_t *record;
	char name[VAR_NAME_MAX + 1];
	char *p = strchr(env, '=');
	assert(p != NULL);

	snprintf(name, MIN(sizeof(name), p - env + 1), "%s", env);
	record = get_record(name);
	if(record == NULL)
		return;
	record->from_parent = 1;

	free(record->initial);
	record->initial = strdup(p + 1);
	free(record->val);
	record->val = strdup(p + 1);
	if(record->initial == NULL || record->val == NULL)
	{
		free_record(record);
	}
}

void clear_variables(void)
{
	int i;
	assert(initialized);

	/* free memory */
	for(i = 0; i < nvars; i++)
	{
		if(vars[i].name == NULL)
			continue;

		if(vars[i].from_parent)
		{
			set_envvar(vars[i].name, vars[i].initial);
		}
		else
		{
			env_remove(vars[i].name);
		}
		free_record(&vars[i]);
	}

	nvars = 0;
	free(vars);
	vars = NULL;
}

int
let_variable(const char *cmd)
{
	char name[VAR_NAME_MAX + 1];
	char *p;
	const char *cp;
	int append = 0;

	assert(initialized);

	/* currently we support only environment variables */
	if(*cmd != '$')
	{
		print_msg(1, "", "Incorrect variable type");
		return -1;
	}
	cmd++;

	/* copy variable name */
	p = name;
	while(*cmd != '\0' && !isspace(*cmd) && *cmd != '.' && *cmd != '=' &&
			p - name < sizeof(name) - 1)
	{
		if(*cmd != '_' && !isalnum(*cmd))
		{
			print_msg(1, "", "Incorrect variable name");
			return -1;
		}
		*p++ = *cmd++;
	}
	/* test for empty variable name */
	if(p == name)
	{
		print_msg(1, "Unsupported variable name", "empty name");
		return -1;
	}
	*p = '\0';

	cmd = skip_whitespace(cmd);

	/* check for dot and skip it */
	if(*cmd == '.')
	{
		append = 1;
		cmd++;
	}

	/* check for equal sign and skip it and all whitespace */
	if(*cmd != '=')
	{
		print_msg(1, "Incorrect :let statement", "'=' expected");
		return -1;
	}
	cmd = skip_whitespace(cmd + 1);

	/* ensure value starts with quotes */
	if(*cmd != '\'' && *cmd != '"')
	{
		print_msg(1, "Incorrect :let statement", "expected single or double quote");
		return -1;
	}

	/* parse value */
	cp = parse_val(cmd);
	if(cp == NULL)
		return -1;

	/* update environment variable */
	if(append)
		append_envvar(name, cp);
	else
		set_envvar(name, cp);

	return 0;
}

/* Returns pointer to a statically allocated buffer or NULL on error */
static const char *
parse_val(const char *str)
{
	static char buf[VAL_LEN_MAX + 1];
	char *p = buf;
	int quote = (*str++ == '"') ? 2 : 1;
	int slash = 0;

	/* parse all in currect quotes */
	while(*str != '\0' && quote != 0)
	{
		if(slash == 1)
		{
			*p++ = *str++;
			slash = 0;
		}
		else if(*str == '\\')
		{
			slash = 1;
		}
		else if(*str == '\'' && quote == 0)
		{
			quote = 1;
		}
		else if(*str == '\'' && quote == 1)
		{
			quote = 0;
		}
		else if(*str == '"' && quote == 0)
		{
			quote = 2;
		}
		else if(*str == '"' && quote == 2)
		{
			quote = 0;
		}
		else
		{
			*p++ = *str;
		}
		str++;
	}

	/* report an error if input is invalid */
	if(*str != '\0' || quote != 0)
	{
		if(quote != 0)
			print_msg(1, "Incorrect value", "unclosed quote");
		else
			print_msg(1, "Incorrect value", "trailing characters");
		return NULL;
	}

	*p = '\0';
	return buf;
}

static void
append_envvar(const char *name, const char *val)
{
	var_t *record;
	char *p;

	record = find_record(name);
	if(record == NULL)
	{
		set_envvar(name, val);
		return;
	}

	p = realloc(record->val, strlen(record->val) + strlen(val) + 1);
	if(p == NULL)
	{
		print_msg(1, "", "Not enough memory");
		return;
	}
	record->val = p;

	strcat(record->val, val);
	env_set(name, record->val);
}

static void
set_envvar(const char *name, const char *val)
{
	var_t *record;
	char *p;

	record = get_record(name);
	if(record == NULL)
	{
		print_msg(1, "", "Not enough memory");
		return;
	}

	p = strdup(val);
	if(p == NULL)
	{
		print_msg(1, "", "Not enough memory");
		return;
	}
	free(record->val);
	record->val = p;
	env_set(name, val);
}

/* searches for variable and creates new record if it didn't existed */
static var_t *
get_record(const char *name)
{
	var_t *p = NULL;
	int i;

	/* search for existent variable */
	for(i = 0; i < nvars; i++)
	{
		if(vars[i].name == NULL)
			p = &vars[i];
		else if(strcmp(vars[i].name, name) == 0)
			return &vars[i];
	}

	if(p == NULL)
	{
		/* try to reallocate list of variables */
		p = realloc(vars, sizeof(*vars)*(nvars + 1));
		if(p == NULL)
			return NULL;
		vars = p;
		p = &vars[nvars];
	}

	/* initialize new record */
	p->initial = strdup("");
	p->name = strdup(name);
	p->val = strdup("");
	p->from_parent = 0;
	p->removed = 0;
	if(p->initial == NULL || p->name == NULL || p->val == NULL)
	{
		free_record(p);
		return NULL;
	}
	nvars++;
	return p;
}

int
unlet_variables(const char *cmd)
{
	int error = 0;
	assert(initialized);

	while(*cmd != '\0')
	{
		var_t *record;

		char name[VAR_NAME_MAX + 1];
		char *p;
		int envvar = 1;

		/* check if its environment variable */
		if(*cmd != '$')
			envvar = 0;
		else
			cmd++;

		/* copy variable name */
		p = name;
		while(*cmd != '\0' && !isspace(*cmd) && *cmd != '=' &&
				p - name < sizeof(name) - 1)
			*p++ = *cmd++;
		*p = '\0';

		cmd = skip_whitespace(cmd);

		/* currently we support only environment variables */
		if(!envvar)
		{
			print_msg(1, "Unsupported variable type", name);

			cmd = skip_non_whitespace(cmd);
			error++;
			continue;
		}

		/* test for empty variable name */
		if(name[0] == '\0')
		{
			print_msg(1, "Unsupported variable name", "empty name");
			error++;
			continue;
		}

		record = find_record(name);
		if(record == NULL || record->removed)
		{
			print_msg(1, "No such variable", name);
			error++;
			continue;
		}

		if(record->from_parent)
			record->removed = 1;
		else
			free_record(record);
		env_remove(name);
	}

	return error;
}

/* searches for existent variable */
static var_t *
find_record(const char *name)
{
	int i;
	for(i = 0; i < nvars; i++)
	{
		if(vars[i].name != NULL && strcmp(vars[i].name, name) == 0)
			return &vars[i];
	}
	return NULL;
}

static void
free_record(var_t *record)
{
	free(record->initial);
	free(record->name);
	free(record->val);

	clear_record(record);
}

static void
clear_record(var_t *record)
{
	record->initial = NULL;
	record->name = NULL;
	record->val = NULL;
}

static void
print_msg(int error, const char *msg, const char *description)
{
	if(print_func == NULL)
		return;
	print_func(error, msg, description);
}

void
complete_variables(const char *cmd, const char **start)
{
	int i;
	size_t len;
	assert(initialized);

	/* currently we support only environment variables */
	if(*cmd != '$')
	{
		*start = cmd;
		add_completion(cmd);
		return;
	}
	cmd++;
	*start = cmd;

	/* add all variables that start with given beginning */
	len = strlen(cmd);
	for(i = 0; i < nvars; i++)
	{
		if(vars[i].name == NULL)
			continue;
		if(vars[i].removed)
			continue;
		if(strncmp(vars[i].name, cmd, len) == 0)
			add_completion(vars[i].name);
	}
	completion_group_end();
	add_completion(cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
