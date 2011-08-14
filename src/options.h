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

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

enum opt_type {
	OPT_BOOL,
	OPT_INT,
	OPT_STR,
	OPT_STRLIST,
	OPT_ENUM,
	OPT_SET,
};

enum opt_op {
	OP_ON,
	OP_OFF,
	OP_SET,
	OP_MODIFIED, /* for OPT_INT, OPT_SET and OPT_STR */
};

union optval_t {
	int bool_val;
	int int_val;
	char *str_val;
	int enum_item;
	int set_items;
};

typedef void (*opt_handler)(enum opt_op op, union optval_t val);
typedef void (*opt_print)(const char *msg, const char *description);

/* handler can be NULL */
void init_options(int *opts_changed_flag, opt_print handler);
void reset_options_to_default(void);
void clear_options(void);
void add_option(const char *name, const char *abbr, enum opt_type type,
		int val_count, const char **vals, opt_handler handler, union optval_t def);
void set_option(const char *name, union optval_t val);
/* Returns non-zero on error */
int set_options(const char *cmd);

/* Returns NULL on error */
void complete_options(const char *cmd, const char **start);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
