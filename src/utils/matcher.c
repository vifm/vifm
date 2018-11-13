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
#include <string.h> /* strdup() strlen() strrchr() strspn() */

#include "../int/file_magic.h"
#include "globs.h"
#include "path.h"
#include "regexp.h"
#include "str.h"

/* Type of a matcher. */
typedef enum
{
	MT_REGEX, /* Regular expression. */
	MT_GLOBS, /* List of globs (translated to regular expression). */
	MT_MIME,  /* List of mime types. */
}
MType;

/* Wrapper for a regular expression, its state and compiled form. */
struct matcher_t
{
	MType type;    /* Type of the matcher's pattern. */
	char *expr;    /* User-entered pattern. */
	char *undec;   /* User-entered pattern with decoration stripped. */
	char *raw;     /* Raw stripped value (regular expression). */
	int full_path; /* Matches full path instead of just file name. */
	int cflags;    /* Regular expression compilation flags. */
	int negated;   /* Whether match is inverted. */
	regex_t regex; /* The expression in compiled form, unless matcher is empty. */
};

static int is_full_path(const char expr[], int re, int glob, int *strip);
static MType determine_type(const char expr[], int re, int glob,
		int glob_by_def, int *strip);
static int compile_expr(matcher_t *m, int strip, int cs_by_def,
		const char on_empty_re[], char **error);
static int parse_glob(matcher_t *m, int strip, char **error);
static int parse_re(matcher_t *m, int strip, int cs_by_def,
		const char on_empty_re[], char **error);
static void free_matcher_items(matcher_t *matcher);
static int is_negated(const char **expr);
static int is_re_expr(const char expr[], int allow_empty);
static int is_globs_expr(const char expr[]);
static int is_mime_expr(const char expr[]);

matcher_t *
matcher_alloc(const char expr[], int cs_by_def, int glob_by_def,
		const char on_empty_re[], char **error)
{
	const char *orig_expr = expr;
	const int negated = is_negated(&expr);
	const int re = is_re_expr(expr, 1), glob = is_globs_expr(expr);
	int strip;
	const int full_path = is_full_path(expr, re, glob, &strip);

	matcher_t *matcher, m = {
		.type = determine_type(expr, re, glob, glob_by_def, &strip),
		.raw = strdup(expr + strip),
		.negated = negated,
		.full_path = full_path,
	};

	*error = NULL;

	if(m.raw == NULL)
	{
		replace_string(error, "Failed to allocate memory for match expr copy.");
		return NULL;
	}

	m.expr = strdup(orig_expr);
	if(m.expr == NULL)
	{
		free(m.raw);
		replace_string(error, "Failed to clone match expr.");
		return NULL;
	}

	if(compile_expr(&m, strip, cs_by_def, on_empty_re, error) != 0)
	{
		free(m.raw);
		free(m.expr);
		free(m.undec);
		return NULL;
	}

	matcher = malloc(sizeof(*matcher));
	if(matcher == NULL)
	{
		replace_string(error, "Failed allocate memory for matcher.");
		free_matcher_items(&m);
	}
	else
	{
		*matcher = m;
	}

	return matcher;
}

/* Checks whether this is full path match expression.  Sets *strip to width of
 * markers before and after pattern.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_full_path(const char expr[], int re, int glob, int *strip)
{
	int full_path = 0;

	*strip = 0;

	if(re)
	{
		const char *const s = strrchr(expr, '/') - 1;
		full_path = (expr[1] == '/' && s > expr + 1 && *s == '/');
		*strip = full_path ? 2 : 1;
	}
	else if(glob)
	{
		full_path = (expr[1] == '{' && expr[strlen(expr) - 2] == '}');
		*strip = full_path ? 2 : 1;
	}

	return full_path;
}

/* Determines type of the matcher for given expression.  Returns the type. */
static MType
determine_type(const char expr[], int re, int glob, int glob_by_def, int *strip)
{
	if(re)
	{
		return MT_REGEX;
	}
	if(glob)
	{
		return MT_GLOBS;
	}
	if(is_mime_expr(expr))
	{
		*strip = 1;
		return MT_MIME;
	}
	return glob_by_def ? MT_GLOBS : MT_REGEX;
}

/* Compiles m->raw into regular expression.  Also replaces global with
 * equivalent regexp.  Returns zero on success or non-zero on error with *error
 * containing description of it. */
static int
compile_expr(matcher_t *m, int strip, int cs_by_def, const char on_empty_re[],
		char **error)
{
	int err;

	switch(m->type)
	{
		case MT_MIME:
		case MT_GLOBS:
			if(parse_glob(m, strip, error) != 0)
			{
				return 1;
			}
			break;
		case MT_REGEX:
			if(parse_re(m, strip, cs_by_def, on_empty_re, error) != 0)
			{
				return 1;
			}
			break;
	}

	if(m->raw[0] == '\0')
	{
		/* This is an empty matcher and we don't compile "". */
		return 0;
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

	if(strip != 0)
	{
		/* Cut off trailing "}" or "}}". */
		m->raw[strlen(m->raw) - strip] = '\0';
	}

	m->undec = strdup(m->raw);
	if(m->undec == NULL)
	{
		replace_string(error, "Failed to allocate memory.");
		return 1;
	}

	re = globs_to_regex(m->raw);
	if(re == NULL)
	{
		replace_string(error, "Failed to convert globs into regexp.");
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
parse_re(matcher_t *m, int strip, int cs_by_def, const char on_empty_re[],
		char **error)
{
	if(strip != 0)
	{
		char *flags = strrchr(m->raw, '/');
		flags = (flags == NULL) ? (m->raw + strlen(m->raw)) : (flags + 1);

		if(parse_case_flag(flags, &cs_by_def) != 0)
		{
			replace_string(error, "Failed to parse flags.");
			return 1;
		}

		/* Cut the flags off by replacing trailing slash(es) with null character. */
		flags[-strip] = '\0';
	}

	if(m->raw[0] == '\0')
	{
		const char *const mark = (strip == 2) ? "//" : (strip == 1 ? "/" : "");

		replace_string(&m->raw, on_empty_re);
		put_string(&m->expr, format_str("%s%s%s%s", mark, on_empty_re, mark,
				m->expr + strip*2));
	}

	m->undec = strdup(m->raw);
	if(m->undec == NULL)
	{
		replace_string(error, "Failed to allocate memory.");
		return 1;
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

	*clone = *matcher;
	clone->expr = strdup(matcher->expr);
	clone->raw = strdup(matcher->raw);
	clone->undec = strdup(matcher->undec);

	/* Don't compile regex for empty matcher. */
	err = (clone->raw != NULL && clone->raw[0] == '\0')
	    ? 0
	    : regcomp(&clone->regex, matcher->raw, matcher->cflags);

	if(err != 0 || clone->expr == NULL || clone->raw == NULL ||
			clone->undec == NULL)
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
		free_matcher_items(matcher);
		free(matcher);
	}
}

/* Frees all resources allocated by the matcher, but not the matcher itself.
 * matcher can't be NULL. */
static void
free_matcher_items(matcher_t *matcher)
{
	if(matcher->raw != NULL && !matcher_is_empty(matcher))
	{
		/* Regex is compiled only for non-empty matchers. */
		regfree(&matcher->regex);
	}
	free(matcher->expr);
	free(matcher->raw);
	free(matcher->undec);
}

int
matcher_matches(const matcher_t *matcher, const char path[])
{
	if(matcher_is_empty(matcher))
	{
		/* Empty matcher matches nothing, not even an empty string. */
		return 0;
	}

	if(matcher->type == MT_MIME)
	{
		path = get_mimetype(path, 1);
		if(path == NULL)
		{
			return matcher->negated;
		}
	}
	else if(!matcher->full_path)
	{
		path = get_last_path_component(path);
	}

	return (regexec(&matcher->regex, path, 0, NULL, 0) == 0)^matcher->negated;
}

int
matcher_is_empty(const matcher_t *matcher)
{
	return (matcher->raw[0] == '\0');
}

const char *
matcher_get_expr(const matcher_t *matcher)
{
	return matcher->expr;
}

const char *
matcher_get_undec(const matcher_t *matcher)
{
	return matcher->undec;
}

int
matcher_includes(const matcher_t *matcher, const matcher_t *like)
{
	if(matcher->type != like->type || matcher->cflags != like->cflags ||
			matcher->full_path != like->full_path)
	{
		return 0;
	}

	return (matcher->cflags & REG_ICASE)
	     ? (strcasestr(matcher->raw, like->raw) != NULL)
	     : (strstr(matcher->raw, like->raw) != NULL);
}

/* Checks whether *expr specifies negated pattern.  Adjusts pointer if so.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_negated(const char **expr)
{
	if(**expr == '!' &&
			(is_re_expr(*expr + 1, 1) || is_globs_expr(*expr + 1) ||
			 is_mime_expr(*expr + 1)))
	{
		++*expr;
		return 1;
	}
	return 0;
}

/* Checks whether expr is a regular expression file name pattern.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_re_expr(const char expr[], int allow_empty)
{
	const char *e = strrchr(expr, '/');
	return expr[0] == '/'                        /* Starts with slash. */
	    && e != NULL && e != expr                /* Has second slash. */
	    && (allow_empty || e - expr > 1);        /* Not empty pattern. */
}

/* Checks whether expr is a glob file name pattern.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_globs_expr(const char expr[])
{
	return surrounded_with(expr, '{', '}') && expr[2] != '\0';
}

/* Checks whether expr is a mime type pattern.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_mime_expr(const char expr[])
{
	return surrounded_with(expr, '<', '>') && expr[2] != '\0';
}

int
matcher_is_full_path(const matcher_t *matcher)
{
	return matcher->full_path;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
