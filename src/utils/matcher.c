/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "matcher.h"

#include <regex.h> /* regex_t regcomp() regexec() regfree() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strlen() strrchr() strspn() */

#include "../globs.h"
#include "str.h"
#include "utils.h"

/* Wrapper for a regular expression, its state and compiled form. */
struct matcher_t
{
	char *expr;    /* User-entered pattern. */
	char *raw;     /* Raw regular expression. */
	int globs;     /* Whether original pattern is globs or regexp. */
	int cflags;    /* Regular expression compilation flags. */
	regex_t regex; /* The expression in compiled form. */
};

static int is_re_expr(const char expr[]);
static int is_globs_expr(const char expr[]);

matcher_t *
matcher_alloc(const char expr[], int cs_by_def, int glob_by_def, char **error)
{
	regex_t regex;
	int err;
	matcher_t *matcher;
	int cflags;

	const int re = (is_re_expr(expr) || (!is_globs_expr(expr) && !glob_by_def));
	char *e = strdup(expr);

	*error = NULL;

	if(re)
	{
		char *const flags = strrchr(e, '/') + 1;
		if(parse_case_flag(flags, &cs_by_def) != 0)
		{
			free(e);
			replace_string(error, "Failed to parse flags.");
			return NULL;
		}

		/* Cut the flags off by replacing slash with null character. */
		flags[-1] = '\0';

		cflags = REG_EXTENDED | (cs_by_def ? 0 : REG_ICASE);
	}
	else
	{
		char *re;

		/* Cut off trailing '}'. */
		e[strlen(e) - 1] = '\0';

		re = globs_to_regex(e + 1);
		free(e);
		e = re;

		cflags = REG_EXTENDED | REG_ICASE;
	}

	err = regcomp(&regex, e, cflags);

	if(err != 0)
	{
		free(e);
		replace_string(error, get_regexp_error(err, &regex));
		regfree(&regex);
		return NULL;
	}

	matcher = malloc(sizeof(*matcher));

	matcher->expr = strdup(expr);
	matcher->raw = e;
	matcher->globs = !re;
	matcher->cflags = cflags;
	matcher->regex = regex;

	return matcher;
}

matcher_t *
matcher_clone(const matcher_t *matcher)
{
	int err;
	matcher_t *const clone = malloc(sizeof(*clone));

	if(clone == NULL)
	{
		return NULL;
	}

	clone->expr = strdup(matcher->expr);
	clone->raw = strdup(matcher->raw);
	clone->globs = matcher->globs;
	clone->cflags = matcher->cflags;

	err = regcomp(&clone->regex, clone->raw, clone->cflags);

	if(err != 0 || clone->expr == NULL || clone->raw == NULL)
	{
		matcher_free(clone);
		return NULL;
	}

	return clone;
}

void
matcher_free(matcher_t *matcher)
{
	if(matcher != NULL)
	{
		free(matcher->expr);
		free(matcher->raw);
		regfree(&matcher->regex);
		free(matcher);
	}
}

int
matcher_matches(matcher_t *matcher, const char name[])
{
	return (regexec(&matcher->regex, name, 0, NULL, 0) == 0);
}

const char *
matcher_get_expr(const matcher_t *matcher)
{
	return matcher->expr;
}

int
matcher_includes(const matcher_t *like, const matcher_t *m)
{
	if(m->globs != like->globs || m->cflags != like->cflags)
	{
		return 0;
	}

	if(m->cflags & REG_ICASE)
	{
		if(strcasestr(like->raw, m->raw) != NULL)
		{
			return 1;
		}
	}
	else
	{
		if(strstr(like->raw, m->raw) != NULL)
		{
			return 1;
		}
	}

	return 0;
}

int
matcher_is_expr(const char str[])
{
	return is_re_expr(str) || is_globs_expr(str);
}

/* Checks whether expr is a regular expression file name pattern.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_re_expr(const char expr[])
{
	const char *e = strrchr(expr, '/');
	return expr[0] == '/'                        /* Starts with slash. */
	    && e != NULL && e != expr                /* Has second slash. */
	    && e - expr > 1                          /* Not empty pattern. */
	    && strspn(e + 1, "iI") == strlen(e + 1); /* Has only correct flags. */
}

/* Checks whether expr is a glob file name pattern.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_globs_expr(const char expr[])
{
	return surrounded_with(expr, '{', '}');
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
