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

#ifndef VIFM__ENGINE__OPTIONS_H__
#define VIFM__ENGINE__OPTIONS_H__

/* The module processes :set command arguments and handles all operations on
 * options. */

/* Type of an option. */
typedef enum
{
	OPT_BOOL,    /* A boolean. */
	OPT_INT,     /* An integer. */
	OPT_STR,     /* A string of undefined format. */
	OPT_STRLIST, /* A comma separated list of strings. */
	OPT_ENUM,    /* An enumeration (one value at a time). */
	OPT_SET,     /* A set (multiple or none values at a time). */
	OPT_CHARSET, /* A set of single characters (multiple or none values at a
	                time). */
}
OPT_TYPE;

/* Operation on an option. */
typedef enum
{
	OP_ON,       /* Boolean value was turned on. */
	OP_OFF,      /* Boolean value was turned off. */
	OP_SET,      /* Value set. */
	OP_MODIFIED, /* Value added/removed (for OPT_INT, OPT_SET and OPT_STR). */
	OP_RESET,    /* Value is reset to default. */
}
OPT_OP;

/* Scope of the option or action on options. */
typedef enum
{
	OPT_GLOBAL, /* Global option (either unique or pair of a local one). */
	OPT_LOCAL,  /* Local option. */
	OPT_ANY,    /* Local option if available, global otherwise. */
}
OPT_SCOPE;

typedef union
{
	int bool_val;  /* Value of OPT_BOOL type. */
	int int_val;   /* Value of OPT_INT type. */
	char *str_val; /* Value of OPT_STR or OPT_STRLIST types. */
	int enum_item; /* Value of OPT_ENUM type. */
	int set_items; /* Value of OPT_SET type. */
}
optval_t;

/* Function type for option handler. */
typedef void (*opt_handler)(OPT_OP op, optval_t val);

/* Type of handler that is invoked on every option set, even if option value
 * is not changed.  Can be used to implement custom logic. */
typedef void (*opt_uni_handler)(const char name[], optval_t val,
		OPT_SCOPE scope);

/* Initializes option module.  opts_changed_flag will be set to true after at
 * least one option will change its value using set_options(...) function.
 * universal_handler can be NULL. */
void vle_opts_init(int *opts_changed_flag, opt_uni_handler universal_handler);

/* Resets an option to its default value. */
void vle_opts_restore_default(const char name[], OPT_SCOPE scope);

/* Resets all options to their default values. */
void vle_opts_restore_defaults(void);

/* Removes all options. */
void vle_opts_reset(void);

/* Adds an option.  The scope can't be OPT_ANY here. */
void vle_opts_add(const char name[], const char abbr[], const char descr[],
		OPT_TYPE type, OPT_SCOPE scope, int val_count, const char *vals[][2],
		opt_handler handler, optval_t def);

/* Sets option value without calling its change handler.  Asserts for correct
 * option name. */
void vle_opts_assign(const char name[], optval_t val, OPT_SCOPE scope);

/* Processes :set command arguments.  Returns non-zero on error. */
int vle_opts_set(const char args[], OPT_SCOPE scope);

/* Converts option value to string representation.  Returns pointer to a
 * statically allocated buffer.  Asserts for correct option name or returns NULL
 * on it.  For boolean options empty string is returned. */
const char * vle_opts_get(const char name[], OPT_SCOPE scope);

/* Completes set arguments list. */
void vle_opts_complete(const char args[], const char **start, OPT_SCOPE scope);

/* Completes names of real options (no pseudo options like "all"). */
void vle_opts_complete_real(const char beginning[], OPT_SCOPE scope);

#endif /* VIFM__ENGINE__OPTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
