/* vifm
 * Copyright (C) 2016 xaizek.
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

#include "matchers.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "matcher.h"
#include "str.h"
#include "string_array.h"
#include "test_helpers.h"

/* Supported types of tokens. */
typedef enum
{
	BEGIN,  /* Beginning of a string, before anything is parsed. */
	EMARK,  /* Exclamation mark (!). */
	LT,     /* Less than operator (<). */
	GT,     /* Greater than operator (>). */
	LCB,    /* Left curly brace ({). */
	RCB,    /* Right curly brace (}). */
	DLCB,   /* Double left curly brace ({{). */
	DRCB,   /* Double right curly brace (}}). */
	SLASH,  /* Slash (/). */
	DSLASH, /* Double slash (//). */
	BSLASH, /* Backslash (\). */
	SYM,    /* Any other symbol that doesn't have meaning in current context. */
	END     /* End of a string, after everything is parsed. */
}
TokenType;

/* List of matchers. */
struct matchers_t
{
	struct matcher_t **list; /* The matchers. */
	int count;               /* Number of matchers. */
	char *expr;              /* User-entered pattern list. */
};

/* Parser state. */
typedef struct
{
	const char *input; /* Current input position. */
	TokenType tok;     /* Current token during parsing of patterns. */
	int is_list;       /* Whether it's comma-separated list of pattern lists. */
}
parsing_state_t;

TSTATIC char ** break_into_matchers(const char concat[], int *count,
		int is_list);
static int find_patterns(parsing_state_t *state);
static int find_pattern(parsing_state_t *state);
static int find_regex(parsing_state_t *state, TokenType decor);
static int find_pat(parsing_state_t *state, TokenType left, TokenType right);
static int is_at_bound(const parsing_state_t *state);
static void load_token(parsing_state_t *state, int single_char);
static int get_token_width(TokenType tok);

matchers_t *
matchers_alloc(const char list[], int cs_by_def, int glob_by_def,
		const char on_empty_re[], char **error)
{
	char **exprs;
	int nexprs;
	int i;

	matchers_t *const matchers = malloc(sizeof(*matchers));

	*error = NULL;

	exprs = break_into_matchers(list, &nexprs, 0);
	matchers->count = nexprs;
	matchers->list = reallocarray(NULL, nexprs, sizeof(matcher_t *));
	matchers->expr = strdup(list);
	if(matchers->list == NULL || matchers->expr == NULL)
	{
		matchers->count = 0;
		matchers_free(matchers);
		free_string_array(exprs, nexprs);
		return NULL;
	}

	for(i = 0; i < nexprs; ++i)
	{
		matchers->list[i] = matcher_alloc(exprs[i], cs_by_def, glob_by_def,
				on_empty_re, error);
		if(matchers->list[i] == NULL)
		{
			char *const err = format_str("%s: %s", exprs[i], *error);

			matchers->count = i;
			matchers_free(matchers);
			free_string_array(exprs, nexprs);

			free(*error);
			*error = err;
			return NULL;
		}
	}

	free_string_array(exprs, nexprs);
	return matchers;
}

matchers_t *
matchers_clone(const matchers_t *matchers)
{
	int i;
	matchers_t *const clone = malloc(sizeof(*matchers));

	clone->count = matchers->count;
	clone->list = reallocarray(NULL, matchers->count, sizeof(matcher_t *));
	clone->expr = strdup(matchers->expr);

	if(clone->list == NULL || clone->expr == NULL)
	{
		clone->count = 0;
		matchers_free(clone);
		return NULL;
	}

	for(i = 0; i < matchers->count; ++i)
	{
		clone->list[i] = matcher_clone(matchers->list[i]);
		if(clone->list[i] == NULL)
		{
			clone->count = i;
			matchers_free(clone);
			return NULL;
		}
	}

	return clone;
}

void
matchers_free(matchers_t *matchers)
{
	int i;
	if(matchers == NULL)
	{
		return;
	}

	for(i = 0; i < matchers->count; ++i)
	{
		matcher_free(matchers->list[i]);
	}
	free(matchers->list);
	free(matchers->expr);
	free(matchers);
}

int
matchers_match(const matchers_t *matchers, const char path[])
{
	int i;
	for(i = 0; i < matchers->count; ++i)
	{
		if(!matcher_matches(matchers->list[i], path))
		{
			return 0;
		}
	}
	return 1;
}

int
matchers_match_dir(const matchers_t *matchers, const char path[])
{
	int i;
	char *const dir_path = format_str("%s/", path);
	for(i = 0; i < matchers->count; ++i)
	{
		const matcher_t *const m = matchers->list[i];
		if(matcher_is_full_path(m) && !ends_with(matcher_get_undec(m), "/"))
		{
			break;
		}
		if(!matcher_matches(m, dir_path))
		{
			break;
		}
	}
	free(dir_path);
	return (i >= matchers->count);
}

const char *
matchers_get_expr(const matchers_t *matchers)
{
	return matchers->expr;
}

int
matchers_includes(const matchers_t *matchers, const matchers_t *like)
{
	int i;
	for(i = 0; i < like->count; ++i)
	{
		int j;
		for(j = 0; j < matchers->count; ++j)
		{
			if(matcher_includes(matchers->list[j], like->list[i]))
			{
				break;
			}
		}
		if(j >= matchers->count)
		{
			return 0;
		}
	}
	return 1;
}

int
matchers_is_expr(const char str[])
{
	parsing_state_t state = { .input = str, .tok = BEGIN };
	load_token(&state, 0);
	do
	{
		if(!find_pattern(&state))
		{
			return 0;
		}
	}
	while(state.tok != END);

	return 1;
}

char **
matchers_list(const char concat[], int *count)
{
	return break_into_matchers(concat, count, 1);
}

/* Below is a parser that accepts list of patterns:
 *
 *   pattern1pattern2...patternN
 *
 * where each pattern can be one of following:
 *
 *   1. [!]{comma-separated-name-globs}
 *   2. [!]{{comma-separated-path-globs}}
 *   3. [!]/name-regular-expression/[iI]
 *   4. [!]//path-regular-expression//[iI]
 *   5. [!]<comma-separated-mime-type-globs>
 *   6. undecorated-pattern
 *
 * If something goes wrong during the parsing, whole-line sixth pattern type is
 * assumed (empty input falls into this category automatically).
 *
 * Formal grammar is below, implementation doesn't follow it in all aspects, but
 * general structure is similar.
 *
 * PATTERN_LIST ::= PATTERNS ( ',' PATTERNS )*
 * PATTERNS     ::= PATTERN+ | UNDECORATED
 * PATTERN      ::= NAME_GLOB | PATH_GLOB | NAME_REGEX | PATH_REGEX | MIME
 *
 * NAME_GLOB    ::= "!"? "{"        CHAR+  "}"             BORDER_TOKEN
 * PATH_GLOB    ::= "!"? "{{"       CHAR+ "}}"             BORDER_TOKEN
 * NAME_REGEX   ::= "!"? "/"  REGEX_CHAR+  "/" REGEX_FLAGS BORDER_TOKEN
 * PATH_REGEX   ::= "!"? "//" REGEX_CHAR+ "//" REGEX_FLAGS BORDER_TOKEN
 * MIME         ::= "!"? "<"        CHAR+  ">"
 * UNDECORATED  ::= CHAR+
 *
 * REGEX_FLAGS  ::= REGEX_FLAG*
 * REGEX_FLAG   ::= "i" | "I"
 * REGEX_CHAR   ::= "\"? CHAR
 *
 * BORDER_TOKEN ::= EOL | "!" | "{" | "{{" | "/" | "//" | "<"
 * CHAR         ::= <any character> */

/* Breaks concatenated patterns or lists of patterns into separate lines.
 * Returns the list of length *count. */
TSTATIC char **
break_into_matchers(const char concat[], int *count, int is_list)
{
	char **list = NULL;
	int len = 0;

	const char *start = concat;
	parsing_state_t state = { .input = start, .tok = BEGIN, .is_list = is_list };
	load_token(&state, 0);
	do
	{
		size_t width;
		char *piece;

		if(is_list ? !find_patterns(&state) : !find_pattern(&state))
		{
			free_string_array(list, len);
			list = NULL;
			*count = add_to_string_array(&list, 0, concat);
			return list;
		}

		width = state.input - start + 1U;
		piece = malloc(width);
		copy_str(piece, width, start);
		len = put_into_string_array(&list, len, piece);

		/* Skip contiguous separators. */
		while(is_list && state.tok == SYM && *state.input == ',')
		{
			load_token(&state, 0);
		}

		start = state.input;
	}
	while(state.tok != END);

	*count = len;
	return list;
}

/* Reads PATTERNS of PATTERN_LIST.  Returns non-zero on success, otherwise zero
 * is returned. */
static int
find_patterns(parsing_state_t *state)
{
	while(find_pattern(state))
	{
		/* Just advance here. */
	}

	return (state->tok == SYM && *state->input == ',') || state->tok == END;
}

/* PATTERN    ::= NAME_GLOB | PATH_GLOB | NAME_REGEX | PATH_REGEX | MIME
 * NAME_GLOB  ::= "!"? "{" CHAR+ "}" BORDER_TOKEN
 * PATH_GLOB  ::= "!"? "{{" CHAR+ "}}" BORDER_TOKEN
 * NAME_REGEX ::= "!"? "/" REGEX_CHAR+ "/" REGEX_FLAGS BORDER_TOKEN
 * PATH_REGEX ::= "!"? "//" REGEX_CHAR+ "//" REGEX_FLAGS BORDER_TOKEN
 * MIME       ::= "!"? "<" CHAR+ ">"
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_pattern(parsing_state_t *state)
{
	return find_pat(state, LCB, RCB)
	    || find_pat(state, DLCB, DRCB)
	    || find_regex(state, SLASH)
	    || find_regex(state, DSLASH)
	    || find_pat(state, LT, GT);
}

/* Finds name or path regex.  Returns non-zero on success, otherwise zero is
 * returned. */
static int
find_regex(parsing_state_t *state, TokenType decor)
{
	size_t length = 0U;
	const parsing_state_t prev_state = *state;
	const int single_char = (get_token_width(decor) == 1);
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != decor)
	{
		goto mismatch;
	}
	do
	{
		load_token(state, single_char);
		if(state->tok == BSLASH)
		{
			load_token(state, single_char);
			load_token(state, single_char);
		}
		length += (state->tok != decor && state->tok != END);
	}
	while(state->tok != decor && state->tok != END);
	if(state->tok != decor || length == 0U)
	{
		goto mismatch;
	}
	do
	{
		load_token(state, 0);
	}
	while(state->tok == SYM && char_is_one_of("iI", state->input[0]));
	if(!is_at_bound(state))
	{
		goto mismatch;
	}
	return 1;

mismatch:
	*state = prev_state;
	return 0;
}

/* Finds name, path glob or mime-type pattern.  Returns non-zero on success,
 * otherwise zero is returned. */
static int
find_pat(parsing_state_t *state, TokenType left, TokenType right)
{
	size_t length = 0U;
	const parsing_state_t prev_state = *state;
	const int single_char = (get_token_width(left) == 1);
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != left)
	{
		goto mismatch;
	}
	do
	{
		load_token(state, single_char);
		length += (state->tok != right && state->tok != END);
	}
	while(state->tok != right && state->tok != END);
	if(state->tok != right || length == 0U)
	{
		goto mismatch;
	}
	load_token(state, 0);
	if(!is_at_bound(state))
	{
		goto mismatch;
	}
	return 1;

mismatch:
	*state = prev_state;
	return 0;
}

/* Checks whether current token is a valid ending of second level nonterminals.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_at_bound(const parsing_state_t *state)
{
	return state->tok == EMARK
	    || state->tok == LT
	    || state->tok == LCB
	    || state->tok == DLCB
	    || state->tok == SLASH
	    || state->tok == DSLASH
	    || state->tok == END
	    || (state->is_list && state->tok == SYM && *state->input == ',');
}

/* Loads next token from input.  Optionally tokens longer than single character
 * are ignored.  Modifies the *state. */
static void
load_token(parsing_state_t *state, int single_char)
{
	const int sc = single_char;
	const char *in;

	state->input += get_token_width(state->tok);
	in = state->input;

	switch(in[0])
	{
		case '\0': state->tok = END;    break;
		case '\\': state->tok = BSLASH; break;

		case '<': state->tok = LT;    break;
		case '>': state->tok = GT;    break;
		case '!': state->tok = EMARK; break;

		case '{': state->tok = (!sc && in[1] == '{') ? DLCB : LCB;     break;
		case '}': state->tok = (!sc && in[1] == '}') ? DRCB : RCB;     break;
		case '/': state->tok = (!sc && in[1] == '/') ? DSLASH : SLASH; break;

		default:
			state->tok = SYM;
			break;
	}
}

/* Obtains token width in characters.  Returns the width. */
static int
get_token_width(TokenType tok)
{
	switch(tok)
	{
		case DLCB:
		case DRCB:
		case DSLASH:
			return 2;
		case BEGIN:
		case END:
			return 0;

		default:
			return 1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
