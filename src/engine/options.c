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
#include "private/options.h"

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset() strchr() strcmp() strcpy() strlen() */

#include "../utils/str.h"
#include "../utils/string_array.h"
#include "completion.h"
#include "text_buffer.h"

const size_t OPTION_NAME_MAX = 64;

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
	SO_SET, /* Set value of the option. */
	SO_ADD, /* Add item(s) to a set. */
	SO_REMOVE, /* Remove item(s) from a set. */
}
SetOp;

/* Type of function accepted by the for_each_char_of() function. */
typedef void (*mod_t)(char buffer[], char value);

static opt_t * add_option_inner(const char name[], OPT_TYPE type, int val_count,
		const char *vals[], opt_handler handler);
static void print_changed_options(void);
static int process_option(const char arg[]);
static int handle_all_pseudo(const char arg[], const char suffix[]);
static void print_options(void);
static opt_t * get_option(const char option[]);
static int set_on(opt_t *opt);
static int set_off(opt_t *opt);
static int set_inv(opt_t *opt);
static int set_set(opt_t *opt, const char value[]);
static int set_reset(opt_t *opt);
static int set_add(opt_t *opt, const char value[]);
static int set_remove(opt_t *opt, const char value[]);
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
static const char * extract_option(const char args[], char buf[], int replace);
static char * skip_alphas(const char str[]);
static void complete_option_name(const char buf[], int bool_only, int pseudo);
static const char * get_opt_full_name(opt_t *const opt);
static int complete_option_value(const opt_t *opt, const char beginning[]);
static int complete_list_value(const opt_t *opt, const char beginning[]);
static int complete_char_value(const opt_t *opt, const char beginning[]);

/* Characters that can occur after option values. */
static const char ENDING_CHARS[] = "!?&";
/* Characters that can between option names and values. */
static const char MIDDLE_CHARS[] = "^+-=:";

static int *opts_changed;

static opt_t *options;
static size_t options_count;

void
init_options(int *opts_changed_flag)
{
	assert(opts_changed_flag != NULL);

	opts_changed = opts_changed_flag;
}

void
reset_option_to_default(const char name[])
{
	opt_t *opt = find_option(name);
	if(opt != NULL)
	{
		set_reset(opt);
	}
}

void
reset_options_to_default(void)
{
	size_t i;
	for(i = 0U; i < options_count; ++i)
	{
		if(options[i].full == NULL)
		{
			set_reset(&options[i]);
		}
	}
}

void
clear_options(void)
{
	size_t i;
	for(i = 0U; i < options_count; ++i)
	{
		free(options[i].name);
		if(options[i].type == OPT_STR || options[i].type == OPT_STRLIST)
		{
			if(options[i].full == NULL)
				free(options[i].def.str_val);
			free(options[i].val.str_val);
		}
	}
	free(options);
	options = NULL;
	options_count = 0;
}

void
add_option(const char name[], const char abbr[], OPT_TYPE type, int val_count,
		const char *vals[], opt_handler handler, optval_t def)
{
	opt_t *full;

	assert(name != NULL);
	assert(abbr != NULL);

	full = add_option_inner(name, type, val_count, vals, handler);
	if(full == NULL)
		return;
	if(abbr[0] != '\0')
	{
		char *full_name = full->name;
		opt_t *abbreviated;

		abbreviated = add_option_inner(abbr, type, val_count, vals, handler);
		if(abbreviated != NULL)
			abbreviated->full = full_name;
		full = find_option(full_name);
	}
	if(type == OPT_STR || type == OPT_STRLIST || type == OPT_CHARSET)
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
		case OPT_CHARSET:
			assert((size_t)full->val_count == strlen(*full->vals) &&
					"Number of character set items is incorrect.");
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
add_option_inner(const char name[], OPT_TYPE type, int val_count,
		const char *vals[], opt_handler handler)
{
	opt_t *p;

	p = realloc(options, sizeof(*options)*(options_count + 1));
	if(p == NULL)
		return NULL;
	options = p;
	options_count++;

	p = options + options_count - 1;

	while((p - 1) >= options && strcmp((p - 1)->name, name) > 0)
	{
		*p = *(p - 1);
		--p;
	}

	p->name = strdup(name);
	p->type = type;
	p->handler = handler;
	p->val_count = val_count;
	p->vals = vals;
	memset(&p->val, 0, sizeof(p->val));
	p->full = NULL;
	return p;
}

void
set_option(const char name[], optval_t val)
{
	opt_t *opt = find_option(name);
	assert(opt != NULL && "Wrong option name.");

	if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		(void)replace_string(&opt->val.str_val, val.str_val);
	}
	else
	{
		opt->val = val;
	}
}

int
set_options(const char args[])
{
	int err = 0;

	if(*args == '\0')
	{
		print_changed_options();
		return 0;
	}

	while(*args != '\0')
	{
		char buf[1024];

		args = extract_option(args, buf, 1);
		if(args == NULL)
			return -1;
		err += process_option(buf);
	}
	return err;
}

/* Prints options, which value differs from the default one. */
static void
print_changed_options(void)
{
	size_t i;
	for(i = 0; i < options_count; ++i)
	{
		opt_t *opt = &options[i];

		if(opt->full != NULL)
			continue;

		if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
		{
			if(strcmp(opt->val.str_val, opt->def.str_val) == 0)
				continue;
		}
		else if(opt->val.int_val == opt->def.int_val)
		{
			continue;
		}

		set_print(&options[i]);
	}
}

/* Processes one :set statement.  Returns zero on success, otherwize non-zero is
 * returned. */
static int
process_option(const char arg[])
{
	char optname[OPTION_NAME_MAX + 1];
	int err;
	const char *suffix;
	opt_t *opt;

	suffix = skip_alphas(arg);

	copy_str(optname, suffix - arg + 1, arg);

	if(strcmp(optname, "all") == 0)
	{
		return handle_all_pseudo(arg, suffix);
	}

	opt = get_option(optname);
	if(opt == NULL)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Unknown option", arg);
		return 1;
	}

	err = 0;
	if(*suffix == '\0')
	{
		opt_t *o = find_option(optname);
		if(o != NULL)
		{
			if(o->type == OPT_BOOL)
				err = set_on(opt);
			else
				err = set_print(o);
		}
		else if(strncmp(optname, "no", 2) == 0)
		{
			err = set_off(opt);
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
			return 1;
		}
		if(*suffix == '!')
			err = set_inv(opt);
		else if(*suffix == '?')
			err = set_print(opt);
		else
			err = set_reset(opt);
	}
	else if(strncmp(suffix, "+=", 2) == 0)
	{
		err = set_add(opt, suffix + 2);
	}
	else if(strncmp(suffix, "-=", 2) == 0)
	{
		err = set_remove(opt, suffix + 2);
	}
	else if(strncmp(suffix, "^=", 2) == 0)
	{
		err = set_hat(opt, suffix + 2);
	}
	else if(*suffix == '=' || *suffix == ':')
	{
		err = set_set(opt, suffix + 1);
	}
	else
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Trailing characters", arg);
	}

	if(err)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid argument", arg);
	}
	return err;
}

/* Handles "all" pseudo-option.  Actual action is determined by the suffix.
 * Returns zero on success, otherwize non-zero is returned. */
static int
handle_all_pseudo(const char arg[], const char suffix[])
{
	switch(*suffix)
	{
		case '\0':
			print_options();
			return 0;
		case '&':
			reset_options_to_default();
			return 0;

		default:
			vle_tb_append_linef(vle_err, "%s: %s", "Trailing characters", arg);
			return 1;
	}
}

/* Prints values of all options. */
static void
print_options(void)
{
	size_t i;
	for(i = 0U; i < options_count; ++i)
	{
		if(options[i].full == NULL)
		{
			set_print(&options[i]);
		}
	}
}

/* Finds option by its name.  Understands "no" and "inv" boolean option
 * prefixes.  Returns NULL for unknown option names. */
static opt_t *
get_option(const char option[])
{
	opt_t *opt;

	opt = find_option(option);
	if(opt != NULL)
		return opt;

	if(strncmp(option, "no", 2) == 0)
		return find_option(option + 2);
	else if(strncmp(option, "inv", 3) == 0)
		return find_option(option + 3);
	else
		return NULL;
}

opt_t *
find_option(const char option[])
{
	int l = 0, u = options_count - 1;
	while(l <= u)
	{
		int i = (l + u)/2;
		int comp = strcmp(option, options[i].name);
		if(comp == 0)
		{
			if(options[i].full == NULL)
				return &options[i];
			else
				return find_option(options[i].full);
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

/* Turns boolean option on.  Returns non-zero in case of error. */
static int
set_on(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(!opt->val.bool_val)
	{
		opt->val.bool_val = 1;
		notify_option_update(opt, OP_ON, opt->val);
	}

	return 0;
}

/* Turns boolean option off.  Returns non-zero in case of error. */
static int
set_off(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(opt->val.bool_val)
	{
		opt->val.bool_val = 0;
		notify_option_update(opt, OP_OFF, opt->val);
	}

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

	return 0;
}

/* Assigns value to an option of all kinds except boolean.  Returns non-zero in
 * case of error. */
static int
set_set(opt_t *opt, const char value[])
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
		if(opt->val.int_val != int_val)
		{
			opt->val.int_val = int_val;
			notify_option_update(opt, OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_CHARSET)
	{
		const size_t valid_len = strspn(value, *opt->vals);
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

	return 0;
}

/* Resets an option to its default value (opt&). */
static int
set_reset(opt_t *opt)
{
	if(opt->type == OPT_STR || opt->type == OPT_STRLIST ||
			opt->type == OPT_CHARSET)
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
	return 0;
}

/* Adds value(s) to the option (+= operator).  Returns zero on success. */
static int
set_add(opt_t *opt, const char value[])
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
		const size_t valid_len = strspn(value, *opt->vals);
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
		opt->val.str_val = str_add(opt->val.str_val, value);
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}

	return 0;
}

/* Removes value(s) from the option (-= operator).  Returns non-zero on
 * success. */
static int
set_remove(opt_t *opt, const char value[])
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

	return 0;
}

/* Toggles value(s) from the option (^= operator).  Returns non-zero on
 * success. */
static int
set_hat(opt_t *opt, const char value[])
{
	if(opt->type != OPT_CHARSET)
	{
		return -1;
	}

	if(charset_toggle_all(opt, value))
	{
		notify_option_update(opt, OP_MODIFIED, opt->val);
	}

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
	char val[1024];
	const char *p;
	const int old_val = opt->val.set_items;
	int new_val = (op == SO_SET) ? 0 : old_val;

	while(*value != '\0')
	{
		int i;
		p = strchr(value, ',');
		if(p == NULL)
			p = value + strlen(value);

		copy_str(val, p - value + 1, value);

		i = find_val(opt, val);
		if(i != -1)
		{
			if(op == SO_SET || op == SO_ADD)
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

/* And an element to string list.  Reallocates the memory of old and returns
 * its new address or NULL if there isn't enough memory. */
static char *
str_add(char *old, const char *value)
{
	size_t len;
	char *new;

	len = 0;
	if(old != NULL)
		len = strlen(old);
	new = realloc(old, len + 1 + strlen(value) + 1);
	if(new == NULL)
		return old;

	if(old == NULL)
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

/* Returns index of the value in the list of options value, or -1 if value name
 * is wrong. */
static int
find_val(const opt_t *opt, const char value[])
{
	return string_array_pos((char **)opt->vals, opt->val_count, value);
}

/* Prints value of the given option to text buffer. */
static int
set_print(const opt_t *opt)
{
	char buf[1024];
	if(opt->type == OPT_BOOL)
	{
		snprintf(buf, sizeof(buf), "%s%s",
				opt->val.bool_val ? "  " : "no", opt->name);
	}
	else
	{
		snprintf(buf, sizeof(buf), "  %s=%s", opt->name, get_value(opt));
	}
	vle_tb_append_line(vle_err, buf);
	return 0;
}

const char *
get_option_value(const char name[])
{
	opt_t *opt = find_option(name);
	assert(opt != NULL && "Wrong option name.");
	if(opt == NULL)
	{
		return NULL;
	}

	return get_value(opt);
}

const char *
get_value(const opt_t *opt)
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
		copy_str(buf, sizeof(buf), opt->vals[opt->val.enum_item]);
	}
	else if(opt->type == OPT_SET)
	{
		int i, first = 1;
		buf[0] = '\0';
		for(i = 0; i < opt->val_count; i++)
			if(opt->val.set_items & (1 << i))
			{
				char *p = buf + strlen(buf);
				snprintf(p, sizeof(buf) - (p - buf), "%s%s",
						first ? "" : ",", opt->vals[i]);
				first = 0;
			}
	}
	else
	{
		assert(0 && "Don't know how to convert value of this type to a string");
	}
	return buf;
}

void
complete_options(const char args[], const char **start)
{
	char buf[1024];
	int bool_only;
	int is_value_completion;
	opt_t *opt = NULL;
	char * p;

	/* Skip all options except the last one. */
	*start = args;
	buf[0] = '\0';
	while(*args != '\0')
	{
		*start = args;
		args = extract_option(args, buf, 0);
		if(args == NULL)
		{
			vle_compl_add_match(buf);
			return;
		}
	}
	if(strlen(buf) != (size_t)(args - *start))
	{
		*start = args;
		buf[0] = '\0';
	}

	bool_only = 0;

	p = skip_alphas(buf);
	is_value_completion =  (p[0] == '=' || p[0] == ':');
	is_value_completion |= (p[0] == '-' && p[1] == '=');
	is_value_completion |= (p[0] == '+' && p[1] == '=');
	is_value_completion |= (p[0] == '^' && p[1] == '=');
	if(is_value_completion)
	{
		char t = *p;
		*p = '\0';
		opt = get_option(buf);
		*p++ = t;
		if(t != '=' && t != ':')
			p++;
		if(opt != NULL && (opt->type == OPT_SET || opt->type == OPT_STRLIST))
		{
			char *t = strrchr(buf, ',');
			if(t != NULL)
				p = t + 1;
		}
		*start += p - buf;
	}
	else if(strncmp(buf, "no", 2) == 0)
	{
		*start += 2;
		memmove(buf, buf + 2, strlen(buf) - 2 + 1);
		bool_only = 1;
	}
	else if(strncmp(buf, "inv", 3) == 0)
	{
		*start += 3;
		memmove(buf, buf + 3, strlen(buf) - 3 + 1);
		bool_only = 1;
	}

	if(!is_value_completion)
	{
		complete_option_name(buf, bool_only, !bool_only);
	}
	else if(opt != NULL)
	{
		if(opt->val_count > 0)
		{
			*start += complete_option_value(opt, p);
		}
		else if(*p == '\0' && opt->type != OPT_BOOL)
		{
			vle_compl_put_match(escape_chars(get_value(opt), " |"));
		}
	}

	vle_compl_finish_group();
	if(opt != NULL && opt->type == OPT_CHARSET)
	{
		vle_compl_add_last_match("");
	}
	else
	{
		vle_compl_add_last_match(is_value_completion ? p : buf);
	}
}

/* Extracts next option from option list.  Returns NULL on error and next call
 * start position on success. */
static const char *
extract_option(const char args[], char buf[], int replace)
{
	int quote = 0;
	int slash = 0;

	while(*args != '\0')
	{
		if(slash == 1)
		{
			*buf++ = *args++;
			slash = 0;
		}
		else if(*args == '\\')
		{
			slash = 1;
			if(replace && (quote == 0 || quote == 2))
				args++;
			else
				*buf++ = *args++;
		}
		else if(*args == ' ' && quote == 0)
		{
			const char *p = args;
			args = skip_whitespace(args);
			if(char_is_one_of(ENDING_CHARS, *args))
			{
				*buf++ = *args++;
				if(*args != '\0' && !isspace(*args))
				{
					buf -= args - p - 1;
					strcpy(buf, p);
					return NULL;
				}
				args = skip_whitespace(args);
			}
			else if(char_is_one_of(MIDDLE_CHARS, *args))
			{
					while(*args != '\0' && !isspace(*args))
						*buf++ = *args++;
					*buf = '\0';
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
		else
		{
			*buf++ = *args++;
		}
	}
	*buf = '\0';
	return args;
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
complete_real_option_names(const char beginning[])
{
	complete_option_name(beginning, 0, 0);
}

/* Completes name of an option.  The pseudo parameter controls whether pseudo
 * options should be enumerated (e.g. "all"). */
static void
complete_option_name(const char buf[], int bool_only, int pseudo)
{
	const size_t len = strlen(buf);
	size_t i;

	if(pseudo && strncmp(buf, "all", len) == 0)
	{
		vle_compl_add_match("all");
	}

	for(i = 0U; i < options_count; ++i)
	{
		opt_t *const opt = &options[i];

		if(bool_only && opt->type != OPT_BOOL)
		{
			continue;
		}

		if(strncmp(buf, opt->name, len) == 0)
		{
			vle_compl_add_match(get_opt_full_name(opt));
		}
	}
}

/* Gets full name of the option.  Returns pointer to the name. */
static const char *
get_opt_full_name(opt_t *const opt)
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

	for(i = 0; i < opt->val_count; i++)
	{
		if(strncmp(beginning, opt->vals[i], len) == 0)
			vle_compl_add_match(opt->vals[i]);
	}

	return 0;
}

/* Completes value of the given option with char type of values.  Returns offset
 * for the beginning. */
static int
complete_char_value(const opt_t *opt, const char beginning[])
{
	const char *vals = *opt->vals;
	int i;

	for(i = 0; i < opt->val_count; i++)
	{
		if(strchr(beginning, vals[i]) == NULL)
		{
			const char char_str[] = { vals[i], '\0' };
			vle_compl_add_match(char_str);
		}
	}

	return strlen(beginning);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
