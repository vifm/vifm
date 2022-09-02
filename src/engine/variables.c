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

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h>
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* strcmp() */

#include "../compat/reallocarray.h"
#include "../utils/env.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "completion.h"
#include "options.h"
#include "parsing.h"
#include "text_buffer.h"
#include "var.h"

#define VAR_NAME_MAX 64
#define VAL_LEN_MAX 2048

/* Types of supported variables. */
typedef enum
{
	VT_ENVVAR,        /* Environment variable. */
	VT_ANY_OPTION,    /* Global and local options (if local exists). */
	VT_GLOBAL_OPTION, /* Global option. */
	VT_LOCAL_OPTION,  /* Local option. */
}
VariableType;

/* Supported operations. */
typedef enum
{
	VO_ASSIGN, /* Assigning a variable (=). */
	VO_APPEND, /* Appending to a string (.=). */
	VO_ADD,    /* Adding to numbers or composite values (+=). */
	VO_SUB,    /* Subtracting from numbers or removing from composites (-=). */
}
VariableOperation;

/* Description of a builtin variable. */
typedef struct
{
	char *name; /* Name of the variable (including "v:" prefix). */
	var_t val;  /* Current value of the variable. */
}
builtinvar_t;

typedef struct
{
	char *name;
	char *val;
	char *initial;
	int from_parent;
	int removed;
}
envvar_t;

const char ENV_VAR_NAME_FIRST_CHAR[] = "abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
const char ENV_VAR_NAME_CHARS[] = "abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

static void init_var(const char name[], const char value[]);
static void clear_builtinvars(void);
static int extract_name(const char **in, VariableType *type, size_t buf_len,
		char buf[]);
static int extract_op(const char **in, VariableOperation *vo);
static int parse_name(const char **in, const char first[], const char other[],
		size_t buf_len, char buf[]);
static int is_valid_op(const char name[], VariableType vt,
		VariableOperation vo);
static int perform_op(const char name[], VariableType vt,
		VariableOperation vo, const char value[]);
static void append_envvar(const char *name, const char *val);
static void set_envvar(const char *name, const char *val);
static int perform_opt_op(const char name[], VariableType vt,
		VariableOperation vo, const char value[]);
static envvar_t * get_record(const char *name);
static char * skip_non_whitespace(const char str[]);
static envvar_t * find_record(const char *name);
static void free_record(envvar_t *record);
static void clear_record(envvar_t *record);
static void complete_envvars(const char var[], const char **start);
static void complete_builtinvars(const char var[], const char **start);

static int initialized;
static envvar_t *vars;
static size_t nvars;
/* List of builtin variables. */
static builtinvar_t *builtin_vars;
/* Number of builtin variables. */
static size_t nbuiltins;

void
init_variables(void)
{
	int env_count;
	char **env_lst;

	if(nvars > 0)
	{
		clear_envvars();
	}

	env_lst = env_list(&env_count);
	if(env_count > 0)
	{
		int i;

		/* Allocate memory for environment variables. */
		vars = reallocarray(NULL, env_count, sizeof(*vars));
		assert(vars != NULL && "Failed to allocate memory for env vars.");

		/* Initialize variable list. */
		for(i = 0; i < env_count; ++i)
		{
			init_var(env_lst[i], env_get(env_lst[i]));
		}
	}

	free_string_array(env_lst, env_count);

	init_parser(&local_getenv);

	initialized = 1;
}

const char *
local_getenv(const char envname[])
{
	envvar_t *record = find_record(envname);
	return (record == NULL || record->removed) ? "" : record->val;
}

/* Initializes environment variable inherited from parent process. */
static void
init_var(const char name[], const char value[])
{
	envvar_t *const record = get_record(name);
	if(record == NULL)
	{
		return;
	}

	record->from_parent = 1;

	(void)replace_string(&record->initial, value);
	(void)replace_string(&record->val, value);
	if(record->initial == NULL || record->val == NULL)
	{
		free_record(record);
	}
}

void
clear_variables(void)
{
	assert(initialized);

	clear_builtinvars();
	clear_envvars();
}

/* Clears the list of builtin variables. */
static void
clear_builtinvars(void)
{
	size_t i;
	for(i = 0U; i < nbuiltins; ++i)
	{
		free(builtin_vars[i].name);
		var_free(builtin_vars[i].val);
	}
	nbuiltins = 0U;
	free(builtin_vars);
	builtin_vars = NULL;
}

void
clear_envvars(void)
{
	size_t i;
	assert(initialized);

	/* Free memory. */
	for(i = 0U; i < nvars; ++i)
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
let_variables(const char cmd[])
{
	char name[VAR_NAME_MAX + 1];
	int error;
	var_t res_var;
	char *str_val;
	ParsingErrors parsing_error;
	VariableType type;
	VariableOperation op;

	assert(initialized);

	if(extract_name(&cmd, &type, sizeof(name), name) != 0)
	{
		return 1;
	}

	cmd = skip_whitespace(cmd);

	if(extract_op(&cmd, &op) != 0)
	{
		return -1;
	}

	cmd = skip_whitespace(cmd);

	parsing_error = parse(cmd, 1, &res_var);
	if(parsing_error != PE_NO_ERROR)
	{
		report_parsing_error(parsing_error);
		return -1;
	}

	if(get_last_position() != NULL && *get_last_position() != '\0')
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Incorrect :let statement",
				"trailing characters");
		return -1;
	}

	if(!is_valid_op(name, type, op))
	{
		vle_tb_append_linef(vle_err, "Wrong variable type for this operation");
		return -1;
	}

	str_val = var_to_str(res_var);

	error = perform_op(name, type, op, str_val);

	free(str_val);
	var_free(res_var);

	return error;
}

/* Extracts name from the string.  Returns zero on success, otherwise non-zero
 * is returned. */
static int
extract_name(const char **in, VariableType *type, size_t buf_len, char buf[])
{
	int error;

	/* Copy variable name. */
	if(**in == '$')
	{
		++*in;
		error = (parse_name(in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
				buf_len, buf) != 0);
		*type = VT_ENVVAR;
	}
	else if(**in == '&')
	{
		++*in;

		*type = VT_ANY_OPTION;
		if(((*in)[0] == 'l' || (*in)[0] == 'g') && (*in)[1] == ':')
		{
			*type = (*in[0] == 'l') ? VT_LOCAL_OPTION : VT_GLOBAL_OPTION;
			*in += 2;
		}

		error = (parse_name(in, OPT_NAME_FIRST_CHAR, OPT_NAME_CHARS, buf_len,
					buf) != 0);
	}
	else
	{
		/* Currently we support only environment variables and options. */
		vle_tb_append_line(vle_err, "Incorrect variable type");
		return -1;
	}
	if(error)
	{
		vle_tb_append_line(vle_err, "Incorrect variable name");
		return -1;
	}

	/* Test for empty variable name. */
	if(buf[0] == '\0')
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Unsupported variable name",
				"empty name");
		return -1;
	}

	return 0;
}

/* Extracts operation from the string.  Sets operation advancing the
 * pointer if needed.  Returns zero on success, otherwise non-zero returned. */
static int
extract_op(const char **in, VariableOperation *vo)
{
	/* Check first operator char and skip it. */
	if(**in == '.')
	{
		++*in;
		*vo = VO_APPEND;
	}
	else if(**in == '+')
	{
		*vo = VO_ADD;
		++*in;
	}
	else if(**in == '-')
	{
		*vo = VO_SUB;
		++*in;
	}
	else
	{
		*vo = VO_ASSIGN;
	}

	/* Check for equal sign and skip it. */
	if(**in != '=')
	{
		vle_tb_append_linef(vle_err, "%s: '=' expected at %s",
				"Incorrect :let statement", *in);
		return 1;
	}
	++*in;

	return 0;
}

/* Parses name of the form `first { other }`.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
parse_name(const char **in, const char first[], const char other[],
		size_t buf_len, char buf[])
{
	if(buf_len == 0UL || !char_is_one_of(first, **in))
	{
		return 1;
	}

	buf[0] = '\0';

	do
	{
		strcatch(buf, **in);
		++*in;
	}
	while(--buf_len > 1UL && char_is_one_of(other, **in));

	return 0;
}

/* Validates operation on a specific variable type.  Returns non-zero for valid
 * operation, otherwise zero is returned. */
static int
is_valid_op(const char name[], VariableType vt, VariableOperation vo)
{
	opt_t *opt;

	if(vt == VT_ENVVAR)
	{
		return (vo == VO_ASSIGN || vo == VO_APPEND);
	}

	opt = vle_opts_find(name, OPT_GLOBAL);
	if(opt == NULL)
	{
		/* Let this error be handled somewhere else. */
		return 1;
	}

	if(opt->type == OPT_BOOL)
	{
		return 0;
	}

	if(opt->type == OPT_STR)
	{
		return (vo == VO_ASSIGN || vo == VO_APPEND);
	}

	return (vo == VO_ASSIGN || vo == VO_ADD || vo == VO_SUB);
}

/* Performs operation on a value.  Returns zero on success, otherwise non-zero
 * is returned. */
static int
perform_op(const char name[], VariableType vt, VariableOperation vo,
		const char value[])
{
	if(vt == VT_ENVVAR)
	{
		/* Update environment variable. */
		if(vo == VO_APPEND)
		{
			append_envvar(name, value);
		}
		else
		{
			set_envvar(name, value);
		}
		return 0;
	}

	/* Update an option. */

	if(vt == VT_ANY_OPTION || vt == VT_LOCAL_OPTION)
	{
		if(perform_opt_op(name, vt, vo, value) != 0)
		{
			return 1;
		}
	}

	if(vt == VT_ANY_OPTION || vt == VT_GLOBAL_OPTION)
	{
		if(perform_opt_op(name, VT_GLOBAL_OPTION, vo, value) != 0)
		{
			return 1;
		}
	}

	return 0;
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
		vle_tb_append_line(vle_err, "Not enough memory");
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
		vle_tb_append_line(vle_err, "Not enough memory");
		return;
	}

	p = strdup(val);
	if(p == NULL)
	{
		vle_tb_append_line(vle_err, "Not enough memory");
		return;
	}
	free(record->val);
	record->val = p;
	env_set(name, val);
}

/* Performs operation on an option.  Returns zero on success, otherwise non-zero
 * is returned. */
static int
perform_opt_op(const char name[], VariableType vt, VariableOperation vo,
		const char value[])
{
	OPT_SCOPE scope = (vt == VT_ANY_OPTION || vt == VT_LOCAL_OPTION)
	                ? OPT_LOCAL
	                : OPT_GLOBAL;

	opt_t *const opt = vle_opts_find(name, scope);
	if(opt == NULL)
	{
		if(vt == VT_ANY_OPTION)
		{
			return 0;
		}

		vle_tb_append_linef(vle_err, "Unknown %s option name: %s",
				(scope == OPT_LOCAL) ? "local" : "global", name);
		return 1;
	}

	switch(vo)
	{
		case VO_ASSIGN:
			if(vle_opt_assign(opt, value) != 0)
			{
				return 1;
			}
			break;
		case VO_ADD:
		case VO_APPEND:
			if(vle_opt_add(opt, value) != 0)
			{
				return 1;
			}
			break;
		case VO_SUB:
			if(vle_opt_remove(opt, value) != 0)
			{
				return 1;
			}
			break;
	}
	return 0;
}

/* Searches for variable and creates new record if it doesn't exist yet.
 * Returns a record or NULL on error.*/
static envvar_t *
get_record(const char *name)
{
	envvar_t *p = NULL;
	size_t i;

	/* Search for existing variable. */
	for(i = 0U; i < nvars; ++i)
	{
		if(vars[i].name == NULL)
			p = &vars[i];
		else if(strcmp(vars[i].name, name) == 0)
			return &vars[i];
	}

	if(p == NULL)
	{
		/* Try to reallocate list of variables. */
		p = realloc(vars, sizeof(*vars)*(nvars + 1U));
		if(p == NULL)
			return NULL;
		vars = p;
		p = &vars[nvars];
		++nvars;
	}

	/* Initialize new record. */
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
	return p;
}

int
unlet_variables(const char cmd[])
{
	int error = 0;
	assert(initialized);

	while(*cmd != '\0')
	{
		envvar_t *record;

		char name[VAR_NAME_MAX + 1];
		char *p;
		int envvar = 1;

		/* Check if it's environment variable. */
		if(*cmd != '$')
			envvar = 0;
		else
			cmd++;

		/* Copy variable name. */
		p = name;
		while(*cmd != '\0' && char_is_one_of(ENV_VAR_NAME_CHARS, *cmd) &&
				(size_t)(p - name) < sizeof(name) - 1)
		{
			*p++ = *cmd++;
		}
		*p = '\0';

		if(*cmd != '\0' && !isspace(*cmd))
		{
			vle_tb_append_line(vle_err, "Trailing characters");
			error++;
			break;
		}

		cmd = skip_whitespace(cmd);

		/* Currently we support only environment variables. */
		if(!envvar)
		{
			vle_tb_append_linef(vle_err, "%s: %s", "Unsupported variable type", name);

			cmd = skip_non_whitespace(cmd);
			error++;
			continue;
		}

		/* Test for empty variable name. */
		if(name[0] == '\0')
		{
			vle_tb_append_linef(vle_err, "%s: %s", "Unsupported variable name",
					"empty name");
			error++;
			continue;
		}

		record = find_record(name);
		if(record == NULL || record->removed)
		{
			vle_tb_append_linef(vle_err, "%s: %s", "No such variable", name);
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

/* Skips consecutive non-whitespace characters.  Returns pointer to the next
 * character in the str. */
static char *
skip_non_whitespace(const char str[])
{
	while(!isspace(*str) && *str != '\0')
	{
		++str;
	}
	return (char *)str;
}

/* searches for existent variable */
static envvar_t *
find_record(const char *name)
{
	size_t i;
	for(i = 0U; i < nvars; ++i)
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
complete_variables(const char var[], const char **start)
{
	assert(initialized && "Variables unit is not initialized.");

	if(var[0] == '$')
	{
		complete_envvars(var, start);
	}
	else if(var[0] == 'v' && var[1] == ':')
	{
		complete_builtinvars(var, start);
	}
	else
	{
		*start = var;
		vle_compl_add_match(var, "");
	}
}

/* Completes environment variable name.  var should point to "$...".  *start is
 * set to completion insertion position in var. */
static void
complete_envvars(const char var[], const char **start)
{
	size_t i;
	size_t len;

	++var;
	*start = var;

	/* Add all variables that start with given beginning. */
	len = strlen(var);
	for(i = 0U; i < nvars; ++i)
	{
		if(vars[i].name == NULL)
			continue;
		if(vars[i].removed)
			continue;
		if(strnoscmp(vars[i].name, var, len) == 0)
			vle_compl_add_match(vars[i].name, vars[i].val);
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(var);
}

/* Completes builtin variable name.  var should point to "v:...".  *start is
 * set to completion insertion position in var. */
static void
complete_builtinvars(const char var[], const char **start)
{
	size_t i;
	size_t len;

	*start = var;

	/* Add all variables that start with given beginning. */
	len = strlen(var);
	for(i = 0U; i < nbuiltins; ++i)
	{
		if(strncmp(builtin_vars[i].name, var, len) == 0)
		{
			char *const str_val = var_to_str(builtin_vars[i].val);
			vle_compl_add_match(builtin_vars[i].name, str_val);
			free(str_val);
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(var);
}

var_t
getvar(const char varname[])
{
	size_t i;
	for(i = 0U; i < nbuiltins; ++i)
	{
		if(strcmp(builtin_vars[i].name, varname) == 0)
		{
			return builtin_vars[i].val;
		}
	}

	return var_error();
}

int
setvar(const char varname[], var_t val)
{
	builtinvar_t new_var;
	size_t i;
	void *p;

	if(!starts_with_lit(varname, "v:"))
	{
		return 1;
	}

	/* Initialize new variable before doing anything else. */
	new_var.name = strdup(varname);
	new_var.val = var_clone(val);
	if(new_var.name == NULL || new_var.val.type == VTYPE_ERROR)
	{
		free(new_var.name);
		var_free(new_var.val);
		return 1;
	}

	/* Search for existing variable. */
	for(i = 0U; i < nbuiltins; ++i)
	{
		if(strcmp(builtin_vars[i].name, varname) == 0)
		{
			free(builtin_vars[i].name);
			var_free(builtin_vars[i].val);
			builtin_vars[i] = new_var;
			return 0;
		}
	}

	/* Try to reallocate list of variables. */
	p = realloc(builtin_vars, sizeof(*builtin_vars)*(nbuiltins + 1U));
	if(p == NULL)
	{
		free(new_var.name);
		var_free(new_var.val);
		return 1;
	}
	builtin_vars = p;
	builtin_vars[nbuiltins] = new_var;
	++nbuiltins;

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
