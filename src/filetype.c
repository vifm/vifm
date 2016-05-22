/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#include "filetype.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isspace() */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strdup() strcasecmp() */

#include "compat/fs_limits.h"
#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h"
#include "utils/matcher.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"

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
TokensType;

/* Parser state. */
typedef struct
{
	const char *input; /* Current input position. */
	TokensType tok;    /* Current token during parsing of patterns. */
}
parsing_state_t;

static const char * find_existing_cmd(const assoc_list_t *record_list,
		const char file[]);
static assoc_record_t find_existing_cmd_record(const assoc_records_t *records);
static void assoc_programs(matcher_t *matcher, const assoc_records_t *programs,
		int for_x, int in_x);
static assoc_records_t parse_command_list(const char cmds[], int with_descr);
static void register_assoc(assoc_t assoc, int for_x, int in_x);
static assoc_records_t clone_all_matching_records(const char file[],
		const assoc_list_t *record_list);
static void add_assoc(assoc_list_t *assoc_list, assoc_t assoc);
static void assoc_viewers(matcher_t *matcher, const assoc_records_t *viewers);
static assoc_records_t clone_assoc_records(const assoc_records_t *records,
		const char pattern[], const assoc_list_t *dst);
static void reset_all_lists(void);
static void add_defaults(int in_x);
static void reset_list(assoc_list_t *assoc_list);
static void reset_list_head(assoc_list_t *assoc_list);
static void free_assoc_record(assoc_record_t *record);
static void undouble_commas(char s[]);
static void free_assoc(assoc_t *assoc);
static void safe_free(char **adr);
static int is_assoc_record_empty(const assoc_record_t *record);
TSTATIC char ** break_into_matchers(const char concat[], int *count);
static int find_pattern(parsing_state_t *state);
static int find_name_glob(parsing_state_t *state);
static int find_path_glob(parsing_state_t *state);
static int find_name_regex(parsing_state_t *state);
static int find_path_regex(parsing_state_t *state);
static int find_mime(parsing_state_t *state);
static int is_at_bound(TokensType tok);
static void load_token(parsing_state_t *state, int single_char);
static int get_token_width(TokensType tok);

/* Predefined builtin command. */
const assoc_record_t NONE_PSEUDO_PROG = {
	.command = "",
	.description = "",
};

/* Internal list that stores only currently active associations.
 * Since it holds only copies of structures from filetype and filextype lists,
 * it doesn't consume much memory, and its items shouldn't be freed */
static assoc_list_t active_filetypes;

/* Used to set type of new association records */
static assoc_record_type_t new_records_type = ART_CUSTOM;

/* Pointer to external command existence check function. */
static external_command_exists_t external_command_exists_func;

void
ft_init(external_command_exists_t ece_func)
{
	external_command_exists_func = ece_func;
}

const char *
ft_get_program(const char file[])
{
	return find_existing_cmd(&active_filetypes, file);
}

const char *
ft_get_viewer(const char file[])
{
	return find_existing_cmd(&fileviewers, file);
}

/* Finds first existing command which pattern matches given file.  Returns the
 * command (it's lifetime is managed by this unit) or NULL on failure. */
static const char *
find_existing_cmd(const assoc_list_t *record_list, const char file[])
{
	int i;

	for(i = 0; i < record_list->count; ++i)
	{
		assoc_record_t prog;
		assoc_t *const assoc = &record_list->list[i];

		if(!matcher_matches(assoc->matcher, file))
		{
			continue;
		}

		prog = find_existing_cmd_record(&assoc->records);
		if(!is_assoc_record_empty(&prog))
		{
			return prog.command;
		}
	}

	return NULL;
}

/* Finds record that corresponds to an external command that is available.
 * Returns the record on success or an empty record on failure. */
static assoc_record_t
find_existing_cmd_record(const assoc_records_t *records)
{
	static assoc_record_t empty_record;

	int i;
	for(i = 0; i < records->count; ++i)
	{
		char cmd_name[NAME_MAX];
		(void)extract_cmd_name(records->list[i].command, 0, sizeof(cmd_name),
				cmd_name);

		if(external_command_exists_func == NULL ||
				external_command_exists_func(cmd_name))
		{
			return records->list[i];
		}
	}

	return empty_record;
}

assoc_records_t
ft_get_all_programs(const char file[])
{
	return clone_all_matching_records(file, &active_filetypes);
}

void
ft_set_programs(matcher_t *matcher, const char programs[], int for_x, int in_x)
{
	assoc_records_t prog_records = parse_command_list(programs, 1);
	assoc_programs(matcher, &prog_records, for_x, in_x);
	ft_assoc_records_free(&prog_records);
}

/* Associates pattern with the list of programs either for X or non-X
 * associations and depending on current execution environment. */
static void
assoc_programs(matcher_t *matcher, const assoc_records_t *programs, int for_x,
		int in_x)
{
	const assoc_t assoc = {
		.matcher = matcher,
		.records = clone_assoc_records(programs, matcher_get_expr(matcher),
				for_x ? &xfiletypes : &filetypes),
	};

	register_assoc(assoc, for_x, in_x);
}

/* Parses comma separated list of commands into array of associations.  Returns
 * the list. */
static assoc_records_t
parse_command_list(const char cmds[], int with_descr)
{
	assoc_records_t records = {};

	char *free_this = strdup(cmds);

	char *part = free_this, *state = NULL;
	while((part = split_and_get_dc(part, &state)) != NULL)
	{
		const char *description = "";

		if(with_descr && *part == '{')
		{
			char *p = strchr(part + 1, '}');
			if(p != NULL)
			{
				*p = '\0';
				description = part + 1;
				part = skip_whitespace(p + 1);
			}
		}

		if(part[0] != '\0')
		{
			ft_assoc_record_add(&records, part, description);
		}
	}

	free(free_this);

	return records;
}

/* Registers association in appropriate associations list and possibly in list
 * of active associations, which depends on association type and execution
 * environment. */
static void
register_assoc(assoc_t assoc, int for_x, int in_x)
{
	add_assoc(for_x ? &xfiletypes : &filetypes, assoc);
	if(!for_x || in_x)
	{
		add_assoc(&active_filetypes, assoc);
	}
}

assoc_records_t
ft_get_all_viewers(const char file[])
{
	return clone_all_matching_records(file, &fileviewers);
}

/* Clones all records which pattern matches the file.  Returns list of records
 * composed of clones. */
static assoc_records_t
clone_all_matching_records(const char file[], const assoc_list_t *record_list)
{
	int i;
	assoc_records_t result = {};

	for(i = 0; i < record_list->count; ++i)
	{
		assoc_t *const assoc = &record_list->list[i];
		if(matcher_matches(assoc->matcher, file))
		{
			ft_assoc_record_add_all(&result, &assoc->records);
		}
	}

	return result;
}

void
ft_set_viewers(matcher_t *matcher, const char viewers[])
{
	assoc_records_t view_records = parse_command_list(viewers, 0);
	assoc_viewers(matcher, &view_records);
	ft_assoc_records_free(&view_records);
}

/* Associates pattern with the list of viewers. */
static void
assoc_viewers(matcher_t *matcher, const assoc_records_t *viewers)
{
	const assoc_t assoc = {
		.matcher = matcher,
		.records = clone_assoc_records(viewers, matcher_get_expr(matcher),
				&fileviewers),
	};

	add_assoc(&fileviewers, assoc);
}

/* Clones list of association records.  Returns the clone. */
static assoc_records_t
clone_assoc_records(const assoc_records_t *records, const char pattern[],
		const assoc_list_t *dst)
{
	int i;
	assoc_records_t list = {};

	for(i = 0; i < records->count; ++i)
	{
		if(!ft_assoc_exists(dst, pattern, records->list[i].command))
		{
			ft_assoc_record_add(&list, records->list[i].command,
					records->list[i].description);
		}
	}

	return list;
}

static void
add_assoc(assoc_list_t *assoc_list, assoc_t assoc)
{
	void *p;
	p = reallocarray(assoc_list->list, assoc_list->count + 1, sizeof(assoc_t));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	assoc_list->list = p;
	assoc_list->list[assoc_list->count] = assoc;
	assoc_list->count++;
}

void
ft_reset(int in_x)
{
	reset_all_lists();
	add_defaults(in_x);
}

static void
reset_all_lists(void)
{
	reset_list(&filetypes);
	reset_list(&xfiletypes);
	reset_list(&fileviewers);

	reset_list_head(&active_filetypes);
}

/* Loads default (builtin) associations. */
static void
add_defaults(int in_x)
{
	char *error;
	matcher_t *const m = matcher_alloc("{*/}", 0, 1, "", &error);
	assert(m != NULL && "Failed to allocate builtin matcher!");

	new_records_type = ART_BUILTIN;
	ft_set_programs(m, "{Enter directory}" VIFM_PSEUDO_CMD, 0, in_x);
	new_records_type = ART_CUSTOM;
}

static void
reset_list(assoc_list_t *assoc_list)
{
	int i;
	for(i = 0; i < assoc_list->count; i++)
	{
		free_assoc(&assoc_list->list[i]);
	}
	reset_list_head(assoc_list);
}

static void
reset_list_head(assoc_list_t *assoc_list)
{
	free(assoc_list->list);
	assoc_list->list = NULL;
	assoc_list->count = 0;
}

static void
free_assoc(assoc_t *assoc)
{
	matcher_free(assoc->matcher);
	ft_assoc_records_free(&assoc->records);
}

void
ft_assoc_records_free(assoc_records_t *records)
{
	int i;
	for(i = 0; i < records->count; i++)
	{
		free_assoc_record(&records->list[i]);
	}

	free(records->list);
	records->list = NULL;
	records->count = 0;
}

/* After this call the structure contains NULL values. */
static void
free_assoc_record(assoc_record_t *record)
{
	safe_free(&record->command);
	safe_free(&record->description);
}

int
ft_assoc_exists(const assoc_list_t *assocs, const char pattern[],
		const char cmd[])
{
	int i;
	char *undoubled;

	if(*cmd == '{')
	{
		const char *const descr_end = strchr(cmd + 1, '}');
		if(descr_end != NULL)
		{
			cmd = descr_end + 1;
		}
	}

	undoubled = strdup(cmd);
	undouble_commas(undoubled);

	for(i = 0; i < assocs->count; ++i)
	{
		int j;

		const assoc_t assoc = assocs->list[i];
		if(strcmp(matcher_get_expr(assoc.matcher), pattern) != 0)
		{
			continue;
		}

		for(j = 0; j < assoc.records.count; ++j)
		{
			const assoc_record_t ft_record = assoc.records.list[j];
			if(strcmp(ft_record.command, undoubled) == 0)
			{
				free(undoubled);
				return 1;
			}
		}
	}

	free(undoubled);
	return 0;
}

/* Updates the string in place to squash double commas into single one. */
static void
undouble_commas(char s[])
{
	char *p;

	p = s;
	while(s[0] != '\0')
	{
		if(s[0] == ',' && s[1] == ',')
		{
			++s;
		}
		*p++ = s[0];
		if(s[0] != '\0')
		{
			++s;
		}
	}
	*p = '\0';
}

void
ft_assoc_record_add(assoc_records_t *records, const char *command,
		const char *description)
{
	void *p;
	p = reallocarray(records->list, records->count + 1, sizeof(assoc_record_t));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	records->list = p;
	records->list[records->count].command = strdup(command);
	records->list[records->count].description = strdup(description);
	records->list[records->count].type = new_records_type;
	records->count++;
}

void
ft_assoc_record_add_all(assoc_records_t *assocs, const assoc_records_t *src)
{
	int i;
	void *p;
	const int src_count = src->count;

	if(src_count == 0)
	{
		return;
	}

	p = reallocarray(assocs->list, assocs->count + src_count,
			sizeof(assoc_record_t));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	assocs->list = p;

	for(i = 0; i < src_count; ++i)
	{
		assocs->list[assocs->count + i].command = strdup(src->list[i].command);
		assocs->list[assocs->count + i].description =
				strdup(src->list[i].description);
		assocs->list[assocs->count + i].type = src->list[i].type;
	}

	assocs->count += src_count;
}

static void
safe_free(char **adr)
{
	free(*adr);
	*adr = NULL;
}

/* Returns non-zero for an empty assoc_record_t structure. */
static int
is_assoc_record_empty(const assoc_record_t *record)
{
	return record->command == NULL && record->description == NULL;
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
	const parsing_state_t prev_state = *state;
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != LCB)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 1);
	}
	while(state->tok != RCB && state->tok != END);
	if(state->tok != RCB)
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

/* PATH_GLOB ::= "!"? "{{" CHAR+ "}}" BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_path_glob(parsing_state_t *state)
{
	const parsing_state_t prev_state = *state;
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != DLCB)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 0);
	}
	while(state->tok != DRCB && state->tok != END);
	if(state->tok != DRCB)
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

/* NAME_REGEX ::= "!"? "/" REGEX_CHAR+ "/" REGEX_FLAGS BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_name_regex(parsing_state_t *state)
{
	const parsing_state_t prev_state = *state;
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != SLASH)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 1);
		if(state->tok == BSLASH)
		{
			load_token(state, 1);
			load_token(state, 1);
		}
	}
	while(state->tok != SLASH && state->tok != END);
	if(state->tok != SLASH)
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

/* PATH_REGEX ::= "!"? "//" REGEX_CHAR+ "//" REGEX_FLAGS BORDER_TOKEN
 * Returns non-zero on success, otherwise zero is returned. */
static int
find_path_regex(parsing_state_t *state)
{
	const parsing_state_t prev_state = *state;
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != DSLASH)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 0);
		if(state->tok == BSLASH)
		{
			load_token(state, 0);
			load_token(state, 0);
		}
	}
	while(state->tok != DSLASH && state->tok != END);
	if(state->tok != DSLASH)
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
	const parsing_state_t prev_state = *state;
	if(state->tok == EMARK)
	{
		load_token(state, 0);
	}
	if(state->tok != LT)
	{
		*state = prev_state;
		return 0;
	}
	do
	{
		load_token(state, 1);
	}
	while(state->tok != GT && state->tok != END);
	if(state->tok != GT)
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
is_at_bound(TokensType tok)
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
get_token_width(TokensType tok)
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
/* vim: set cinoptions+=t0 filetype=c : */
