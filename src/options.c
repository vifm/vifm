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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"

#define OPTION_NAME_MAX 64

/* TODO possibly add validators */

struct opt_t {
	char *name;
	enum opt_type type;
	union optval_t val;
	opt_handler handler;
	/* for OPT_ENUM and OPT_SET */
	int val_count;
	const char **vals;
};

static const char * extract_option(const char *cmd, char *buf);
static void process_option(const char *cmd);
static const char * skip_alphas(const char *cmd);
static int get_option(const char *option);
static int find_option(const char *option);
static int set_on(struct opt_t *opt);
static int set_off(struct opt_t *opt);
static int set_inv(struct opt_t *opt);
static int set_set(struct opt_t *opt, const char *value);
static int set_add(struct opt_t *opt, const char *value);
static int set_remove(struct opt_t *opt, const char *value);
static int set_op(struct opt_t *opt, const char *value, int add);
static char * str_add(char *old, const char *value);
static char * str_remove(char *old, const char *value);
static int find_val(const struct opt_t *opt, const char *value);
static int set_print(struct opt_t *opt);
static void print_msg(const char *msg, const char *description);

static opt_print print_func;

static struct opt_t *options;
static size_t options_count;

void
set_print_handler(opt_print handler)
{
	print_func = handler;
}

void
add_option(const char *name, enum opt_type type, int val_count,
		const char **vals, opt_handler handler)
{
	struct opt_t *p;

	p = realloc(options, sizeof(*options)*(options_count + 1));
	if(p == NULL)
		return;
	options = p;
	options_count++;

	p = options + options_count - 1;
	p->name = strdup(name);
	p->type = type;
	p->handler = handler;
	p->val_count = val_count;
	p->vals = vals;
	p->val.str_val = NULL;
}

void
set_option(const char *name, union optval_t val)
{
	int id = find_option(name);
	if(id == -1)
		return;

	if(options[id].type == OPT_STR || options[id].type == OPT_STRLIST)
	{
		free(options[id].val.str_val);
		options[id].val.str_val = strdup(val.str_val);
	}
	else
	{
		options[id].val = val;
	}
}

void
set_options(const char *cmd)
{
	while(*cmd != '\0')
	{
		char buf[1024];

		cmd = extract_option(cmd, buf);
		process_option(buf);

		if(*cmd == '\0')
			break;
	}
}

static const char *
extract_option(const char *cmd, char *buf)
{
	int slash = 0;
	while(*cmd != '\0')
	{
		if(slash == 1)
		{
			*buf++ = *cmd++;
			slash = 0;
		}
		else if(*cmd == '\\')
		{
			slash = 1;
			cmd++;
		}
		else if(*cmd == ' ')
		{
			while(isspace(*cmd))
				++cmd;
			break;
		}
		else
		{
			*buf++ = *cmd++;
		}
	}
	*buf = '\0';
	return cmd;
}

static void
process_option(const char *cmd)
{
	char option[OPTION_NAME_MAX + 1];
	int id;
	int err;
	const char *p;

	p = skip_alphas(cmd);

	snprintf(option, p - cmd + 1, "%s", cmd);
	id = get_option(option);
	if(id == -1)
	{
		print_msg("Unknown option", cmd);
		return;
	}

	err = 0;
	if(*p == '\0')
	{
		if(find_option(option) != -1)
			err = set_on(&options[id]);
		else if(strncmp(option, "no", 2) == 0)
			err = set_off(&options[id]);
		else if(strncmp(option, "inv", 3) == 0)
			err = set_inv(&options[id]);
	}
	else if(*p == '!' || *p == '?')
	{
		if(*(p + 1) != '\0')
		{
			print_msg("Trailing characters", cmd);
			return;
		}
		if(*p == '!')
			err = set_inv(&options[id]);
		else
			err = set_print(&options[id]);
	}
	else if(strncmp(p, "+=", 2) == 0)
	{
		err = set_add(&options[id], p + 2);
	}
	else if(strncmp(p, "-=", 2) == 0)
	{
		err = set_remove(&options[id], p + 2);
	}
	else if(*p == '=')
	{
		err = set_set(&options[id], p + 1);
	}
	else
	{
		print_msg("Trailing characters", cmd);
	}

	if(err)
		print_msg("Invalid argument", cmd);
}

static const char *
skip_alphas(const char *cmd)
{
	while(isalpha(*cmd))
		cmd++;
	return cmd;
}

static int
get_option(const char *option)
{
	int id;

	id = find_option(option);
	if(id != -1)
		return id;

	if(strncmp(option, "no", 2) == 0)
		return find_option(option + 2);
	else if(strncmp(option, "inv", 3) == 0)
		return find_option(option + 3);
	else
		return -1;
}

static int
find_option(const char *option)
{
	int l = 0, u = options_count - 1;
	while (l <= u)
	{
		int i = (l + u)/2;
		int comp = strcmp(option, options[i].name);
		if(comp == 0)
			return i;
		else if(comp < 0)
			u = i - 1;
		else
			l = i + 1;
	}
	return -1;
}

static int
set_on(struct opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(!opt->val.bool_val)
	{
		opt->val.bool_val = 1;
		opt->handler(OP_ON, opt->val);
	}

	return 0;
}

static int
set_off(struct opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(opt->val.bool_val)
	{
		opt->val.bool_val = 0;
		opt->handler(OP_OFF, opt->val);
	}

	return 0;
}

static int
set_inv(struct opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	opt->val.bool_val = !opt->val.bool_val;
	opt->handler(opt->val.bool_val ? OP_ON : OP_OFF, opt->val);

	return 0;
}

static int
set_set(struct opt_t *opt, const char *value)
{
	if(opt->type == OPT_BOOL)
		return -1;

	if(opt->type == OPT_ENUM || opt->type == OPT_SET)
	{
		int i = find_val(opt, value);
		if(i == -1)
			return -1;

		if(opt->val.enum_item != i)
		{
			opt->val.enum_item = i;
			opt->handler(OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		if(opt->val.str_val == NULL || strcmp(opt->val.str_val, value) != 0)
		{
			free(opt->val.str_val);
			opt->val.str_val = strdup(value);
			opt->handler(OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_INT)
	{
		char *end;
		int int_val = strtoll(value, &end, 10);
		if(opt->val.int_val != int_val)
		{
			opt->val.int_val = int_val;
			opt->handler(OP_SET, opt->val);
		}
	}

	return 0;
}

static int
set_add(struct opt_t *opt, const char *value)
{
	if(opt->type != OPT_SET && opt->type != OPT_STRLIST)
		return -1;

	if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, 1))
			opt->handler(OP_MODIFIED, opt->val);
	}
	else if(*value != '\0')
	{
		opt->val.str_val = str_add(opt->val.str_val, value);
		opt->handler(OP_MODIFIED, opt->val);
	}

	return 0;
}

static int
set_remove(struct opt_t *opt, const char *value)
{
	if(opt->type != OPT_SET && opt->type != OPT_STRLIST)
		return -1;

	if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, 0))
			opt->handler(OP_MODIFIED, opt->val);
	}
	else if(*value != '\0')
	{
		size_t len = 0;
		if(opt->val.str_val != NULL)
			len = strlen(opt->val.str_val);

		opt->val.str_val = str_remove(opt->val.str_val, value);

		if(opt->val.str_val != NULL && len != strlen(opt->val.str_val))
			opt->handler(OP_MODIFIED, opt->val);
	}

	return 0;
}

/* returns not zero if set was modified */
static int
set_op(struct opt_t *opt, const char *value, int add)
{
	char val[1024];
	const char *p;
	int saved_val = opt->val.set_items;

	while(*value != '\0')
	{
		int i;
		p = strchr(value, ',');
		if(p == NULL)
			p = value + strlen(value);

		snprintf(val, p - value + 1, "%s", value);

		i = find_val(opt, val);
		if(i != -1)
		{
			if(add)
				opt->val.set_items |= 2 << i;
			else
				opt->val.set_items &= ~(2 << i);
		}

		if(*p == '\0')
			break;

		value = p + 1;
		while(*value == ',')
			++value;
	}

	return opt->val.set_items != saved_val;
}

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
		strcat(strcpy(new, old), ",");
	strcat(new, value);
	return new;
}

static char *
str_remove(char *old, const char *value)
{
	char *s = old;
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
		}

		if(*p == '\0')
			break;

		old = p + 1;
	}
	return s;
}

static int
find_val(const struct opt_t *opt, const char *value)
{
	int i;
	for(i = 0; i < opt->val_count; i++)
		if(strcmp(opt->vals[i], value) == 0)
			break;
	if(i == opt->val_count)
		return -1;
	return i;
}

static int
set_print(struct opt_t *opt)
{
	char buf[1024];
	if(opt->type == OPT_BOOL)
	{
		snprintf(buf, sizeof(buf), "%s%s",
				opt->val.bool_val ? "" : "no", opt->name);
	}
	else if(opt->type == OPT_INT)
	{
		snprintf(buf, sizeof(buf), "%s=%d", opt->name, opt->val.int_val);
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		snprintf(buf, sizeof(buf), "%s=%s", opt->name,
				opt->val.str_val ? opt->val.str_val : "");
	}
	else if(opt->type == OPT_ENUM)
	{
		snprintf(buf, sizeof(buf), "%s=%s", opt->name, opt->vals[opt->val.enum_item]);
	}
	else if(opt->type == OPT_SET)
	{
		int i, first = 1;
		snprintf(buf, sizeof(buf), "%s=", opt->name);
		for(i = 0; i < opt->val_count; i++)
			if(opt->val.set_items & (2 << i))
			{
				char *p = buf + strlen(buf);
				snprintf(p, sizeof(buf) - (p - buf), "%s%s",
						first ? "" : ",", opt->vals[i]);
				first = 0;
			}
	}
	print_msg("", buf);
	return 0;
}

static void
print_msg(const char *msg, const char *description)
{
	if(print_func != NULL)
		print_func(msg, description);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
