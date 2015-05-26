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

#include "globs.h"
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

static int compile_expr(matcher_t *m, int strip, int cs_by_def, char **error);
static int parse_glob(matcher_t *m, int strip, char **error);
static int parse_re(matcher_t *m, int strip, int cs_by_def, char **error);
static int is_re_expr(const char expr[]);
static int is_globs_expr(const char expr[]);

matcher_t *
matcher_alloc(const char expr[], int cs_by_def, int glob_by_def, char **error)
{
	const int re = is_re_expr(expr), glob = is_globs_expr(expr);
	const int strip = (re || glob);
	matcher_t *matcher, m = {
		.globs = !(re || (!glob && !glob_by_def)),
		.raw = strdup(expr + (strip ? 1 : 0)),
	};

	*error = NULL;

	if(m.raw == NULL)
	{
		replace_string(error, "Failed to allocate memory for match expr copy.");
		return NULL;
	}

	if(compile_expr(&m, strip, cs_by_def, error) != 0)
	{
		free(m.raw);
		return NULL;
	}

	m.expr = strdup(expr);
	if(m.expr == NULL)
	{
		replace_string(error, "Failed to clone match expr.");
		matcher_free(&m);
		return NULL;
	}

	matcher = malloc(sizeof(*matcher));
	if(matcher == NULL)
	{
		replace_string(error, "Failed allocate memory for matcher.");
		matcher_free(&m);
	}
	else
	{
		*matcher = m;
	}

	return matcher;
}

/* Compiles m->raw into regular expression.  Also replaces global with
 * equivalent regexp.  Returns zero on success or non-zero on error with *error
 * containing description of it. */
static int
compile_expr(matcher_t *m, int strip, int cs_by_def, char **error)
{
	int err;

	if(m->globs)
	{
		if(parse_glob(m, strip, error) != 0)
		{
			return 1;
		}
	}
	else
	{
		if(parse_re(m, strip, cs_by_def, error) != 0)
		{
			return 1;
		}
	}

	err = regcomp(&m->regex, m->raw, m->cflags);
	if(err != 0)
	{
		replace_string(error, get_regexp_error(err, &m->regex));
		regfree(&m->regex);
		return 1;
	}

	return 0;
}

/* Replaces global with equivalent regexp and setups flags.  Returns zero on
 * success or non-zero on error with *error containing description of it. */
static int
parse_glob(matcher_t *m, int strip, char **error)
{
	char *re;

	if(strip)
	{
		/* Cut off trailing '}'. */
		m->raw[strlen(m->raw) - 1] = '\0';
	}

	re = globs_to_regex(m->raw);
	if(re == NULL)
	{
		replace_string(error, "Failed convert globs into regexp.");
		return 1;
	}

	free(m->raw);
	m->raw = re;

	m->cflags = REG_EXTENDED | REG_ICASE;
	return 0;
}

/* Parses regexp flags.  Returns zero on success or non-zero on error with
 * *error containing description of it. */
static int
parse_re(matcher_t *m, int strip, int cs_by_def, char **error)
{
	if(strip)
	{
		char *const flags = strrchr(m->raw, '/') + 1;
		if(parse_case_flag(flags, &cs_by_def) != 0)
		{
			replace_string(error, "Failed to parse flags.");
			return 1;
		}

		/* Cut the flags off by replacing slash with null character. */
		flags[-1] = '\0';
	}

	m->cflags = REG_EXTENDED | (cs_by_def ? 0 : REG_ICASE);
	return 0;
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
