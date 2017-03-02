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

/* The parsing and evaluation are mostly separated.  Currently parsing evaluates
 * values without side-effects, but this can be changed later.
 *
 * Output of parsing phase is an expression tree, which is made of nodes of type
 * expr_t.  After parsing they either contain literals or specification of how
 * their value should be evaluated.  Expression value is evaluated at most once.
 *
 * There are two types of evaluation-time operations (part of Ops enumeration):
 *  1. With specific evaluation order requirements.
 *  2. With evaluation of all arguments before the node.
 *
 * First type corresponds to logical AND and OR operations, which evaluate their
 * arguments lazily from left to right.
 *
 * Second type is for the rest of builtins and user-provided functions.
 *
 * parse_or_expr() is a root-level parser of expressions and it basically
 * performs parsing phase.  parse_or_expr() evaluates expression, which is the
 * second phase.
 *
 * If parsing stops before the end of an expression, partial result is stored in
 * global variables to be queried by client code (this way expressions can
 * follow one another on a line and parsed sequentially). */

#include "parsing.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isalnum() isalpha() tolower() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h> /* strcat() strcmp() strncpy() */

#include "../compat/reallocarray.h"
#include "../utils/str.h"
#include "private/options.h"
#include "functions.h"
#include "options.h"
#include "var.h"
#include "variables.h"

#define VAR_NAME_LENGTH_MAX 1024
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
	AND,        /* Logical AND operator (&&). */
	OR,         /* Logical OR operator (||). */
	WHITESPACE, /* Any of whitespace characters (space, tabulation). */
	PLUS,       /* Plus sign (+). */
	MINUS,      /* Minus sign (-). */
	DIGIT,      /* Digit ([0-9]). */
	SYM,        /* Any other symbol with no spec. meaning in current context. */
	END         /* End of a string, after everything is parsed. */
}
TOKENS_TYPE;

/* Types of evaluation operations. */
typedef enum
{
	OP_NONE, /* The node has already been evaluated or is a literal. */
	OP_OR,   /* Logical OR. */
	OP_AND,  /* Logical AND. */
	OP_CALL, /* Builtin operator implemented as a function or builtin function. */
}
Ops;

/* Defines expression and how to evaluate its value.  Value is stored here after
 * evaluation. */
typedef struct expr_t
{
	var_t value;        /* Value of a literal or result of evaluation. */
	Ops op_type;        /* Type of operation. */
	char *func;         /* Function (builtin or user) name for OP_CALL. */
	int nops;           /* Number of operands. */
	struct expr_t *ops; /* Operands. */
}
expr_t;

static int eval_expr(expr_t *expr);
static int eval_or_op(int nops, expr_t ops[], var_t *result);
static int eval_and_op(int nops, expr_t ops[], var_t *result);
static int eval_call_op(const char name[], int nops, expr_t ops[],
		var_t *result);
static int compare_variables(TOKENS_TYPE operation, var_t lhs, var_t rhs);
static var_t eval_concat(int nops, expr_t ops[]);
static int add_expr_op(expr_t *expr, const expr_t *arg);
static void free_expr(const expr_t *expr);
static expr_t parse_or_expr(const char **in);
static expr_t parse_and_expr(const char **in);
static expr_t parse_comp_expr(const char **in);
static int is_comparison_operator(TOKENS_TYPE type);
static expr_t parse_concat_expr(const char **in);
static expr_t parse_term(const char **in);
static expr_t parse_signed_number(const char **in);
static var_t parse_number(const char **in);
static var_t parse_singly_quoted_string(const char **in);
static int parse_singly_quoted_char(const char **in, char buffer[]);
static var_t parse_doubly_quoted_string(const char **in);
static int parse_doubly_quoted_char(const char **in, char buffer[]);
static var_t eval_envvar(const char **in);
static var_t eval_builtinvar(const char **in);
static var_t eval_opt(const char **in);
static expr_t parse_logical_not(const char **in);
static int parse_sequence(const char **in, const char first[],
		const char other[], size_t buf_len, char buf[]);
static expr_t parse_funccall(const char **in);
static void parse_arglist(const char **in, expr_t *call_expr);
static void skip_whitespace_tokens(const char **in);
static void get_next(const char **in);

/* This contains information about the last tokens read. */
static struct
{
	TOKENS_TYPE type; /* Type of the token. */
	char c;           /* Last character of the token. */
	char str[3];      /* Full token string. */
}
prev_token, last_token;

static int initialized;
static getenv_func getenv_fu;
static ParsingErrors last_error;
static const char *last_position;
static const char *last_parsed_char;
static var_t res_val;

/* Empty expression to be returned on errors. */
static expr_t null_expr;

/* Public interface --------------------------------------------------------- */

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
	expr_t expr_root;

	assert(initialized && "Parser must be initialized before use.");

	last_error = PE_NO_ERROR;
	last_token.type = BEGIN;

	last_position = input;
	get_next(&last_position);
	expr_root = parse_or_expr(&last_position);
	last_parsed_char = last_position;

	if(last_token.type != END)
	{
		if(last_parsed_char > input)
		{
			last_parsed_char--;
		}
		if(last_error == PE_NO_ERROR)
		{
			if(last_token.type == DQ && strchr(last_position, '"') == NULL)
			{
				/* This is a comment, just ignore it. */
				last_position += strlen(last_position);
			}
			else if(eval_expr(&expr_root) == 0)
			{
				var_free(res_val);
				res_val = var_clone(expr_root.value);
				last_error = PE_INVALID_EXPRESSION;
			}
		}
	}

	if(last_error == PE_NO_ERROR)
	{
		if(eval_expr(&expr_root) == 0)
		{
			var_free(res_val);
			res_val = var_clone(expr_root.value);
			*result = var_clone(expr_root.value);
		}
	}

	if(last_error == PE_INVALID_EXPRESSION)
	{
		last_position = skip_whitespace(input);
	}

	free_expr(&expr_root);
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
	assert(initialized && "Parser must be initialized before use.");
	return prev_token.type == WHITESPACE;
}

/* Expression evaluation ---------------------------------------------------- */

/* Evaluates values of an expression.  Returns zero on success, which means that
 * expr->value is now correct, otherwise non-zero is returned. */
static int
eval_expr(expr_t *expr)
{
	int result = 1;
	switch(expr->op_type)
	{
		case OP_NONE:
			/* Do nothing, value is already available. */
			return 0;
		case OP_OR:
			result = eval_or_op(expr->nops, expr->ops, &expr->value);
			break;
		case OP_AND:
			result = eval_and_op(expr->nops, expr->ops, &expr->value);
			break;
		case OP_CALL:
			assert(expr->func != NULL && "Function must have a name.");
			result = eval_call_op(expr->func, expr->nops, expr->ops, &expr->value);
			break;
	}
	if(result == 0)
	{
		expr->op_type = OP_NONE;
	}
	return result;
}

/* Evaluates logical OR operation.  All operands are evaluated lazily from left
 * to right.  Returns zero on success, otherwise non-zero is returned. */
static int
eval_or_op(int nops, expr_t ops[], var_t *result)
{
	int val;
	int i;

	if(nops == 0)
	{
		*result = var_true();
		return 0;
	}

	if(eval_expr(&ops[0]) != 0)
	{
		return 1;
	}

	if(nops == 1)
	{
		*result = var_clone(ops[0].value);
		return 0;
	}

	/* Conversion to integer so that strings are converted into numbers instead of
	 * checked to be empty. */
	val = var_to_integer(ops[0].value);

	for(i = 1; i < nops && !val; ++i)
	{
		if(eval_expr(&ops[i]) != 0)
		{
			return 1;
		}
		val |= var_to_integer(ops[i].value);
	}

	*result = var_from_bool(val);
	return 0;
}

/* Evaluates logical AND operation.  All operands are evaluated lazily from left
 * to right.  Returns zero on success, otherwise non-zero is returned. */
static int
eval_and_op(int nops, expr_t ops[], var_t *result)
{
	int val;
	int i;

	if(nops == 0)
	{
		*result = var_false();
		return 0;
	}

	if(eval_expr(&ops[0]) != 0)
	{
		return 1;
	}

	if(nops == 1)
	{
		*result = var_clone(ops[0].value);
		return 0;
	}

	/* Conversion to integer so that strings are converted into numbers instead of
	 * checked to be empty. */
	val = var_to_integer(ops[0].value);

	for(i = 1; i < nops && val; ++i)
	{
		if(eval_expr(&ops[i]) != 0)
		{
			return 1;
		}
		val &= var_to_integer(ops[i].value);
	}

	*result = var_from_bool(val);
	return 0;
}

/* Evaluates invocation operation.  All operands are evaluated beforehand.
 * Returns zero on success, otherwise non-zero is returned. */
static int
eval_call_op(const char name[], int nops, expr_t ops[], var_t *result)
{
	int i;

	for(i = 0; i < nops; ++i)
	{
		if(eval_expr(&ops[i]) != 0)
		{
			return 1;
		}
	}

	if(strcmp(name, "==") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(EQ, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, "!=") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(NE, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, "<") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(LT, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, "<=") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(LE, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, ">") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(GT, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, ">=") == 0)
	{
		assert(nops == 2 && "Must be two arguments.");
		*result = var_from_bool(compare_variables(GE, ops[0].value, ops[1].value));
	}
	else if(strcmp(name, ".") == 0)
	{
		*result = eval_concat(nops, ops);
	}
	else if(strcmp(name, "!") == 0)
	{
		assert(nops == 1 && "Must be single argument.");
		*result = var_from_bool(!var_to_integer(ops[0].value));
	}
	else if(strcmp(name, "-") == 0 || strcmp(name, "+") == 0)
	{
		assert(nops == 1 && "Must be single argument.");
		var_val_t val = { .integer = var_to_integer(ops[0].value) };
		if(name[0] == '-')
		{
			val.integer = -val.integer;
		}
		*result = var_new(VTYPE_INT, val);
	}
	else
	{
		int i;
		call_info_t call_info;
		function_call_info_init(&call_info);

		for(i = 0; i < nops; ++i)
		{
			function_call_info_add_arg(&call_info, var_clone(ops[i].value));
		}

		*result = function_call(name, &call_info);
		if(result->type == VTYPE_ERROR)
		{
			last_error = PE_INVALID_EXPRESSION;
			var_free(*result);
			*result = var_false();
		}
		function_call_info_free(&call_info);
	}

	return (last_error != PE_NO_ERROR);
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

/* Evaluates concatenation of expressions.  Returns resultant value or variable
 * of type VTYPE_ERROR. */
static var_t
eval_concat(int nops, expr_t ops[])
{
	var_t result = var_error();
	int i;

	assert(nops > 0 && "Must be at least one argument.");

	if(nops == 1)
	{
		return var_clone(ops[0].value);
	}

	char res[CMD_LINE_LENGTH_MAX];
	size_t res_len = 0U;
	res[0] = '\0';

	for(i = 0; i < nops; ++i)
	{
		char *const str_val = var_to_string(ops[i].value);
		if(str_val == NULL)
		{
			last_error = PE_INTERNAL;
			break;
		}

		copy_str(res + res_len, sizeof(res) - res_len, str_val);
		res_len += strlen(res + res_len);
		free(str_val);
	}

	if(last_error == PE_NO_ERROR)
	{
		const var_val_t var_val = { .string = res };
		result = var_new(VTYPE_STRING, var_val);
	}

	return result;
}

/* Appends operand to an expression.  Returns zero on success, otherwise
 * non-zero is returned and the *op is freed. */
static int
add_expr_op(expr_t *expr, const expr_t *op)
{
	void *p = reallocarray(expr->ops, expr->nops + 1, sizeof(expr->ops[0]));
	if(p == NULL)
	{
		free_expr(op);
		return 1;
	}
	expr->ops = p;

	expr->ops[expr->nops++] = *op;
	return 0;
}

/* Frees all resources associated with the expression. */
static void
free_expr(const expr_t *expr)
{
	int i;

	free(expr->func);
	var_free(expr->value);

	for(i = 0; i < expr->nops; ++i)
	{
		free_expr(&expr->ops[i]);
	}
	free(expr->ops);
}

/* Input parsing ------------------------------------------------------------ */

/* or_expr ::= and_expr | and_expr '||' or_expr */
static expr_t
parse_or_expr(const char **in)
{
	expr_t result = { .op_type = OP_OR };

	while(last_error == PE_NO_ERROR)
	{
		const expr_t op = parse_and_expr(in);
		if(last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			last_error = PE_INTERNAL;
			break;
		}

		if(last_token.type != OR)
		{
			/* Return partial result. */
			break;
		}

		get_next(in);
	}

	if(last_error == PE_INTERNAL)
	{
		free_expr(&result);
		result.op_type = OP_NONE;
		result.value = var_false();
	}

	return result;
}

/* and_expr ::= comp_expr | comp_expr '&&' and_expr */
static expr_t
parse_and_expr(const char **in)
{
	expr_t result = { .op_type = OP_AND };

	while(last_error == PE_NO_ERROR)
	{
		const expr_t op = parse_comp_expr(in);
		if(last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			last_error = PE_INTERNAL;
			break;
		}

		if(last_token.type != AND)
		{
			/* Return partial result. */
			break;
		}

		get_next(in);
	}

	if(last_error == PE_INTERNAL)
	{
		free_expr(&result);
		result.op_type = OP_NONE;
		result.value = var_false();
	}

	return result;
}

/* comp_expr ::= concat_expr | concat_expr op concat_expr
 * op ::= '==' | '!=' | '<' | '<=' | '>' | '>=' */
static expr_t
parse_comp_expr(const char **in)
{
	expr_t lhs;
	expr_t rhs;
	expr_t result = { .op_type = OP_CALL };

	lhs = parse_concat_expr(in);
	if(last_error != PE_NO_ERROR || !is_comparison_operator(last_token.type))
	{
		return lhs;
	}

	result.func = strdup(last_token.str);

	if(add_expr_op(&result, &lhs) != 0 || result.func == NULL)
	{
		free_expr(&result);
		last_error = PE_INTERNAL;
		return null_expr;
	}

	get_next(in);
	rhs = parse_concat_expr(in);
	if(add_expr_op(&result, &rhs) != 0)
	{
		free_expr(&result);
		last_error = PE_INTERNAL;
		return null_expr;
	}

	if(last_error != PE_NO_ERROR)
	{
		free_expr(&result);
		return null_expr;
	}

	return result;
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

/* concat_expr ::= term { '.' term } */
static expr_t
parse_concat_expr(const char **in)
{
	expr_t result = { .op_type = OP_CALL };

	result.func = strdup(".");
	if(result.func == NULL)
	{
		last_error = PE_INTERNAL;
	}

	while(last_error == PE_NO_ERROR)
	{
		expr_t op;

		skip_whitespace_tokens(in);
		op = parse_term(in);
		skip_whitespace_tokens(in);

		if(last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			last_error = PE_INTERNAL;
			break;
		}

		if(last_token.type != DOT)
		{
			/* Return partial result. */
			break;
		}

		get_next(in);
	}

	if(last_error == PE_INTERNAL)
	{
		free_expr(&result);
		result = null_expr;
	}

	return result;
}

/* term ::= signed_number | number | sqstr | dqstr | envvar | builtinvar |
 *          funccall | opt | logical_not */
static expr_t
parse_term(const char **in)
{
	expr_t result = { .op_type = OP_NONE };

	switch(last_token.type)
	{
		case MINUS:
		case PLUS:
			result = parse_signed_number(in);
			break;
		case DIGIT:
			result.value = parse_number(in);
			break;
		case SQ:
			get_next(in);
			result.value = parse_singly_quoted_string(in);
			break;
		case DQ:
			get_next(in);
			result.value = parse_doubly_quoted_string(in);
			break;
		case DOLLAR:
			get_next(in);
			result.value = eval_envvar(in);
			break;
		case AMPERSAND:
			get_next(in);
			result.value = eval_opt(in);
			break;
		case EMARK:
			get_next(in);
			result = parse_logical_not(in);
			break;

		case SYM:
			if(char_is_one_of("abcdefghijklmnopqrstuvwxyz_", tolower(last_token.c)))
			{
				if(**in == ':')
				{
					result.value = eval_builtinvar(in);
				}
				else
				{
					result = parse_funccall(in);
				}
				break;
			}
			/* break is omitted intentionally. */

		default:
			--*in;
			last_error = PE_INVALID_EXPRESSION;
			result.value = var_error();
			break;
	}
	return result;
}

/* signed_number ::= ( + | - ) { + | - } term */
static expr_t
parse_signed_number(const char **in)
{
	const int sign = (last_token.type == MINUS) ? -1 : 1;
	expr_t result = { .op_type = OP_CALL };
	expr_t op;

	get_next(in);
	skip_whitespace_tokens(in);

	op = parse_term(in);
	if(last_error != PE_NO_ERROR)
	{
		free_expr(&op);
		return null_expr;
	}

	result.func = strdup((sign == 1) ? "+" : "-");
	if(add_expr_op(&result, &op) != 0 || result.func == NULL)
	{
		free_expr(&result);
		last_error = PE_INTERNAL;
		return null_expr;
	}

	return result;
}

/* number ::= num { num } */
static var_t
parse_number(const char **in)
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
parse_singly_quoted_string(const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX];
	buffer[0] = '\0';
	while(parse_singly_quoted_char(in, buffer));

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
parse_singly_quoted_char(const char **in, char buffer[])
{
	int double_sq;
	int sq_char;

	double_sq = (last_token.type == SQ && **in == '\'');
	sq_char = last_token.type != SQ && last_token.type != END;
	if(!sq_char && !double_sq)
	{
		return 0;
	}

	strcat(buffer, last_token.str);
	get_next(in);

	if(double_sq)
	{
		get_next(in);
	}
	return 1;
}

/* dqstr ::= ''' dqchar { dqchar } ''' */
static var_t
parse_doubly_quoted_string(const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX];
	buffer[0] = '\0';
	while(parse_doubly_quoted_char(in, buffer));

	if(last_token.type == DQ)
	{
		const var_val_t var_val = { .string = buffer };
		get_next(in);
		return var_new(VTYPE_STRING, var_val);
	}

	last_error = (last_token.type == END)
	           ? PE_INVALID_EXPRESSION
	           : PE_MISSING_QUOTE;
	return var_false();
}

/* dqchar
 * Returns non-zero if there are more characters in the string. */
int
parse_doubly_quoted_char(const char **in, char buffer[])
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
		strcatch(buffer, table[(int)last_token.c]);
	}
	else
	{
		strcat(buffer, last_token.str);
	}

	get_next(in);
	return dq_char;
}

/* envvar ::= '$' envvarname */
static var_t
eval_envvar(const char **in)
{
	var_val_t var_val;

	char name[VAR_NAME_LENGTH_MAX];
	if(!parse_sequence(in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
		sizeof(name), name))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	var_val.const_string = getenv_fu(name);
	return var_new(VTYPE_STRING, var_val);
}

/* builtinvar ::= 'v:' varname */
static var_t
eval_builtinvar(const char **in)
{
	var_t var_value;
	char name[VAR_NAME_LENGTH_MAX];
	strcpy(name, "v:");

	if(last_token.c != 'v' || **in != ':')
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	get_next(in);
	get_next(in);

	/* XXX: re-using environment variable constants, but could make new ones. */
	if(!parse_sequence(in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
				sizeof(name) - 2U, &name[2]))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	var_value = getvar(name);
	if(var_value.type == VTYPE_ERROR)
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	return var_clone(var_value);
}

/* envvar ::= '&' [ 'l:' | 'g:' ] optname */
static var_t
eval_opt(const char **in)
{
	OPT_SCOPE scope = OPT_ANY;
	const opt_t *option;
	var_val_t var_val;

	char name[OPTION_NAME_MAX + 1];

	if((last_token.c == 'l' || last_token.c == 'g') && **in == ':')
	{
		scope = (last_token.c == 'l') ? OPT_LOCAL : OPT_GLOBAL;
		get_next(in);
		get_next(in);
	}

	if(!parse_sequence(in, OPT_NAME_FIRST_CHAR, OPT_NAME_CHARS, sizeof(name),
		name))
	{
		last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	option = find_option(name, scope);
	if(option == NULL)
	{
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
static expr_t
parse_logical_not(const char **in)
{
	expr_t result = { .op_type = OP_CALL };
	expr_t op;

	skip_whitespace_tokens(in);

	op = parse_term(in);
	if(last_error != PE_NO_ERROR)
	{
		free_expr(&op);
		return null_expr;
	}

	if(add_expr_op(&result, &op) != 0)
	{
		last_error = PE_INTERNAL;
		return null_expr;
	}

	result.func = strdup("!");
	if(result.func == NULL)
	{
		last_error = PE_INTERNAL;
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* sequence ::= first { other }
 * Returns zero on failure, otherwise non-zero is returned. */
static int
parse_sequence(const char **in, const char first[], const char other[],
		size_t buf_len, char buf[])
{
	if(buf_len == 0UL || !char_is_one_of(first, last_token.c))
	{
		return 0;
	}

	buf[0] = '\0';

	do
	{
		strcatch(buf, last_token.c);
		get_next(in);
	}
	while(--buf_len > 1UL && char_is_one_of(other, last_token.c));

	return 1;
}

/* funccall ::= varname '(' [arglist] ')' */
static expr_t
parse_funccall(const char **in)
{
	char *name;
	size_t name_len;
	expr_t result = { .op_type = OP_CALL };

	if(!isalpha(last_token.c))
	{
		last_error = PE_INVALID_EXPRESSION;
		return null_expr;
	}

	name = strdup(last_token.str);
	name_len = strlen(name);
	get_next(in);
	while(last_token.type == SYM && isalnum(last_token.c))
	{
		if(strappendch(&name, &name_len, last_token.c) != 0)
		{
			free(name);
			last_error = PE_INTERNAL;
			return null_expr;
		}
		get_next(in);
	}

	if(last_token.type != LPAREN || !function_registered(name))
	{
		free(name);
		last_error = PE_INVALID_EXPRESSION;
		return null_expr;
	}

	result.func = name;

	get_next(in);
	skip_whitespace_tokens(in);

	/* If argument list is not empty. */
	if(last_token.type != RPAREN)
	{
		const char *old_in = *in - 1;
		parse_arglist(in, &result);
		if(last_error != PE_NO_ERROR)
		{
			*in = old_in;
			free_expr(&result);
			return null_expr;
		}
	}

	skip_whitespace_tokens(in);
	if(last_token.type != RPAREN)
	{
		last_error = PE_INVALID_EXPRESSION;
	}
	get_next(in);

	return result;
}

/* arglist ::= or_expr { ',' or_expr } */
static void
parse_arglist(const char **in, expr_t *call_expr)
{
	do
	{
		const expr_t op = parse_or_expr(in);
		if(last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(call_expr, &op) != 0)
		{
			last_error = PE_INTERNAL;
			break;
		}

		skip_whitespace_tokens(in);

		if(last_token.type != COMMA)
		{
			break;
		}
		get_next(in);
	}
	while(last_error == PE_NO_ERROR);

	if(last_error == PE_INVALID_EXPRESSION)
	{
		last_error = PE_INVALID_SUBEXPRESSION;
	}
}

/* Input tokenization ------------------------------------------------------- */

/* Skips series of consecutive whitespace. */
static void
skip_whitespace_tokens(const char **in)
{
	while(last_token.type == WHITESPACE)
	{
		get_next(in);
	}
}

/* Gets next token from input.  Configures last_token global variable. */
static void
get_next(const char **in)
{
	const char *const start = *in;
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
		case '&':
			if((*in)[1] == '&')
			{
				tt = AND;
				++*in;
			}
			else
			{
				tt = AMPERSAND;
			}
			break;
		case '|':
			if((*in)[1] == '|')
			{
				tt = OR;
				++*in;
			}
			else
			{
				tt = SYM;
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
			/* break is omitted intentionally. */

		default:
			tt = SYM;
			break;
	}
	prev_token = last_token;
	last_token.c = **in;
	last_token.type = tt;

	if(tt != END)
		++*in;

	strncpy(last_token.str, start, *in - start);
	last_token.str[*in - start] = '\0';
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
