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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "completion.h"
#include "utils.h"

#include "options.h"

#define OPTION_NAME_MAX 64

/* TODO possibly add validators */
/* TODO possibly add default handlers (just set new value) and types
 * OPT_*_PTR */

typedef struct
{
	char *name;
	OPT_TYPE type;
	optval_t val;
	optval_t def;
	opt_handler handler;
	/* for OPT_ENUM and OPT_SET */
	int val_count;
	const char **vals;

	const char *full;
}opt_t;

static opt_t * add_option_inner(const char *name, OPT_TYPE type, int val_count,
		const char **vals, opt_handler handler);
static void print_changed_options(void);
static const char * extract_option(const char *cmd, char *buf, int replace);
static int process_option(const char *cmd);
static const char * skip_alphas(const char *cmd);
static void print_options(void);
static opt_t * get_option(const char *option);
static opt_t * find_option(const char *option);
static int set_on(opt_t *opt);
static int set_off(opt_t *opt);
static int set_inv(opt_t *opt);
static int set_set(opt_t *opt, const char *value);
static int set_reset(opt_t *opt);
static int set_add(opt_t *opt, const char *value);
static int set_remove(opt_t *opt, const char *value);
static int set_op(opt_t *opt, const char *value, int add);
static char * str_add(char *old, const char *value);
static char * str_remove(char *old, const char *value);
static int find_val(const opt_t *opt, const char *value);
static int set_print(opt_t *opt);
static void print_msg(const char *msg, const char *description);
static void complete_option(const char *buf, int bool_only);
static void complete_value(const char *buf, opt_t *opt);

static opt_print print_func;
static int *opts_changed;

static opt_t *options;
static size_t options_count;

void
init_options(int *opts_changed_flag, opt_print handler)
{
	assert(opts_changed_flag != NULL);

	opts_changed = opts_changed_flag;
	print_func = handler;
}

void
reset_options_to_default(void)
{
	int i;
	for(i = 0; i < options_count; i++)
	{
		if(options[i].full != NULL)
			continue;
		set_reset(&options[i]);
	}
}

void
clear_options(void)
{
	int i;

	for(i = 0; i < options_count; i++)
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
add_option(const char *name, const char *abbr, OPT_TYPE type, int val_count,
		const char **vals, opt_handler handler, optval_t def)
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
	if(type == OPT_STR || type == OPT_STRLIST)
	{
		full->def.str_val = strdup(def.str_val);
		full->val.str_val = strdup(def.str_val);
	}
	else
	{
		full->def.int_val = def.int_val;
		full->val.int_val = def.int_val;
	}
}

static opt_t *
add_option_inner(const char *name, OPT_TYPE type, int val_count,
		const char **vals, opt_handler handler)
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
	p->val.str_val = NULL;
	p->full = NULL;
	return p;
}

void
set_option(const char *name, optval_t val)
{
	opt_t *opt = find_option(name);
	if(opt == NULL)
		return;

	if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		free(opt->val.str_val);
		opt->val.str_val = strdup(val.str_val);
	}
	else
	{
		opt->val = val;
	}
}

int
set_options(const char *cmd)
{
	int err = 0;

	if(*cmd == '\0')
	{
		print_changed_options();
		return 0;
	}

	while(*cmd != '\0')
	{
		char buf[1024];

		cmd = extract_option(cmd, buf, 1);
		if(cmd == NULL)
			return -1;
		err += process_option(buf);
	}
	return err;
}

static void
print_changed_options(void)
{
	int i;
	for(i = 0; i < options_count; i++)
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

static const char *
extract_option(const char *cmd, char *buf, int replace)
{
	int quote = 0;
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
			if(replace && (quote == 0 || quote == 2))
				cmd++;
			else
				*buf++ = *cmd++;
		}
		else if(*cmd == ' ' && quote == 0)
		{
			const char *p = cmd;
			cmd = skip_whitespace(cmd);
			if(*cmd == '?' || *cmd == '!' || *cmd == '&')
			{
				*buf++ = *cmd++;
				if(*cmd != '\0' && !isspace(*cmd))
				{
					buf -= cmd - p - 1;
					while(*p != '\0')
						*buf++ = *p++;
					*buf = '\0';
					return NULL;
				}
				cmd = skip_whitespace(cmd);
			}
			break;
		}
		else if(*cmd == '\'' && quote == 0)
		{
			quote = 1;
			cmd++;
		}
		else if(*cmd == '\'' && quote == 1)
		{
			quote = 0;
			cmd++;
		}
		else if(*cmd == '"' && quote == 0)
		{
			quote = 2;
			cmd++;
		}
		else if(*cmd == '"' && quote == 2)
		{
			quote = 0;
			cmd++;
		}
		else
		{
			*buf++ = *cmd++;
		}
	}
	*buf = '\0';
	return cmd;
}

static int
process_option(const char *cmd)
{
	char option[OPTION_NAME_MAX + 1];
	int err;
	const char *p;
	opt_t *opt;

	p = skip_alphas(cmd);

	snprintf(option, p - cmd + 1, "%s", cmd);

	if(strcmp(option, "all") == 0)
	{
		print_options();
		return 0;
	}

	opt = get_option(option);
	if(opt == NULL)
	{
		print_msg("Unknown option", cmd);
		return 1;
	}

	err = 0;
	if(*p == '\0')
	{
		opt_t *o = find_option(option);
		if(o != NULL)
		{
			if(o->type == OPT_BOOL)
				err = set_on(opt);
			else
				err = set_print(o);
		}
		else if(strncmp(option, "no", 2) == 0)
		{
			err = set_off(opt);
		}
		else if(strncmp(option, "inv", 3) == 0)
		{
			err = set_inv(opt);
		}
	}
	else if(*p == '!' || *p == '?' || *p == '&')
	{
		if(*(p + 1) != '\0')
		{
			print_msg("Trailing characters", cmd);
			return 1;
		}
		if(*p == '!')
			err = set_inv(opt);
		else if(*p == '?')
			err = set_print(opt);
		else
			err = set_reset(opt);
	}
	else if(strncmp(p, "+=", 2) == 0)
	{
		err = set_add(opt, p + 2);
	}
	else if(strncmp(p, "-=", 2) == 0)
	{
		err = set_remove(opt, p + 2);
	}
	else if(*p == '=' || *p == ':')
	{
		err = set_set(opt, p + 1);
	}
	else
	{
		print_msg("Trailing characters", cmd);
	}

	if(err)
		print_msg("Invalid argument", cmd);
	return err;
}

static const char *
skip_alphas(const char *cmd)
{
	while(isalpha(*cmd))
		cmd++;
	return cmd;
}

static void
print_options(void)
{
	int i;
	for(i = 0; i < options_count; i++)
	{
		if(options[i].full != NULL)
			continue;
		set_print(&options[i]);
	}
}

static opt_t *
get_option(const char *option)
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

static opt_t *
find_option(const char *option)
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

static int
set_on(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(!opt->val.bool_val)
	{
		opt->val.bool_val = 1;
		*opts_changed = 1;
		opt->handler(OP_ON, opt->val);
	}

	return 0;
}

static int
set_off(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	if(opt->val.bool_val)
	{
		opt->val.bool_val = 0;
		*opts_changed = 1;
		opt->handler(OP_OFF, opt->val);
	}

	return 0;
}

static int
set_inv(opt_t *opt)
{
	if(opt->type != OPT_BOOL)
		return -1;

	opt->val.bool_val = !opt->val.bool_val;
	*opts_changed = 1;
	opt->handler(opt->val.bool_val ? OP_ON : OP_OFF, opt->val);

	return 0;
}

static int
set_set(opt_t *opt, const char *value)
{
	if(opt->type == OPT_BOOL)
		return -1;

	if(opt->type == OPT_SET)
	{
		opt->val.set_items = 0;
		while(*value != '\0')
		{
			const char *p;
			char buf[64];

			if((p = strchr(value, ',')) == 0)
				p = value + strlen(value);

			snprintf(buf, p - value + 1, "%s", value);
			set_add(opt, buf);
			
			value = (*p == '\0') ? p : p + 1;
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
			*opts_changed = 1;
			opt->handler(OP_SET, opt->val);
		}
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		if(opt->val.str_val == NULL || strcmp(opt->val.str_val, value) != 0)
		{
			free(opt->val.str_val);
			opt->val.str_val = strdup(value);
			*opts_changed = 1;
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
			*opts_changed = 1;
			opt->handler(OP_SET, opt->val);
		}
	}

	return 0;
}

static int
set_reset(opt_t *opt)
{
	if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		char *p;

		if(strcmp(opt->val.str_val, opt->def.str_val) == 0)
			return 0;

		p = strdup(opt->def.str_val);
		if(p == NULL)
			return -1;
		free(opt->val.str_val);
		opt->val.str_val = p;
		opt->handler(OP_RESET, opt->val);
	}
	else if(opt->val.int_val != opt->def.int_val)
	{
		opt->val.int_val = opt->def.int_val;
		*opts_changed = 1;
		opt->handler(OP_RESET, opt->val);
	}
	return 0;
}

static int
set_add(opt_t *opt, const char *value)
{
	if(opt->type != OPT_INT && opt->type != OPT_SET && opt->type != OPT_STRLIST)
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

		*opts_changed = 1;
		opt->val.int_val += i;
		opt->handler(OP_MODIFIED, opt->val);
	}
	else if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, 1))
		{
			*opts_changed = 1;
			opt->handler(OP_MODIFIED, opt->val);
		}
	}
	else if(*value != '\0')
	{
		opt->val.str_val = str_add(opt->val.str_val, value);
		*opts_changed = 1;
		opt->handler(OP_MODIFIED, opt->val);
	}

	return 0;
}

static int
set_remove(opt_t *opt, const char *value)
{
	if(opt->type != OPT_INT && opt->type != OPT_SET && opt->type != OPT_STRLIST)
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

		*opts_changed = 1;
		opt->val.int_val -= i;
		opt->handler(OP_MODIFIED, opt->val);
	}
	else if(opt->type == OPT_SET)
	{
		if(set_op(opt, value, 0))
		{
			*opts_changed = 1;
			opt->handler(OP_MODIFIED, opt->val);
		}
	}
	else if(*value != '\0')
	{
		size_t len = 0;
		if(opt->val.str_val != NULL)
			len = strlen(opt->val.str_val);

		opt->val.str_val = str_remove(opt->val.str_val, value);

		if(opt->val.str_val != NULL && len != strlen(opt->val.str_val))
		{
			*opts_changed = 1;
			opt->handler(OP_MODIFIED, opt->val);
		}
	}

	return 0;
}

/* returns not zero if set was modified */
static int
set_op(opt_t *opt, const char *value, int add)
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
				opt->val.set_items |= 1 << i;
			else
				opt->val.set_items &= ~(1 << i);
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
find_val(const opt_t *opt, const char *value)
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
set_print(opt_t *opt)
{
	char buf[1024];
	if(opt->type == OPT_BOOL)
	{
		snprintf(buf, sizeof(buf), "%s%s",
				opt->val.bool_val ? "  " : "no", opt->name);
	}
	else if(opt->type == OPT_INT)
	{
		snprintf(buf, sizeof(buf), "  %s=%d", opt->name, opt->val.int_val);
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST)
	{
		snprintf(buf, sizeof(buf), "  %s=%s", opt->name,
				opt->val.str_val ? opt->val.str_val : "");
	}
	else if(opt->type == OPT_ENUM)
	{
		snprintf(buf, sizeof(buf), "  %s=%s", opt->name,
				opt->vals[opt->val.enum_item]);
	}
	else if(opt->type == OPT_SET)
	{
		int i, first = 1;
		snprintf(buf, sizeof(buf), "  %s=", opt->name);
		for(i = 0; i < opt->val_count; i++)
			if(opt->val.set_items & (1 << i))
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

void
complete_options(const char *cmd, const char **start)
{
	char buf[1024];
	int bool_only;
	int value;
	opt_t *opt = NULL;
	char * p;

	*start = cmd;
	buf[0] = '\0';
	while(*cmd != '\0')
	{
		*start = cmd;
		cmd = extract_option(cmd, buf, 0);
		if(cmd == NULL)
		{
			add_completion(buf);
			return;
		}
	}
	if(strlen(buf) != cmd - *start)
	{
		*start = cmd;
		buf[0] = '\0';
	}

	p = (char *)skip_alphas(buf);
	value = (p[0] == '=' || p[0] == ':');
	value = value || (p[0] == '-' && p[1] == '=');
	value = value || (p[0] == '+' && p[1] == '=');
	if(value)
	{
		char t = *p;
		*p = '\0';
		opt = get_option(buf);
		*p++ = t;
		if(t != '=' && t != ':')
			p++;
		if(opt->type == OPT_SET || opt->type == OPT_STRLIST)
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
	else
	{
		bool_only = 0;
	}

	if(!value)
		complete_option(buf, bool_only);
	else if(opt != NULL)
		complete_value(p, opt);

	completion_group_end();
	if(!value)
		add_completion(buf);
	else
		add_completion(p);
}

static void
complete_option(const char *buf, int bool_only)
{
	opt_t *opt;
	size_t len;
	int i;

	opt = find_option(buf);
	if(opt != NULL && strcmp(opt->name, buf) != 0)
	{
		add_completion(opt->name);
		return;
	}

	len = strlen(buf);
	if(strncmp(buf, "all", len) == 0)
		add_completion("all");
	for(i = 0; i < options_count; i++)
	{
		if(options[i].full != NULL)
			continue;
		if(bool_only && options[i].type != OPT_BOOL)
			continue;
		if(strncmp(buf, options[i].name, len) == 0)
			add_completion(options[i].name);
	}
}

static void
complete_value(const char *buf, opt_t *opt)
{
	size_t len;
	int i;

	len = strlen(buf);

	for(i = 0; i < opt->val_count; i++)
	{
		if(strncmp(buf, opt->vals[i], len) == 0)
			add_completion(opt->vals[i]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
