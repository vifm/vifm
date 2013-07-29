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

#include "variables.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h> /* free() */
#include <string.h>

#include "../utils/env.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "completion.h"
#include "parsing.h"
#include "text_buffer.h"
#include "var.h"

#define VAR_NAME_MAX 64
#define VAL_LEN_MAX 2048

typedef struct {
	char *name;
	char *val;
	char *initial;
	int from_parent;
	int removed;
}envvar_t;

static const char ENV_VAR_NAME_CHARS[] = "abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

static void init_var(const char *env);
static void report_parsing_error(ParsingErrors error);
static void append_envvar(const char *name, const char *val);
static void set_envvar(const char *name, const char *val);
static envvar_t * get_record(const char *name);
static envvar_t * find_record(const char *name);
static void free_record(envvar_t *record);
static void clear_record(envvar_t *record);

static int initialized;
static envvar_t *vars;
static size_t nvars;

void
init_variables(void)
{
	int env_count;
	extern char **environ;

	if(nvars > 0)
		clear_variables();

	/* count environment variables */
	env_count = 0;
	while(environ[env_count] != NULL)
		env_count++;

	if(env_count > 0)
	{
		int i;

		/* allocate memory for environment variables */
		vars = malloc(sizeof(*vars)*env_count);
		assert(vars != NULL);

		/* initialize variable list */
		i = 0;
		while(environ[i] != NULL)
		{
			init_var(environ[i]);
			i++;
		}
	}

	init_parser(&local_getenv);

	initialized = 1;
}

const char *
local_getenv(const char *envname)
{
	envvar_t *record = find_record(envname);
	return (record == NULL || record->removed) ? "" : record->val;
}

static void
init_var(const char *env)
{
	envvar_t *record;
	char name[VAR_NAME_MAX + 1];
	char *p = strchr(env, '=');
	assert(p != NULL);

	snprintf(name, MIN(sizeof(name), p - env + 1), "%s", env);
	record = get_record(name);
	if(record == NULL)
		return;
	record->from_parent = 1;

	(void)replace_string(&record->initial, p + 1);
	(void)replace_string(&record->val, p + 1);
	if(record->initial == NULL || record->val == NULL)
	{
		free_record(record);
	}
}

void
clear_variables(void)
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
	int append = 0;
	var_t res_var;
	char *str_var;
	ParsingErrors parsing_error;

	assert(initialized);

	/* currently we support only environment variables */
	if(*cmd != '$')
	{
		text_buffer_add("Incorrect variable type");
		return -1;
	}
	cmd++;

	/* copy variable name */
	p = name;
	while(*cmd != '\0' && char_is_one_of(ENV_VAR_NAME_CHARS, *cmd) &&
			*cmd != '.' && *cmd != '=' && p - name < sizeof(name) - 1)
	{
		if(*cmd != '_' && !isalnum(*cmd))
		{
			text_buffer_add("Incorrect variable name");
			return -1;
		}
		*p++ = *cmd++;
	}
	/* test for empty variable name */
	if(p == name)
	{
		text_buffer_addf("%s: %s", "Unsupported variable name", "empty name");
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

	/* check for equal sign and skip it */
	if(*cmd != '=')
	{
		text_buffer_addf("%s: %s", "Incorrect :let statement", "'=' expected");
		return -1;
	}

	parsing_error = parse(cmd + 1, &res_var);
	if(parsing_error != PE_NO_ERROR)
	{
		report_parsing_error(parsing_error);
		return -1;
	}

	if(get_last_position() != NULL && *get_last_position() != '\0')
	{
		text_buffer_addf("%s: %s", "Incorrect :let statement",
				"trailing characters");
		return -1;
	}

	/* update environment variable */
	str_var = var_to_string(res_var);
	if(append)
		append_envvar(name, str_var);
	else
		set_envvar(name, str_var);
	free(str_var);

	var_free(res_var);

	return 0;
}

static void
report_parsing_error(ParsingErrors error)
{
	if(error == PE_INVALID_EXPRESSION)
	{
		text_buffer_addf("%s: %s", "Invalid expression", get_last_position());
	}
	else if(error == PE_MISSING_QUOTE)
	{
		text_buffer_addf("%s: %s", "Invalid :let expression (missing quote)",
				get_last_position());
	}
	else
	{
		assert(0 && "Unexpected parsing error code.");
	}
}

static void
append_envvar(const char *name, const char *val)
{
	envvar_t *record;
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
		text_buffer_add("Not enough memory");
		return;
	}
	record->val = p;

	strcat(record->val, val);
	env_set(name, record->val);
}

static void
set_envvar(const char *name, const char *val)
{
	envvar_t *record;
	char *p;

	record = get_record(name);
	if(record == NULL)
	{
		text_buffer_add("Not enough memory");
		return;
	}

	p = strdup(val);
	if(p == NULL)
	{
		text_buffer_add("Not enough memory");
		return;
	}
	free(record->val);
	record->val = p;
	env_set(name, val);
}

/* searches for variable and creates new record if it didn't existed */
static envvar_t *
get_record(const char *name)
{
	envvar_t *p = NULL;
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
		envvar_t *record;

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
		while(*cmd != '\0' && char_is_one_of(ENV_VAR_NAME_CHARS, *cmd) &&
				p - name < sizeof(name) - 1)
			*p++ = *cmd++;
		*p = '\0';

		if(*cmd != '\0' && !isspace(*cmd))
		{
			text_buffer_add("Trailing characters");
			error++;
			break;
		}

		cmd = skip_whitespace(cmd);

		/* currently we support only environment variables */
		if(!envvar)
		{
			text_buffer_addf("%s: %s", "Unsupported variable type", name);

			cmd = skip_non_whitespace(cmd);
			error++;
			continue;
		}

		/* test for empty variable name */
		if(name[0] == '\0')
		{
			text_buffer_addf("%s: %s", "Unsupported variable name", "empty name");
			error++;
			continue;
		}

		record = find_record(name);
		if(record == NULL || record->removed)
		{
			text_buffer_addf("%s: %s", "No such variable", name);
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
static envvar_t *
find_record(const char *name)
{
	int i;
	for(i = 0; i < nvars; i++)
	{
		if(vars[i].name != NULL && stroscmp(vars[i].name, name) == 0)
			return &vars[i];
	}
	return NULL;
}

static void
free_record(envvar_t *record)
{
	free(record->initial);
	free(record->name);
	free(record->val);

	clear_record(record);
}

static void
clear_record(envvar_t *record)
{
	record->initial = NULL;
	record->name = NULL;
	record->val = NULL;
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
		if(strnoscmp(vars[i].name, cmd, len) == 0)
			add_completion(vars[i].name);
	}
	completion_group_end();
	add_completion(cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
