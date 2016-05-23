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
}
parsing_state_t;

TSTATIC char ** break_into_matchers(const char concat[], int *count);
static int find_pattern(parsing_state_t *state);
static int find_name_glob(parsing_state_t *state);
static int find_path_glob(parsing_state_t *state);
static int find_name_regex(parsing_state_t *state);
static int find_path_regex(parsing_state_t *state);
static int find_regex(parsing_state_t *state, TokenType decor);
static int find_mime(parsing_state_t *state);
static int find_pat(parsing_state_t *state, TokenType left, TokenType right);
static int is_at_bound(TokenType tok);
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

	exprs = break_into_matchers(list, &nexprs);
	matchers->count = nexprs;
	matchers->list = reallocarray(NULL, nexprs, sizeof(matcher_t *));
	matchers->expr = strdup(list);
	if(matchers->list == NULL || matchers->expr == NULL)
	{
		free(matchers->list);
		free(matchers->expr);
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

const char *
matchers_get_expr(const matchers_t *matchers)
{
	return matchers->expr;
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

/* Breaks concatenated patterns into separate lines.  Returns the list of length
 * *count. */
TSTATIC char **
break_into_matchers(const char concat[], int *count)
{
	char **list = NULL;
	int len = 0;

	const char *start = concat;
	parsing_state_t state = { .input = start, .tok = BEGIN };
	load_token(&state, 0);
	do
	{
		size_t width;
		char *piece;

		if(!find_pattern(&state))
		{
			free_string_array(list, len);
			list = NULL;
			*count = add_to_string_array(&list, 0, 1, concat);
			return list;
		}

		width = state.input - start + 1U;
		piece = malloc(width);
		copy_str(piece, width, start);
		len = put_into_string_array(&list, len, piece);

		start = state.input;
	}
	while(state.tok != END);

	*count = len;
	return list;
}

/* PATTERN ::= NAME_GLOB | PATH_GLOB | NAME_REGEX | PATH_REGEX | MIME
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_pattern(parsing_state_t *state)
{
	return find_name_glob(state)
	    || find_path_glob(state)
	    || find_name_regex(state)
	    || find_path_regex(state)
	    || find_mime(state);
}

/* NAME_GLOB ::= "!"? "{" CHAR+ "}" BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_name_glob(parsing_state_t *state)
{
	return find_pat(state, LCB, RCB);
}

/* PATH_GLOB ::= "!"? "{{" CHAR+ "}}" BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_path_glob(parsing_state_t *state)
{
	return find_pat(state, DLCB, DRCB);
}

/* NAME_REGEX ::= "!"? "/" REGEX_CHAR+ "/" REGEX_FLAGS BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_name_regex(parsing_state_t *state)
{
	return find_regex(state, SLASH);
}

/* PATH_REGEX ::= "!"? "//" REGEX_CHAR+ "//" REGEX_FLAGS BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_path_regex(parsing_state_t *state)
{
	return find_regex(state, DSLASH);
}

/* Finds name or path regex.  Returns non-zero on success, otherwise zero is
 * returned. */
static int
find_regex(parsing_state_t *state, TokenType decor)
{
	const parsing_state_t prev_state = *state;
	const int single_char = (get_token_width(decor) == 1);
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != decor)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, single_char);
		if(state->tok == BSLASH)
		{
			load_token(state, single_char);
			load_token(state, single_char);
		}
	}
	while(state->tok != decor && state->tok != END);
	if(state->tok != decor)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 0);
	}
	while(state->tok == SYM && char_is_one_of("iI", state->input[0]));
	if(!is_at_bound(state->tok))
	{
		*state = prev_state;
		return 0;
	}
	return 1;
}

/* MIME ::= "!"? "<" CHAR+ ">"
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_mime(parsing_state_t *state)
{
	return find_pat(state, LT, GT);
}

/* Finds name, path glob or mime-type pattern.  Returns non-zero on success,
 * otherwise zero is returned. */
static int
find_pat(parsing_state_t *state, TokenType left, TokenType right)
{
	const parsing_state_t prev_state = *state;
	const int single_char = (get_token_width(left) == 1);
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != left)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, single_char);
	}
	while(state->tok != right && state->tok != END);
	if(state->tok != right)
	{
		*state = prev_state;
		return 0;
	}
	load_token(state, 0);
	if(!is_at_bound(state->tok))
	{
		*state = prev_state;
		return 0;
	}
	return 1;
}

/* Checks whether token is a valid ending of second level nonterminals.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_at_bound(TokenType tok)
{
	return tok == EMARK
	    || tok == LT
	    || tok == LCB
	    || tok == DLCB
	    || tok == SLASH
	    || tok == DSLASH
	    || tok == END;
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
