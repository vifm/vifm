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

/* Removes all options. */
void vle_opts_reset(void);

/* High-level interface. */

/* Resets an option to its default value. */
void vle_opts_restore_default(const char name[], OPT_SCOPE scope);

/* Resets all options to their default values. */
void vle_opts_restore_defaults(void);

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

/* Low-level interface. */

/* List of valid first characters in the name of an environment variable. */
extern const char OPT_NAME_FIRST_CHAR[];

/* List of valid non-first characters in the name of an environment variable. */
extern const char OPT_NAME_CHARS[];

/* Internal structure holding information about an option.  Options with short
 * form of name will get two such structures, the one for the short name will
 * have the full member set to the name member of other structure. */
typedef struct opt_t opt_t;
struct opt_t
{
	char *name;             /* Name of an option. */
	const char *descr;      /* Option description. */
	OPT_TYPE type;          /* Option type. */
	OPT_SCOPE scope;        /* Scope: local or global. */
	optval_t val;           /* Current value of an option. */
	optval_t def;           /* Default value of an option. */
	opt_handler handler;    /* A pointer to option handler. */
	int val_count;          /* For OPT_ENUM, OPT_SET and OPT_CHARSET types. */
	const char *(*vals)[2]; /* For OPT_ENUM, OPT_SET and OPT_CHARSET types. */

	/* This is not a pointer because they change on inserting options. */
	const char *full;       /* Points to full name of the option. */
};

/* Returns a pointer to a structure describing option of given name or NULL
 * when no such option exists. */
opt_t * vle_opts_find(const char option[], OPT_SCOPE scope);

/* Turns boolean option on.  Returns non-zero in case of error. */
int vle_opt_on(opt_t *opt);

/* Turns boolean option off.  Returns non-zero in case of error. */
int vle_opt_off(opt_t *opt);

/* Assigns value to an option of all kinds except boolean.  Returns non-zero in
 * case of error. */
int vle_opt_assign(opt_t *opt, const char value[]);

/* Adds value(s) to the option (+= operator).  Returns zero on success. */
int vle_opt_add(opt_t *opt, const char value[]);

/* Removes value(s) from the option (-= operator).  Returns non-zero on
 * success. */
int vle_opt_remove(opt_t *opt, const char value[]);

/* Converts option value to string representation.  Returns pointer to a
 * statically allocated buffer. */
const char * vle_opt_to_string(const opt_t *opt);

#endif /* VIFM__ENGINE__OPTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
