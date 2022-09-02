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

#include "options.h"

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h>
#include <stdlib.h> /* realloc() */
#include <string.h> /* memset() strchr() strcmp() strcpy() strlen() */

#include "../compat/reallocarray.h"
#include "../utils/str.h"
#include "completion.h"
#include "text_buffer.h"

#define LOWER_CHARS "abcdefghijklmnopqrstuvwxyz"
const char OPT_NAME_FIRST_CHAR[] = LOWER_CHARS;
const char OPT_NAME_CHARS[] = LOWER_CHARS;
#undef LOWER_CHARS

/* TODO: possibly add validators (e.g. to reject negative values). */
/* TODO: possibly add default handlers (just set new value) and types
 *       OPT_*_PTR. */

/* List of operations on options of set type for the set_op(...) function. */
typedef enum
{
	SO_SET,    /* Set value of the option. */
	SO_ADD,    /* Add item(s) to a set. */
	SO_REMOVE, /* Remove item(s) from a set. */
	SO_TOGGLE, /* Toggle item(s) in a set. */
}
SetOp;

/* Type of function accepted by the for_each_char_of() function. */
typedef void (*mod_t)(char buffer[], char value);

/* Type of callback for the enum_options() function. */
typedef void (*opt_traverse)(opt_t *opt);

static void do_nothing_handler(const char name[], optval_t val,
		OPT_SCOPE scope);
static void reset_options(OPT_SCOPE scope);
static opt_t * add_option_inner(const char name[], const char descr[],
		OPT_TYPE type, OPT_SCOPE scope, int val_count, const char *vals[][2],
		opt_handler handler);
static void print_if_changed(opt_t *opt);
static int process_option(const char arg[], OPT_SCOPE real_scope,
		OPT_SCOPE scope, int *print);
static int handle_all_pseudo(const char arg[], const char suffix[],
		OPT_SCOPE scope, int *print);
static void set_print_void(opt_t *opt);
static void enum_options(OPT_SCOPE scope, opt_traverse traverser);
static void enum_unique_options(opt_traverse traverser);
static opt_t * get_option(const char option[], OPT_SCOPE scope);
static opt_t * pick_option(opt_t *opts[2], OPT_SCOPE scope);
static int set_inv(opt_t *opt);
static int set_reset(opt_t *opt);
static int uses_str_value(OPT_TYPE type);
static int set_hat(opt_t *opt, const char value[]);
static void notify_option_update(opt_t *opt, OPT_OP op, optval_t val);
static int set_op(opt_t *opt, const char value[], SetOp op);
static int charset_set(opt_t *opt, const char value[]);
static int charset_add_all(opt_t *opt, const char value[]);
static void charset_add(char buffer[], char value);
static int charset_remove_all(opt_t *opt, const char value[]);
static void charset_remove(char buffer[], char value);
static int charset_toggle_all(opt_t *opt, const char value[]);
static int charset_do_all(opt_t *opt, mod_t func, const char value[]);
static void charset_toggle(char buffer[], char value);
static void for_each_char_of(char buffer[], mod_t func, const char input[]);
static int replace_if_changed(char **current, const char new[]);
static char * str_add(char old[], const char value[]);
static int str_remove(char old[], const char value[]);
static int find_val(const opt_t *opt, const char value[]);
static int set_print(const opt_t *opt);
static char * extract_option(const char **argsp, int replace);
static char * skip_alphas(const char str[]);
static void complete_option_name(const char buf[], int bool_only, int pseudo,
		OPT_SCOPE scope);
static int option_matches(const opt_t *opt, OPT_SCOPE scope);
static const char * get_opt_full_name(opt_t *opt);
static int complete_option_value(const opt_t *opt, const char beginning[]);
static int complete_list_value(const opt_t *opt, const char beginning[]);
static int complete_char_value(const opt_t *opt, const char beginning[]);

/* Characters that can occur after option values. */
static const char ENDING_CHARS[] = "!?&";
/* Characters that can occur between option names and values. */
static const char MIDDLE_CHARS[] = "^+-=:";

static int *opts_changed;
/* Handler to call on setting (not necessary changing) of any option. */
static opt_uni_handler uni_handler;

static opt_t *options;
static size_t option_count;

void
vle_opts_init(int *opts_changed_flag, opt_uni_handler universal_handler)
{
	assert(opts_changed_flag != NULL);

	opts_changed = opts_changed_flag;
	uni_handler = (universal_handler == NULL)
	            ? &do_nothing_handler
	            : universal_handler;
}

/* Just to omit checks of uni_handler for NULL. */
static void
do_nothing_handler(const char name[], optval_t val, OPT_SCOPE scope)
{
	/* Do nothing. */
}

void
vle_opts_restore_default(const char name[], OPT_SCOPE scope)
{
	opt_t *opt = vle_opts_find(name, scope);
	if(opt != NULL)
	{
		set_reset(opt);
	}
}

void
vle_opts_restore_defaults(void)
{
	reset_options(OPT_ANY);
}

static void
reset_options(OPT_SCOPE scope)
{
	size_t i;
	for(i = 0U; i < option_count; ++i)
	{
		if(options[i].full == NULL && option_matches(&options[i], scope))
		{
			set_reset(&options[i]);
		}
	}
}

void
vle_opts_reset(void)
{
	size_t i;
	for(i = 0U; i < option_count; ++i)
	{
		free(options[i].name);
		if(uses_str_value(options[i].type))
		{
			/* Abbreviations share default value with full option. */
			if(options[i].full == NULL)
			{
				free(options[i].def.str_val);
			}
			free(options[i].val.str_val);
		}
	}
	free(options);
	options = NULL;
	option_count = 0U;
}

void
vle_opts_add(const char name[], const char abbr[], const char descr[],
		OPT_TYPE type, OPT_SCOPE scope, int val_count, const char *vals[][2],
		opt_handler handler, optval_t def)
{
	opt_t *full;

	assert(name != NULL);
	assert(abbr != NULL);

	full = add_option_inner(name, descr, type, scope, val_count, vals, handler);
	if(full == NULL)
	{
		return;
	}

	if(abbr[0] != '\0')
	{
		/* Save pointer to name used in this module for use below. */
		const char *const full_name = full->name;

		opt_t *abbreviated;
		abbreviated = add_option_inner(abbr, descr, type, scope, val_count, vals,
				handler);
		if(abbreviated != NULL)
		{
			abbreviated->full = full_name;
		}

		/* Inserting an option invalidates pointers to other options, so reload
		 * it. */
		full = vle_opts_find(full_name, scope);
	}

	if(uses_str_value(type))
	{
		full->def.str_val = strdup(def.str_val);
		full->val.str_val = strdup(def.str_val);
	}
	else
	{
		full->def.int_val = def.int_val;
		full->val.int_val = def.int_val;
	}

#ifndef NDEBUG
	switch(type)
	{
		int i;

		case OPT_CHARSET:
			assert((size_t)full->val_count == 1U + strlen(full->vals[0][0]) &&
					"Number of character set items is incorrect.");
			for(i = 1; i < full->val_count; ++i)
			{
				assert(full->vals[i][0][0] != '\0' && full->vals[i][0][1] == '\0' &&
						"Incorrect character set item.");
			}
			break;

		default:
			/* No checks for other types, at least for now. */
			break;
	}
#endif
}

/* Adds option to internal array of option descriptors.  Returns a pointer to
 * the descriptor or NULL on out of memory error. */
static opt_t *
add_option_inner(const char name[], const char descr[], OPT_TYPE type,
		OPT_SCOPE scope, int val_count, const char *vals[][2], opt_handler handler)
{
	opt_t *p;

	assert(scope != OPT_ANY && "OPT_ANY can't be an option type.");
	assert(vle_opts_find(name, scope) == NULL && "Duplicated option.");

	p = reallocarray(options, option_count + 1, sizeof(*options));
	if(p == NULL)
	{
		return NULL;
	}
	options = p;
	option_count++;

	p = options + option_count - 1;

	while((p - 1) >= options && strcmp((p - 1)->name, name) > 0)
	{
		*p = *(p - 1);
		--p;
	}

	p->name = strdup(name);
	p->descr = descr;
	p->type = type;
	p->scope = scope;
	p->handler = handler;
	p->val_count = val_count;
	p->vals = vals;
	memset(&p->val, 0, sizeof(p->val));
	p->full = NULL;
	return p;
}

void
vle_opts_assign(const char name[], optval_t val, OPT_SCOPE scope)
{
	opt_t *opt = vle_opts_find(name, scope);
	assert(opt != NULL && "Wrong option name.");

	if(uses_str_value(opt->type))
	{
		(void)replace_string(&opt->val.str_val, val.str_val);
	}
	else
	{
		opt->val = val;
	}
}

int
vle_opts_set(const char args[], OPT_SCOPE scope)
{
	int errors = 0;

	if(*args == '\0')
	{
		enum_options(scope, &print_if_changed);
		return 0;
	}

	while(*args != '\0')
	{
		int print;

		char *const opt = extract_option(&args, 1);
		if(args == NULL || opt == NULL)
		{
			free(opt);
			return -1;
		}

		if(*args == '\0' && *opt == '\0')
		{
			/* Stop on reaching comment. */
			free(opt);
			break;
		}

		if(scope == OPT_ANY)
		{
			const int error = process_option(opt, scope, OPT_LOCAL, &print);
			errors += error;
			if(!print && !error)
			{
				errors += process_option(opt, scope, OPT_GLOBAL, &print);
			}
		}
		else
		{
			errors += process_option(opt, scope, scope, &print);
		}

		free(opt);
	}
	return errors;
}

/* Prints the option if its current value differs from its default value. */
static void
print_if_changed(opt_t *opt)
{
	if(uses_str_value(opt->type))
	{
		if(strcmp(opt->val.str_val, opt->def.str_val) == 0)
		{
			return;
		}
	}
	else if(opt->val.int_val == opt->def.int_val)
	{
		return;
	}

	(void)set_print(opt);
}

/* Processes one :set statement.  Returns zero on success, otherwise non-zero is
 * returned. */
static int
process_option(const char arg[], OPT_SCOPE real_scope, OPT_SCOPE scope,
		int *print)
{
	int err;
	const char *suffix;
	opt_t *opt;
	char *optname;

	*print = 0;

	suffix = skip_alphas(arg);

	optname = format_str("%.*s", (int)(suffix - arg), arg);

	if(strcmp(optname, "all") == 0)
	{
		free(optname);
		return handle_all_pseudo(arg, suffix, real_scope, print);
	}

	opt = get_option(optname, scope);
	if(opt == NULL)
	{
		/* Silently return on attempts to set global-only option with :setlocal. */
		if(scope == OPT_LOCAL && get_option(optname, OPT_GLOBAL) != NULL)
		{
			free(optname);
			return 0;
		}

		if(optname[0] == '\0')
		{
			vle_tb_append_linef(vle_err, "%s: %s", "No valid option name in", arg);
		}
		else
		{
			vle_tb_append_linef(vle_err, "%s: %s", "Unknown option", optname);
		}
		free(optname);
		return 1;
	}

	err = 0;
	if(*suffix == '\0')
	{
		opt_t *o = vle_opts_find(optname, scope);
		if(o != NULL)
		{
			if(o->type == OPT_BOOL)
				err = vle_opt_on(opt);
			else
			{
				*print = 1;
				err = set_print(o);
			}
		}
		else if(strncmp(optname, "no", 2) == 0)
		{
			err = vle_opt_off(opt);
		}
		else if(strncmp(optname, "inv", 3) == 0)
		{
			err = set_inv(opt);
		}
	}
	else if(char_is_one_of(ENDING_CHARS, *suffix))
	{
		if(*(suffix + 1) != '\0')
		{
			vle_tb_append_linef(vle_err, "%s: %s", "Trailing characters", arg);
			free(optname);
			return 1;
		}
		if(*suffix == '!')
			err = set_inv(opt);
		else if(*suffix == '?')
		{
			*print = 1;
			err = set_print(opt);
		}
		else
			err = set_reset(opt);
	}
	else if(strncmp(suffix, "+=", 2) == 0)
	{
		err = vle_opt_add(opt, suffix + 2);
	}
	else if(strncmp(suffix, "-=", 2) == 0)
	{
		err = vle_opt_remove(opt, suffix + 2);
	}
	else if(strncmp(suffix, "^=", 2) == 0)
	{
		err = set_hat(opt, suffix + 2);
	}
	else if(*suffix == '=' || *suffix == ':')
	{
		err = vle_opt_assign(opt, suffix + 1);
	}
	else
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Trailing characters", arg);
	}

	if(err)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid argument", arg);
	}
	free(optname);
	return err;
}

/* Handles "all" pseudo-option.  Actual action is determined by the suffix.
 * Returns zero on success, otherwise non-zero is returned. */
static int
handle_all_pseudo(const char arg[], const char suffix[], OPT_SCOPE scope,
		int *print)
{
	*print = 0;

	switch(*suffix)
	{
		case '\0':
			*print = 1;
			enum_options(scope, &set_print_void);
			return 0;
		case '&':
			reset_options(scope);
			return 0;

		default:
			vle_tb_append_linef(vle_err, "%s: %s", "Trailing characters", arg);
			return 1;
	}
}

/* Wrapper for set_print() to make its prototype compatible with
 * enum_options(). */
static void
set_print_void(opt_t *opt)
{
	(void)set_print(opt);
}

/* Enumerates options of specified scope calling the traverser for each one. */
static void
enum_options(OPT_SCOPE scope, opt_traverse traverser)
{
	size_t i;

	if(scope == OPT_ANY)
	{
		enum_unique_options(traverser);
		return;
	}

	for(i = 0U; i < option_count; ++i)
	{
		if(options[i].full == NULL && options[i].scope == scope)
		{
			traverser(&options[i]);
		}
	}
}

/* Enumerates options preferring local options to global ones on calling the
 * traverser. */
static void
enum_unique_options(opt_traverse traverser)
{
	size_t g = 0U, l = 0U;

	do
	{
		while(g < option_count &&
				(options[g].scope != OPT_GLOBAL || options[g].full != NULL))
		{
			++g;
		}
		while(l < option_count &&
				(options[l].scope != OPT_LOCAL || options[l].full != NULL))
		{
			++l;
		}

		if(l < option_count && g < option_count)
		{
			const int cmp = strcmp(options[l].name, options[g].name);
			if(cmp == 0)
			{
				traverser(&options[l]);
				++l;
				++g;
			}
			else
			{
				traverser(&options[(cmp < 0) ? l++ : g++]);
			}
		}
		else if(l < option_count)
		{
			traverser(&options[l++]);
		}
		else if(g < option_count)
		{
			traverser(&options[g++]);
		}
	}
	while(g < option_count || l < option_count);
}

/* Finds option by its name.  Understands "no" and "inv" boolean option
 * prefixes.  Returns NULL for unknown option names. */
static opt_t *
get_option(const char option[], OPT_SCOPE scope)
{
	opt_t *opt;

	opt = vle_opts_find(option, scope);
	if(opt != NULL)
		return opt;

	if(strncmp(option, "no", 2) == 0)
		return vle_opts_find(option + 2, scope);
	else if(strncmp(option, "inv", 3) == 0)
		return vle_opts_find(option + 3, scope);
	else
		return NULL;
}

opt_t *
vle_opts_find(const char option[], OPT_SCOPE scope)
{
	int l = 0, u = option_count - 1;
	while(l <= u)
	{
		const size_t i = l + (u - l)/2;
		const int comp = strcmp(option, options[i].name);
		if(comp == 0)
		{
			opt_t *opt;
			opt_t *opts[2];

			opts[0] = &options[i];

			if(i > 0 && strcmp(option, options[i - 1].name) == 0)
			{
				opts[1] = &options[i - 1];
			}
			else if(i < option_count - 1 && strcmp(option, options[i + 1].name) == 0)
			{
				opts[1] = &options[i + 1];
			}
			else
			{
				opts[1] = NULL;
			}

			opt = pick_option(opts, scope);
			if(opt != NULL && opt->full != NULL)
			{
				return vle_opts_find(opt->full, scope);
			}
			return opt;
		}
		else if(comp < 0)
		{
			u = i - 1;
		}
		else
		{
			l = i + 1;
		}
	}
	return NULL;
}

/* Of two options (global and local, when second exists) picks the one that
 * matches the scope in a best way (exact match is preferred as well as local
 * option over global one).  Returns picked option or NULL on no match. */
static opt_t *
pick_option(opt_t *opts[2], OPT_SCOPE scope)
{
	if(opts[1] == NULL)
	{
		if(option_matches(opts[0], scope))
		{
			return opts[0];
		}
		return NULL;
	}

	if(opts[0]->scope == OPT_LOCAL)
	{
		opt_t *const opt = opts[0];
		opts[0] = opts[1];
		opts[1] = opt;
	}

	switch(scope)
	{
		case OPT_GLOBAL:
			return opts[0];
		case OPT_LOCAL:
		case OPT_ANY:
			return opts[1];

		default:
			assert(0 && "Unhandled scope?");
			return opts[1];
	}
}

int
vle_opt_on(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(!opt->val.bool_val)
	{
		opt->val.bool_val = 1;
		notify_option_update(opt, OP_ON, opt->val);
	}
	uni_handler(opt->name, opt->val, opt->scope);

	return 0;
}

int
vle_opt_off(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(opt->val.bool_val)
	{
		opt->val.bool_val = 0;
		notify_option_update(opt, OP_OFF, opt->val);
	}
	uni_handler(opt->name, opt->val, opt->scope);

	return 0;
}

/* Inverts value of boolean option.  Returns non-zero in case of error. */
static int
set_inv(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	opt->val.bool_val = !opt->val.bool_val;
	notify_option_update(opt, opt->val.bool_val ? OP_ON : OP_OFF, opt->val);
	uni_handler(opt->name, opt->val, opt->scope);

	return 0;
}

int
vle_opt_assign(opt_t *opt, const char value[])
{
	if(opt->type == OPT_BOOL)
		return -1;

	if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, SO_SET))
		{
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_ENUM)
	{
		int i = find_val(opt, value);
		if(i == -1)
			return -1;

		if(opt->val.enum_item != i)
		{
			opt->val.enum_item = i;
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		if(opt->val.str_val == NULL || strcmp(opt->val.str_val, value) != 0)
		{
			(void)replace_string(&opt->val.str_val, value);
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_INT)
	{
		char *end;
		int int_val = strtoll(value, &end, 10);
		if(*end != '\0')
		{
			vle_tb_append_linef(vle_err, "Not a number: %s", value);
			return -1;
		}
		if(opt->val.int_val != int_val)
		{
			opt->val.int_val = int_val;
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_CHARSET)
	{
		const size_t valid_len = strspn(value, *opt->vals[0]);
		if(valid_len != strlen(value))
		{
			vle_tb_append_linef(vle_err, "Illegal character: <%c>", value[valid_len]);
			return -1;
		}
		if(charset_set(opt, value))
		{
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else
	{
		assert(0 && "Unknown type of option.");
	}

	uni_handler(opt->name, opt->val, opt->scope);
	return 0;
}

/* Resets an option to its default value (opt&). */
static int
set_reset(opt_t *opt)
{
	if(uses_str_value(opt->type))
	{
		if(replace_if_changed(&opt->val.str_val, opt->def.str_val))
		{
			notify_option_update(opt, OP_RESET, opt->val);
		}
	}
	else if(opt->val.int_val != opt->def.int_val)
	{
		opt->val.int_val = opt->def.int_val;
		notify_option_update(opt, OP_RESET, opt->val);
	}

	uni_handler(opt->name, opt->val, opt->scope);
	return 0;
}

/* Checks whether given option type allocates memory for strings on heap.
 * Returns non-zero if so, otherwise zero is returned. */
static int
uses_str_value(OPT_TYPE type)
{
	return (type == OPT_STR || type == OPT_STRLIST || type == OPT_CHARSET);
}

int
vle_opt_add(opt_t *opt, const char value[])
{
	if(opt->type != OPT_INT && opt->type != OPT_SET && opt->type != OPT_STRLIST &&
			opt->type != OPT_CHARSET && opt->type != OPT_STR)
		return -1;

	if(opt->type == OPT_INT)
	{
		char *p;
		int i;

		i = strtol(value, &p, 10);
		if(*p != '\0')
			return -1;
		if(i == 0)
			return 0;

		opt->val.int_val += i;
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}
	else if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, SO_ADD))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else if(opt->type == OPT_CHARSET)
	{
		const size_t valid_len = strspn(value, *opt->vals[0]);
		if(valid_len != strlen(value))
		{
			vle_tb_append_linef(vle_err, "Illegal character: <%c>", value[valid_len]);
			return -1;
		}
		if(charset_add_all(opt, value))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else if(*value != '\0')
	{
		if(opt->type == OPT_STR)
		{
			size_t len = strlen(opt->val.str_val);
			if(strappend(&opt->val.str_val, &len, value) != 0)
			{
				vle_tb_append_line(vle_err, "Memory allocation error");
				return -1;
			}
		}
		else
		{
			opt->val.str_val = str_add(opt->val.str_val, value);
		}
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}

	uni_handler(opt->name, opt->val, opt->scope);
	return 0;
}

int
vle_opt_remove(opt_t *opt, const char value[])
{
	if(opt->type != OPT_INT && opt->type != OPT_SET && opt->type != OPT_STRLIST &&
			opt->type != OPT_CHARSET)
		return -1;

	if(opt->type == OPT_INT)
	{
		char *p;
		int i;

		i = strtol(value, &p, 10);
		if(*p != '\0')
			return -1;
		if(i == 0)
			return 0;

		opt->val.int_val -= i;
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}
	else if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, SO_REMOVE))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else if(opt->type == OPT_CHARSET)
	{
		if(charset_remove_all(opt, value))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else if(*value != '\0')
	{
		if(str_remove(opt->val.str_val, value))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}

	uni_handler(opt->name, opt->val, opt->scope);
	return 0;
}

/* Toggles value(s) from the option (^= operator).  Returns zero on success. */
static int
set_hat(opt_t *opt, const char value[])
{
	if(opt->type != OPT_STRLIST && opt->type != OPT_SET &&
			opt->type != OPT_CHARSET)
	{
		return -1;
	}

	if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, SO_TOGGLE))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else if(opt->type == OPT_CHARSET)
	{
		if(charset_toggle_all(opt, value))
		{
			notify_option_update(opt, OP_MODIFIED, opt->val);
		}
	}
	else
	{
		if(!str_remove(opt->val.str_val, value))
		{
			/* Nothing was removed, so add it. */
			opt->val.str_val = str_add(opt->val.str_val, value);
		}
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}

	uni_handler(opt->name, opt->val, opt->scope);
	return 0;
}

/* Calls option handler to notify about option change.  Also updates
 * opts_changed flag. */
static void
notify_option_update(opt_t *opt, OPT_OP op, optval_t val)
{
	*opts_changed = 1;
	opt->handler(op, val);
}

/* Performs set/add/remove operations on options of set kind.  Returns not zero
 * if set was modified. */
static int
set_op(opt_t *opt, const char value[], SetOp op)
{
	const char *p;
	const int old_val = opt->val.set_items;
	int new_val = (op == SO_SET) ? 0 : old_val;

	while(*value != '\0')
	{
		char *val;
		int i;

		p = until_first(value, ',');

		val = format_str("%.*s", (int)(p - value), value);
		i = find_val(opt, val);
		free(val);

		if(i != -1)
		{
			if(op == SO_SET || op == SO_ADD ||
					(op == SO_TOGGLE && (old_val & (1 << i)) == 0))
				new_val |= 1 << i;
			else
				new_val &= ~(1 << i);
		}

		if(*p == '\0')
			break;

		value = p + 1;
		while(*value == ',')
			++value;
	}

	opt->val.set_items = new_val;
	return new_val != old_val;
}

/* Sets new value of an option of type OPT_CHARSET.  Returns non-zero when value
 * of the option was changed, otherwise zero is returned. */
static int
charset_set(opt_t *opt, const char value[])
{
	char new_val[opt->val_count + 1];
	new_val[0] = '\0';

	for_each_char_of(new_val, charset_add, value);
	return replace_if_changed(&opt->val.str_val, new_val);
}

/* Adds set of new items to the value of an option of type OPT_CHARSET.  Returns
 * non-zero when value of the option was changed, otherwise zero is returned. */
static int
charset_add_all(opt_t *opt, const char value[])
{
	return charset_do_all(opt, &charset_add, value);
}

/* Adds an item to the value of an option of type OPT_CHARSET. */
static void
charset_add(char buffer[], char value)
{
	const char value_str[] = { value, '\0' };
	charset_remove(buffer, value);
	strcat(buffer, value_str);
}

/* Removes set of items from the value of an option of type OPT_CHARSET.
 * Returns non-zero when value of the option was changed, otherwise zero is
 * returned. */
static int
charset_remove_all(opt_t *opt, const char value[])
{
	return charset_do_all(opt, &charset_remove, value);
}

/* Removes an item from the value of an option of type OPT_CHARSET. */
static void
charset_remove(char buffer[], char value)
{
	char *l = buffer;
	const char *r = buffer;
	while(*r != '\0')
	{
		if(*r != value)
		{
			*l++ = *r;
		}
		r++;
	}
	*l++ = '\0';
}

/* Toggles items in the option of type OPT_CHARSET.  Returns non-zero when value
 * of the option was changed, otherwise zero is returned. */
static int
charset_toggle_all(opt_t *opt, const char value[])
{
	return charset_do_all(opt, &charset_toggle, value);
}

/* Invokes modification function for the value of the option of type
 * OPT_CHARSET.  Returns non-zero when value of the option was changed,
 * otherwise zero is returned. */
static int
charset_do_all(opt_t *opt, mod_t func, const char value[])
{
	char new_val[opt->val_count + 1];
	copy_str(new_val, sizeof(new_val), opt->val.str_val);
	assert(strlen(opt->val.str_val) <= (size_t)opt->val_count &&
			"Character set includes duplicates?");

	for_each_char_of(new_val, func, value);
	return replace_if_changed(&opt->val.str_val, new_val);
}

/* Toggles items in the value of an option of type OPT_CHARSET. */
static void
charset_toggle(char buffer[], char value)
{
	char *l = buffer;
	const char *r = buffer;
	while(*r != '\0')
	{
		if(*r != value)
		{
			*l++ = *r;
		}
		r++;
	}
	if(l == r)
	{
		*l++ = value;
	}
	*l++ = '\0';
}

/* Calls the func once for each unique character in the input argument, passing
 * the buffer and the character as its arguments. */
static void
for_each_char_of(char buffer[], mod_t func, const char input[])
{
	const char *val = input;
	while(*val != '\0')
	{
		/* Process only unique characters. */
		if(strchr(input, *val) == val)
		{
			func(buffer, *val);
		}
		++val;
	}
}

/* Compares *current and new strings and replaces *current with a copy of the
 * new if they differ.  Returns non-zero when replace was performed, otherwise
 * zero is returned. */
static int
replace_if_changed(char **current, const char new[])
{
	if(strcmp(*current, new) == 0)
	{
		return 0;
	}

	return (replace_string(current, new) == 0) ? 1 : 0;
}

/* Add an element to string list.  Reallocates the memory of old and returns
 * its new address or NULL if there isn't enough memory. */
static char *
str_add(char *old, const char *value)
{
	const size_t old_len = (old == NULL ? 0 : strlen(old));
	char *new = realloc(old, old_len + 1 + strlen(value) + 1);
	if(new == NULL)
		return old;

	if(old_len == 0)
		strcpy(new, "");
	else
		strcat(new, ",");
	strcat(new, value);
	return new;
}

/* Removes an element from string list or does nothing if there is no such
 * element.  Returns non-zero when value of the option is changed, otherwise
 * zero is returned. */
static int
str_remove(char old[], const char value[])
{
	int changed = 0;

	while(*old != '\0')
	{
		char *p;

		p = strchr(old, ',');
		if(p == NULL)
			p = old + strlen(old);

		if(strncmp(old, value, p - old) == 0)
		{
			if(*p == '\0')
				*old = '\0';
			else
				memmove(old, p + 1, strlen(p + 1) + 1);
			p = old;
			changed = 1;
		}

		if(*p == '\0')
			break;

		old = p + 1;
	}
	return changed;
}

/* Returns index of the value in the list of option values, or -1 if value name
 * is wrong. */
static int
find_val(const opt_t *opt, const char value[])
{
	int i;
	for(i = 0; i < opt->val_count; ++i)
	{
		if(strcmp(opt->vals[i][0], value) == 0)
		{
			return i;
		}
	}
	return -1;
}

/* Prints value of the given option to text buffer. */
static int
set_print(const opt_t *opt)
{
	if(opt->type == OPT_BOOL)
	{
		vle_tb_append_linef(vle_err, "%s%s", opt->val.bool_val ? "  " : "no",
				opt->name);
	}
	else
	{
		vle_tb_append_linef(vle_err, "  %s=%s", opt->name, vle_opt_to_string(opt));
	}
	return 0;
}

const char *
vle_opts_get(const char name[], OPT_SCOPE scope)
{
	opt_t *opt = vle_opts_find(name, scope);
	assert(opt != NULL && "Wrong option name.");
	if(opt == NULL)
	{
		return NULL;
	}

	return vle_opt_to_string(opt);
}

const char *
vle_opt_to_string(const opt_t *opt)
{
	static char buf[1024];
	if(opt->type == OPT_BOOL)
	{
		buf[0] = '\0';
	}
	else if(opt->type == OPT_INT)
	{
		snprintf(buf, sizeof(buf), "%d", opt->val.int_val);
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST ||
			opt->type == OPT_CHARSET)
	{
		copy_str(buf, sizeof(buf), opt->val.str_val ? opt->val.str_val : "");
	}
	else if(opt->type == OPT_ENUM)
	{
		copy_str(buf, sizeof(buf), opt->vals[opt->val.enum_item][0]);
	}
	else if(opt->type == OPT_SET)
	{
		int i, first = 1;
		buf[0] = '\0';
		for(i = 0; i < opt->val_count; ++i)
		{
			if(opt->val.set_items & (1 << i))
			{
				char *p = buf + strlen(buf);
				snprintf(p, sizeof(buf) - (p - buf), "%s%s",
						first ? "" : ",", opt->vals[i][0]);
				first = 0;
			}
		}
	}
	else
	{
		assert(0 && "Don't know how to convert value of this type to a string");
	}
	return buf;
}

void
vle_opts_complete(const char args[], const char **start, OPT_SCOPE scope)
{
	int bool_only;
	int is_value_completion;
	opt_t *opt = NULL;
	char *last_opt = strdup("");
	char *p;

	/* Skip all options except the last one. */
	*start = args;
	while(*args != '\0')
	{
		*start = args;

		free(last_opt);
		last_opt = extract_option(&args, 0);

		if(args == NULL || (*args == '\0' && is_null_or_empty(last_opt)))
		{
			/* Just exit on error or reaching a comment. */
			if(is_null_or_empty(last_opt))
			{
				free(last_opt);
				/* Need to place original input to avoid its disappearance (if we
				 * complete with "", it will replace part of the string). */
				vle_compl_add_match(*start, "");
			}
			else
			{
				vle_compl_put_match(last_opt, "");
			}
			return;
		}
	}

	/* We must have hit an error above. */
	if(last_opt == NULL)
	{
		return;
	}

	/* If last option doesn't span to the end of the string, then we should
	 * complete new option at the end of the line. */
	if(strlen(last_opt) != (size_t)(args - *start))
	{
		*start = args;
		last_opt[0] = '\0';
	}

	bool_only = 0;

	p = skip_alphas(last_opt);
	is_value_completion =  (p[0] == '=' || p[0] == ':');
	is_value_completion |= (p[0] == '-' && p[1] == '=');
	is_value_completion |= (p[0] == '+' && p[1] == '=');
	is_value_completion |= (p[0] == '^' && p[1] == '=');
	if(is_value_completion)
	{
		char t = *p;
		*p = '\0';
		opt = get_option(last_opt, scope);
		*p++ = t;
		if(t != '=' && t != ':')
			p++;
		if(opt != NULL && (opt->type == OPT_SET ||
					(opt->type == OPT_STRLIST && opt->val_count > 0)))
		{
			char *t = strrchr(last_opt, ',');
			if(t != NULL)
				p = t + 1;
		}
		*start += p - last_opt;
	}
	else if(strncmp(last_opt, "no", 2) == 0)
	{
		*start += 2;
		memmove(last_opt, last_opt + 2, strlen(last_opt) - 2 + 1);
		bool_only = 1;
	}
	else if(strncmp(last_opt, "inv", 3) == 0)
	{
		*start += 3;
		memmove(last_opt, last_opt + 3, strlen(last_opt) - 3 + 1);
		bool_only = 1;
	}

	if(!is_value_completion)
	{
		complete_option_name(last_opt, bool_only, !bool_only, scope);
	}
	else if(opt != NULL)
	{
		if(opt->val_count > 0)
		{
			*start += complete_option_value(opt, p);
		}
		else if(*p == '\0' && opt->type != OPT_BOOL)
		{
			vle_compl_put_match(escape_chars(vle_opt_to_string(opt), " |"), "");
		}
	}

	vle_compl_finish_group();
	if(opt != NULL && opt->type == OPT_CHARSET)
	{
		vle_compl_add_last_match("");
	}
	else
	{
		vle_compl_add_last_match(is_value_completion ? p : last_opt);
	}

	free(last_opt);
}

/* Extracts next option from option list.  On error either returns NULL or sets
 * *argsp to NULL, which allows different handling.  On success returns option
 * string.  On reaching trailing comment, empty string is returned.  *argsp is
 * advanced according to parsing. */
static char *
extract_option(const char **argsp, int replace)
{
	int quote = 0;
	int slash = 0;

	const char *args = *argsp;
	char *opt = NULL;
	size_t opt_len = 0U;

	if(*args == '"' && strchr(args + 1, '"') == NULL)
	{
		/* This is a comment. */
		*argsp = args + strlen(args);
		return strdup("");
	}

	while(*args != '\0')
	{
		if(slash == 1)
		{
			if(strappendch(&opt, &opt_len, *args++) != 0)
			{
				goto error;
			}

			slash = 0;
		}
		else if(*args == '\\')
		{
			slash = 1;
			if(replace && (quote == 0 || quote == 2))
			{
				++args;
			}
			else if(strappendch(&opt, &opt_len, *args++) != 0)
			{
				goto error;
			}
		}
		else if(*args == ' ' && quote == 0)
		{
			const char *p = args;
			args = skip_whitespace(args);
			if(char_is_one_of(ENDING_CHARS, *args))
			{
				if(strappendch(&opt, &opt_len, *args++) != 0)
				{
					goto error;
				}
				if(*args != '\0' && !isspace(*args))
				{
					/* Eat the last character which will be restored on the next step. */
					--opt_len;
					/* Append the rest of the string and signal about an error. */
					if(strappend(&opt, &opt_len, p) != 0)
					{
						goto error;
					}
					*argsp = NULL;
					return opt;
				}
				args = skip_whitespace(args);
			}
			else if(char_is_one_of(MIDDLE_CHARS, *args))
			{
				while(*args != '\0' && !isspace(*args))
				{
					if(strappendch(&opt, &opt_len, *args++) != 0)
					{
						goto error;
					}
				}
			}
			break;
		}
		else if(*args == '\'' && quote == 0)
		{
			quote = 1;
			args++;
		}
		else if(*args == '\'' && quote == 1)
		{
			quote = 0;
			args++;
		}
		else if(*args == '"' && quote == 0)
		{
			quote = 2;
			args++;
		}
		else if(*args == '"' && quote == 2)
		{
			quote = 0;
			args++;
		}
		else if(strappendch(&opt, &opt_len, *args++) != 0)
		{
			goto error;
		}
	}

	if(quote != 0)
	{
		/* Probably an unmatched quote. */
		goto error;
	}

	*argsp = args;
	return opt;

error:
	free(opt);
	*argsp = NULL;
	return NULL;
}

/* Returns pointer to the first non-alpha character in the string. */
static char *
skip_alphas(const char str[])
{
	while(isalpha(*str))
		str++;
	return (char *)str;
}

void
vle_opts_complete_real(const char beginning[], OPT_SCOPE scope)
{
	complete_option_name(beginning, 0, 0, scope);
	vle_compl_finish_group();
	vle_compl_add_last_match(beginning);
}

/* Completes name of an option.  The pseudo parameter controls whether pseudo
 * options should be enumerated (e.g. "all"). */
static void
complete_option_name(const char buf[], int bool_only, int pseudo,
		OPT_SCOPE scope)
{
	const size_t len = strlen(buf);
	size_t i;

	assert(scope != OPT_ANY && "Won't complete for this scope.");
	if(scope == OPT_ANY)
	{
		return;
	}

	if(pseudo && strncmp(buf, "all", len) == 0)
	{
		vle_compl_add_match("all", "pseudo-option that groups others");
	}

	for(i = 0U; i < option_count; ++i)
	{
		opt_t *const opt = &options[i];

		if((bool_only && opt->type != OPT_BOOL) || !option_matches(opt, scope))
		{
			continue;
		}

		if(strncmp(buf, opt->name, len) == 0)
		{
			vle_compl_add_match(get_opt_full_name(opt), opt->descr);
		}
	}
}

/* Checks whether option matches the scope.  Returns non-zero if so, otherwise
 * zero is returned. */
static int
option_matches(const opt_t *opt, OPT_SCOPE scope)
{
	return opt->scope == scope || scope == OPT_ANY;
}

/* Gets full name of the option.  Returns pointer to the name. */
static const char *
get_opt_full_name(opt_t *opt)
{
	return (opt->full == NULL) ? opt->name : opt->full;
}

/* Completes value of the given option.  Returns offset for the beginning. */
static int
complete_option_value(const opt_t *opt, const char beginning[])
{
	if(opt->type == OPT_CHARSET)
	{
		return complete_char_value(opt, beginning);
	}
	else
	{
		return complete_list_value(opt, beginning);
	}
}

/* Completes value of the given option with string type of values.  Returns
 * offset for the beginning. */
static int
complete_list_value(const opt_t *opt, const char beginning[])
{
	size_t len;
	int i;

	len = strlen(beginning);

	for(i = 0; i < opt->val_count; ++i)
	{
		if(strncmp(beginning, opt->vals[i][0], len) == 0)
		{
			vle_compl_add_match(opt->vals[i][0], opt->vals[i][1]);
		}
	}

	return 0;
}

/* Completes value of the given option with char type of values.  Returns offset
 * for the beginning. */
static int
complete_char_value(const opt_t *opt, const char beginning[])
{
	int i;
	for(i = 1; i < opt->val_count; ++i)
	{
		if(strchr(beginning, *opt->vals[i][0]) == NULL)
		{
			vle_compl_add_match(opt->vals[i][0], opt->vals[i][1]);
		}
	}

	return strlen(beginning);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
