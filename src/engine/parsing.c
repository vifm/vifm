/* vifm
 * Copyright (C) 2012 xaizek.
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

#include "parsing.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isalnum() isalpha() tolower() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h> /* strcat() strcmp() */

#include "../utils/str.h"
#include "private/options.h"
#include "functions.h"
#include "options.h"
#include "var.h"
#include "variables.h"

#define ENVVAR_NAME_LENGTH_MAX 1024
#define NAME_LENGTH_MAX 256
#define CMD_LINE_LENGTH_MAX 4096

/* Supported types of tokens. */
typedef enum
{
	BEGIN,      /* Beginning of a string, before anything is parsed. */
	SQ,         /* Single quote ('). */
	DQ,         /* Double quote ("). */
	DOT,        /* Dot (.). */
	DOLLAR,     /* Dollar sign ($). */
	AMPERSAND,  /* Ampersand sign (&). */
	LPAREN,     /* Left parenthesis ((). */
	RPAREN,     /* Right parenthesis ()). */
	EMARK,      /* Exclamation mark (!). */
	COMMA,      /* Comma, concatenation operator (,). */
	EQ,         /* Equality operator (==). */
	NE,         /* Inequality operator (!=). */
	LT,         /* Less than operator (<). */
	LE,         /* Less than or equal operator (<=). */
	GE,         /* Greater than or equal operator (>=). */
	GT,         /* Greater than operator (>). */
	WHITESPACE, /* Any of whitespace characters (space, tabulation). */
	PLUS,       /* Plus sign (+). */
	MINUS,      /* Minus sign (-). */
	DIGIT,      /* Digit ([0-9]). */
	SYM,        /* Any other symbol that don't have meaning in current context. */
	END         /* End of a string, after everything is parsed. */
}
TOKENS_TYPE;

static var_t eval_statement(const char **in);
static int is_comparison_operator(TOKENS_TYPE type);
static int compare_variables(TOKENS_TYPE operation, var_t lhs, var_t rhs);
static var_t eval_expression(const char **in);
static var_t eval_term(const char **in);
static var_t eval_signed_number(const char **in);
static var_t eval_number(const char **in);
static var_t eval_single_quoted_string(const char **in);
static int eval_single_quoted_char(const char **in, char buffer[]);
static var_t eval_double_quoted_string(const char **in);
static int eval_double_quoted_char(const char **in, char buffer[]);
static var_t eval_envvar(const char **in);
static var_t eval_opt(const char **in);
static var_t eval_logical_not(const char **in);
static int read_sequence(const char **in, const char first[],
		const char other[], size_t buf_len, char buf[]);
static var_t eval_funccall(const char **in);
static void eval_arglist(const char **in, call_info_t *call_info);
static void eval_arg(const char **in, call_info_t *call_info);
static void skip_whitespace_tokens(const char **in);
static void get_next(const char **in);

/* This contains information about the last token read. */
struct
{
	TOKENS_TYPE type;
	char c;
}prev_token, last_token;

static int initialized;
static getenv_func getenv_fu;
static ParsingErrors last_error;
static const char *last_position;
static const char *last_parsed_char;
static var_t res_val;

void
init_parser(getenv_func getenv_f)
{
	getenv_fu = getenv_f;
	initialized = 1;
}

const char *
get_last_position(void)
{
	assert(initialized && "Parser must be initialized before use.");
	return last_position;
}

const char *
get_last_parsed_char(void)
{
	assert(initialized && "Parser must be initialized before use.");
	return last_parsed_char;
}

ParsingErrors
parse(const char input[], var_t *result)
{
	assert(initialized && "Parser must be initialized before use.");

	last_error = PE_NO_ERROR;
	last_token.type = BEGIN;

	var_free(res_val);
	res_val = var_false();
	last_position = input;
	get_next(&last_position);
	res_val = eval_statement(&last_position);
	last_parsed_char = last_position;

	if(last_token.type != END)
	{
		if(last_parsed_char > input)
		{
			last_parsed_char--;
		}
		if(last_error == PE_NO_ERROR)
		{
			last_error = PE_INVALID_EXPRESSION;
		}
	}
	if(last_error == PE_INVALID_EXPRESSION)
	{
		last_position = skip_whitespace(input);
	}

	if(last_error == PE_NO_ERROR)
	{
		*result = var_clone(res_val);
	}
	return last_error;
}

var_t
get_parsing_result(void)
{
	assert(initialized && "Parser must be initialized before use.");
	return var_clone(res_val);
}

int
is_prev_token_whitespace(void)
{
	return prev_token.type == WHITESPACE;
}

/* stmt ::= expr | expr op expr */
/* op ::= '==' | '!=' */
static var_t
eval_statement(const char **in)
{
	TOKENS_TYPE op;
	var_t lhs;
	var_t rhs;
	var_val_t result;

	lhs = eval_expression(in);
	if(last_error != PE_NO_ERROR)
	{
		return lhs;
	}
	else if(last_token.type == END)
	{
		return lhs;
	}
	else if(!is_comparison_operator(last_token.type))
	{
		last_error = PE_INVALID_EXPRESSION;
		/* Return partial result. */
		return lhs;
	}
	op = last_token.type;

	get_next(in);
	rhs = eval_expression(in);
	if(last_error != PE_NO_ERROR)
	{
		var_free(lhs);
		return var_false();
	}

	result.integer = compare_variables(op, lhs, rhs);

	var_free(lhs);
	var_free(rhs);
	return var_new(VTYPE_INT, result);
}

/* Checks whether given token corresponds to any of comparison operators.
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_comparison_operator(TOKENS_TYPE type)
{
	return type == EQ || type == NE
	    || type == LT || type == LE
	    || type == GE || type == GT;
}

/* Compares lhs and rhs variables by comparison operator specified by a token.
 * Returns non-zero for if comparison evaluates to true, otherwise non-zero is
 * returned. */
static int
compare_variables(TOKENS_TYPE operation, var_t lhs, var_t rhs)
{
	if(lhs.type == VTYPE_STRING && rhs.type == VTYPE_STRING)
	{
		const int result = strcmp(lhs.value.string, rhs.value.string);
		switch(operation)
		{
			case EQ: return result == 0;
			case NE: return result != 0;
			case LT: return result < 0;
			case LE: return result <= 0;
			case GE: return result >= 0;
			case GT: return result > 0;

			default:
				assert(0 && "Unhandled comparison operator");
				return 0;
		}
	}
	else
	{
		const int lhs_int = var_to_integer(lhs);
		const int rhs_int = var_to_integer(rhs);
		switch(operation)
		{
			case EQ: return lhs_int == rhs_int;
			case NE: return lhs_int != rhs_int;
			case LT: return lhs_int < rhs_int;
			case LE: return lhs_int <= rhs_int;
			case GE: return lhs_int >= rhs_int;
			case GT: return lhs_int > rhs_int;

			default:
				assert(0 && "Unhandled comparison operator");
				return 0;
		}
	}
}

/* expr ::= term { '.' term } */
static var_t
eval_expression(const char **in)
{
	var_t result = var_false();
	char res[CMD_LINE_LENGTH_MAX];
	size_t res_len = 0U;
	int single_term = 1;
	res[0] = '\0';

	while(last_error == PE_NO_ERROR)
	{
		var_t term;
		char *str_val;

		skip_whitespace_tokens(in);
		term = eval_term(in);
		skip_whitespace_tokens(in);
		if(last_error != PE_NO_ERROR)
		{
			var_free(term);
			break;
		}

		str_val = var_to_string(term);
		res_len += snprintf(res + res_len, sizeof(res) - res_len, "%s", str_val);
		free(str_val);

		if(last_token.type != DOT)
		{
			if(single_term)
			{
				result = term;
			}
			else
			{
				var_free(term);
			}
			break;
		}
		var_free(term);

		single_term = 0;
		get_next(in);
	}

	if(last_error == PE_NO_ERROR)
	{
		if(!single_term)
		{
			const var_val_t var_val = { .string = res };
			result = var_new(VTYPE_STRING, var_val);
		}
	}

	return result;
}

/* term ::= signed_number | number | sqstr | dqstr | envvar | funccall | opt |
 *          logical_not */
static var_t
eval_term(const char **in)
{
	switch(last_token.type)
	{
		case MINUS:
		case PLUS:
			return eval_signed_number(in);
		case DIGIT:
			return eval_number(in);
		case SQ:
			get_next(in);
			return eval_single_quoted_string(in);
		case DQ:
			get_next(in);
			return eval_double_quoted_string(in);
		case DOLLAR:
			get_next(in);
			return eval_envvar(in);
		case AMPERSAND:
			get_next(in);
			return eval_opt(in);
		case EMARK:
			get_next(in);
			return eval_logical_not(in);

		case SYM:
			if(char_is_one_of("abcdefghijklmnopqrstuvwxyz_", tolower(last_token.c)))
			{
				return eval_funccall(in);
			}
			/* break is intensionally omitted. */

		default:
			--*in;
			last_error = PE_INVALID_EXPRESSION;
			return var_false();
	}
}

/* signed_number ::= ( + | - ) { + | - } number */
static var_t
eval_signed_number(const char **in)
{
	const int sign = (last_token.type == MINUS) ? -1 : 1;
	var_t operand;
	int number;
	var_val_t result_val;

	get_next(in);
	skip_whitespace_tokens(in);
	operand = eval_term(in);

	number = var_to_integer(operand);
	var_free(operand);

	result_val.integer = sign*number;
	return var_new(VTYPE_INT, result_val);
}

/* number ::= num { num } */
static var_t
eval_number(const char **in)
{
	var_val_t var_val = { };

	char buffer[CMD_LINE_LENGTH_MAX];
	buffer[0] = '\0';

	do
	{
		strcatch(buffer, last_token.c);
		get_next(in);
	}
	while(last_token.type == DIGIT);

	var_val.integer = str_to_int(buffer);
	return var_new(VTYPE_INT, var_val);
}

/* sqstr ::= ''' sqchar { sqchar } ''' */
static var_t
eval_single_quoted_string(const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX];
	buffer[0] = '\0';
	while(eval_single_quoted_char(in, buffer));

	if(last_token.type == SQ)
	{
		const var_val_t var_val = { .string = buffer };
		get_next(in);
		return var_new(VTYPE_STRING, var_val);
	}

	last_error = PE_MISSING_QUOTE;
	return var_false();
}

/* sqchar
 * Returns non-zero if there are more characters in the string. */
static int
eval_single_quoted_char(const char **in, char buffer[])
{
	int double_sq;
	int sq_char;

	double_sq = (last_token.type == SQ && **in == '\'');
	sq_char = last_token.type != SQ && last_token.type != END;
	if(!sq_char && !double_sq)
	{
		return 0;
	}

	strcatch(buffer, last_token.c);
	get_next(in);

	if(double_sq)
	{
		get_next(in);
	}
	return 1;
}

/* dqstr ::= ''' dqchar { dqchar } ''' */
static var_t
eval_double_quoted_string(const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX];
	buffer[0] = '\0';
	while(eval_double_quoted_char(in, buffer));

	if(last_token.type == DQ)
	{
		const var_val_t var_val = { .string = buffer };
		get_next(in);
		return var_new(VTYPE_STRING, var_val);
	}

	last_error = PE_MISSING_QUOTE;
	return var_false();
}

/* dqchar
 * Returns non-zero if there are more characters in the string. */
int
eval_double_quoted_char(const char **in, char buffer[])
{
	static const char table[] =
						/* 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
	/* 00 */	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	/* 10 */	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	/* 20 */	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	/* 30 */	"\x00\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	/* 40 */	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	/* 50 */	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	/* 60 */	"\x60\x61\x08\x63\x64\x1b\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x0a\x6f"
	/* 70 */	"\x70\x71\x0d\x73\x09\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
	/* 80 */	"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
	/* 90 */	"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
	/* a0 */	"\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
	/* b0 */	"\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
	/* c0 */	"\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
	/* d0 */	"\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
	/* e0 */	"\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
	/* f0 */	"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

	int dq_char;

	dq_char = last_token.type != DQ && last_token.type != END;

	if(!dq_char)
		return 0;

	if(last_token.c == '\\')
	{
		get_next(in);
		if(last_token.type == END)
		{
			last_error = PE_INVALID_EXPRESSION;
			return 0;
		}
		last_token.c = table[(int)last_token.c];
	}

	strcatch(buffer, last_token.c);
	get_next(in);

	return dq_char;
}

/* envvar ::= '$' envvarname */
static var_t
eval_envvar(const char **in)
{
	var_val_t var_val;

	char name[ENVVAR_NAME_LENGTH_MAX];
	if(!read_sequence(in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
		sizeof(name), name))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	var_val.const_string = getenv_fu(name);
	return var_new(VTYPE_STRING, var_val);
}

/* envvar ::= '&' optname */
static var_t
eval_opt(const char **in)
{
	var_val_t var_val;

	char name[OPTION_NAME_MAX];
	if(!read_sequence(in, OPT_NAME_FIRST_CHAR, OPT_NAME_CHARS, sizeof(name),
		name))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	const opt_t *const option = find_option(name);
	if(option == NULL) {
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	switch(option->type)
	{
		case OPT_STR:
		case OPT_STRLIST:
		case OPT_CHARSET:
			var_val.string = option->val.str_val;
			return var_new(VTYPE_STRING, var_val);

		case OPT_BOOL:
			var_val.integer = option->val.bool_val;
			return var_new(VTYPE_INT, var_val);

		case OPT_INT:
			var_val.integer = option->val.int_val;
			return var_new(VTYPE_INT, var_val);

		case OPT_ENUM:
		case OPT_SET:
			var_val.const_string = get_value(option);
			return var_new(VTYPE_STRING, var_val);

		default:
			assert(0 && "Unexpected option type");
			return var_false();
	}
}

/* logical_not ::= '!' term */
static var_t
eval_logical_not(const char **in)
{
	var_t term;
	int bool_val;

	skip_whitespace_tokens(in);

	term = eval_term(in);
	bool_val = var_to_boolean(term);

	var_free(term);

	return var_from_bool(!bool_val);
}

/* sequence ::= first { other }
 * Returns zero on failure, otherwise non-zero is returned. */
static int
read_sequence(const char **in, const char first[], const char other[],
		size_t buf_len, char buf[])
{
	if(buf_len == 0UL || !char_is_one_of(ENV_VAR_NAME_FIRST_CHAR, last_token.c))
	{
		return 0;
	}

	buf[0] = '\0';

	do
	{
		strcatch(buf, last_token.c);
		get_next(in);
	}
	while(--buf_len > 1UL && char_is_one_of(ENV_VAR_NAME_CHARS, last_token.c));

	return 1;
}

/* funccall ::= varname '(' arglist ')' */
static var_t
eval_funccall(const char **in)
{
	char name[NAME_LENGTH_MAX];
	call_info_t call_info;
	var_t ret_val;

	if(!isalpha(last_token.c))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	name[0] = last_token.c;
	name[1] = '\0';
	get_next(in);
	while(last_token.type == SYM && isalnum(last_token.c))
	{
		strcatch(name, last_token.c);
		get_next(in);
	}

	if(last_token.type != LPAREN)
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	get_next(in);
	skip_whitespace_tokens(in);

	function_call_info_init(&call_info);
	/* If argument list is not empty. */
	if(last_token.type != RPAREN)
	{
		const char *old_in = *in - 1;
		eval_arglist(in, &call_info);
		if(last_error != PE_NO_ERROR)
		{
			*in = old_in;
			function_call_info_free(&call_info);
			return var_false();
		}
	}

	ret_val = function_call(name, &call_info);
	if(ret_val.type == VTYPE_ERROR)
	{
		last_error = PE_INVALID_EXPRESSION;
		var_free(ret_val);
		ret_val = var_false();
	}
	function_call_info_free(&call_info);

	skip_whitespace_tokens(in);
	if(last_token.type != RPAREN)
	{
		last_error = PE_INVALID_EXPRESSION;
	}
	get_next(in);

	return ret_val;
}

/* arglist ::= expr { ',' expr } */
static void
eval_arglist(const char **in, call_info_t *call_info)
{
	eval_arg(in, call_info);
	while(last_error == PE_NO_ERROR && last_token.type == COMMA)
	{
		get_next(in);
		eval_arg(in, call_info);
	}

	if(last_error == PE_INVALID_EXPRESSION)
	{
		last_error = PE_INVALID_SUBEXPRESSION;
	}
}

/* arg ::= expr */
static void
eval_arg(const char **in, call_info_t *call_info)
{
	var_t arg = eval_expression(in);
	if(last_error == PE_NO_ERROR)
	{
		function_call_info_add_arg(call_info, arg);
		skip_whitespace_tokens(in);
	}
}

/* Skips series of consecutive whitespace. */
static void
skip_whitespace_tokens(const char **in)
{
	while(last_token.type == WHITESPACE)
	{
		get_next(in);
	}
}

/* Gets next token from input. Configures last_token global variable. */
static void
get_next(const char **in)
{
	TOKENS_TYPE tt;

	if(last_token.type == END)
		return;

	switch((*in)[0])
	{
		case '\'':
			tt = SQ;
			break;
		case '"':
			tt = DQ;
			break;
		case '.':
			tt = DOT;
			break;
		case '$':
			tt = DOLLAR;
			break;
		case '&':
			tt = AMPERSAND;
			break;
		case '(':
			tt = LPAREN;
			break;
		case ')':
			tt = RPAREN;
			break;
		case ',':
			tt = COMMA;
			break;
		case ' ':
		case '\t':
			tt = WHITESPACE;
			break;
		case '\0':
			tt = END;
			break;
		case '+':
			tt = PLUS;
			break;
		case '-':
			tt = MINUS;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			tt = DIGIT;
			break;
		case '<':
			if((*in)[1] == '=')
			{
				++*in;
				tt = LE;
			}
			else
			{
				tt = LT;
			}
			break;
		case '>':
			if((*in)[1] == '=')
			{
				++*in;
				tt = GE;
			}
			else
			{
				tt = GT;
			}
			break;
		case '=':
		case '!':
			if((*in)[1] == '=')
			{
				tt = ((*in)[0] == '=') ? EQ : NE;
				++*in;
				break;
			}
			else if((*in)[0] == '!')
			{
				tt = EMARK;
				break;
			}
			/* break is omitted intensionally. */

		default:
			tt = SYM;
			break;
	}
	prev_token = last_token;
	last_token.c = **in;
	last_token.type = tt;

	if(tt != END)
		++*in;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
