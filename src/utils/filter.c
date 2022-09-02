/* vifm
 * Copyright (C) 2013 xaizek.
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

#include "filter.h"

#if(defined(BSD) && (BSD>=199103))
#include <sys/types.h> /* required for regex.h on FreeBSD 4.2 */
#endif

#include <regex.h> /* REG_EXTENDED REG_ICASE regex_t regfree() */

#include <assert.h> /* assert */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strdup() strlen() */

#include "regexp.h"
#include "str.h"

static int append_to_filter(filter_t *filter, const char value[]);
static void reset_regex(filter_t *filter, const char value[]);
static void free_regex(filter_t *filter);
static void compile_regex(filter_t *filter, const char value[]);
static char * escape_name_for_filter(const char string[]);

int
filter_init(filter_t *filter, int case_sensitive)
{
	filter->raw = strdup("");
	if(filter->raw == NULL)
	{
		return 1;
	}

	filter->is_regex_valid = 0;

	filter->cflags = REG_EXTENDED;

	if(!case_sensitive)
	{
		filter->cflags |= REG_ICASE;
	}

	return 0;
}

void
filter_dispose(filter_t *filter)
{
	free(filter->raw);
	filter->raw = NULL;

	free_regex(filter);
}

int
filter_is_empty(const filter_t *filter)
{
	return filter->raw[0] == '\0' && !filter->is_regex_valid;
}

void
filter_clear(filter_t *filter)
{
	filter->raw[0] = '\0';
	free_regex(filter);
}

int
filter_set(filter_t *filter, const char value[])
{
	if(value[0] == '\0')
	{
		filter_clear(filter);
		return 0;
	}
	else if(replace_string(&filter->raw, value) == 0)
	{
		reset_regex(filter, value);
		return (filter->is_regex_valid || filter->raw[0] == '\0') ? 0 : 1;
	}
	else
	{
		return 1;
	}
}

int
filter_assign(filter_t *filter, const filter_t *source)
{
	filter_t tmp;
	if(filter_init(&tmp, !(source->cflags & REG_ICASE)) != 0)
	{
		return 1;
	}

	if(filter_set(&tmp, source->raw) != 0)
	{
		filter_clear(&tmp);
		return 1;
	}

	filter_dispose(filter);
	*filter = tmp;
	return 0;
}

int
filter_change(filter_t *filter, const char value[], int case_sensitive)
{
	if(case_sensitive)
	{
		filter->cflags &= ~REG_ICASE;
	}
	else
	{
		filter->cflags |= REG_ICASE;
	}
	return filter_set(filter, value);
}

int
filter_append(filter_t *filter, const char value[])
{
	if(value[0] == '\0')
	{
		return 1;
	}
	else
	{
		return append_to_filter(filter, value);
	}
}

/* Appends value to filter expression (using logical or and whole pattern
 * matching).  Returns zero on success, otherwise non-zero is returned. */
static int
append_to_filter(filter_t *filter, const char value[])
{
	size_t len = strlen(filter->raw);

	char *const escaped_value = escape_name_for_filter(value);
	if(escaped_value == NULL)
	{
		return 1;
	}

	if(len != 0)
	{
		filter->raw = extend_string(filter->raw, "|", &len);
	}

	filter->raw = extend_string(filter->raw, "^", &len);

	filter->raw = extend_string(filter->raw, escaped_value, &len);
	free(escaped_value);

	filter->raw = extend_string(filter->raw, "$", &len);

	reset_regex(filter, filter->raw);

	return (filter->is_regex_valid || filter->raw[0] == '\0') ? 0 : 1;
}

/* Replaces possibly existing regular expression with the new one. */
static void
reset_regex(filter_t *filter, const char value[])
{
	free_regex(filter);
	compile_regex(filter, value);
}

/* Frees resources allocated by the regular expression, if any. */
static void
free_regex(filter_t *filter)
{
	if(filter->is_regex_valid)
	{
		regfree(&filter->regex);
		filter->is_regex_valid = 0;
	}
}

/* Compiles the regular expression, which is assumed to be either freed or not
 * allocated yet. */
static void
compile_regex(filter_t *filter, const char value[])
{
	int comp_error;
	assert(!filter->is_regex_valid && "Filter should have been freed.");
	comp_error = regexp_compile(&filter->regex, value, filter->cflags);
	filter->is_regex_valid = comp_error == 0;
}

/* Escapes the string for the purpose of using it in filter.  Returns new
 * string, caller should free it. */
static char *
escape_name_for_filter(const char string[])
{
	static const char *NEED_ESCAPING = "\\[](){}+*^$.?|";
	size_t len;
	char *ret, *dup;

	len = strlen(string);

	dup = ret = malloc(len*2 + 2 + 1);
	if(dup == NULL)
	{
		return NULL;
	}

	while(*string != '\0')
	{
		if(char_is_one_of(NEED_ESCAPING, *string))
		{
			*dup++ = '\\';
		}
		*dup++ = *string++;
	}
	*dup = '\0';
	return ret;
}

int
filter_matches(const filter_t *filter, const char pattern[])
{
	if(filter->is_regex_valid)
	{
		return regexec(&filter->regex, pattern, 0, NULL, 0) == 0;
	}
	else
	{
		return -1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
