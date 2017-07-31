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

#ifndef VIFM__ENGINE__PRIVATE__OPTIONS_H__
#define VIFM__ENGINE__PRIVATE__OPTIONS_H__

#include <stddef.h> /* size_t */

#include "../options.h"

/* List of valid first characters in the name of an environment variable. */
extern const char OPT_NAME_FIRST_CHAR[];

/* List of valid non-first characters in the name of an environment variable. */
extern const char OPT_NAME_CHARS[];

/* Internal structure holding information about an option.  Options with short
 * form of name will get two such structures, the one for the short name will
 * have the full member set to the name member of other structure. */
typedef struct
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
}
opt_t;

/* Returns a pointer to a structure describing option of given name or NULL
 * when no such option exists. */
opt_t * find_option(const char option[], OPT_SCOPE scope);

/* Assigns value to an option of all kinds except boolean.  Returns non-zero in
 * case of error. */
int set_set(opt_t *opt, const char value[]);

/* Adds value(s) to the option (+= operator).  Returns zero on success. */
int set_add(opt_t *opt, const char value[]);

/* Removes value(s) from the option (-= operator).  Returns non-zero on
 * success. */
int set_remove(opt_t *opt, const char value[]);

/* Converts option value to string representation.  Returns pointer to a
 * statically allocated buffer. */
const char * get_value(const opt_t *opt);

#endif /* VIFM__ENGINE__PRIVATE__OPTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
