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

/*
 * The parsing and evaluation are mostly separated.  Currently parsing evaluates
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
 * performs parsing phase.  eval_expr() evaluates expression, which is the
 * second phase.
 *
 * If parsing stops before the end of an expression, partial result is still
 * returned as a result for the API client (this way expressions can follow one
 * another on a line and be parsed sequentially).
 */

#include "parsing.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isalnum() isalpha() tolower() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h> /* strcat() strcmp() strncpy() */

#include "../compat/reallocarray.h"
#include "../utils/str.h"
#include "functions.h"
#include "options.h"
#include "text_buffer.h"
#include "var.h"
#include "variables.h"

#define VAR_NAME_LENGTH_MAX 1024
#define CMD_LINE_LENGTH_MAX 4096

/* Maximum number of characters in option's name. */
static const size_t OPTION_NAME_MAX = 64;

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

/* Information about a single token. */
typedef struct
{
	TOKENS_TYPE type; /* Type of the token. */
	char c;           /* Last character of the token. */
	char str[3];      /* Full token string. */
}
parse_token_t;

/* Context that's passed to almost all functions. */
typedef struct
{
	int interactive; /* Whether parsing is being executed by the user. */

	ParsingErrors last_error; /* Last error (whether in parsing or evaluation). */

	parse_token_t last_token;  /* Current token. */
	parse_token_t prev_token;  /* Previous token. */
	const char *last_position; /* Last position in the input. */
}
parse_context_t;

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

/* Metadata container for static buffer. */
typedef struct
{
	char *data;        /* Pointer to the buffer. */
	size_t len;        /* Current size of the buffer. */
	const size_t size; /* Capacity of the buffer. */
}
sbuffer;

static parsing_result_t parse_from(const char input[],
		expr_t (*production)(parse_context_t *ctx, const char **in), int strict,
		int interactive);
static int eval_expr(parse_context_t *ctx, expr_t *expr);
static int eval_or_op(parse_context_t *ctx, int nops, expr_t ops[],
		var_t *result);
static int eval_and_op(parse_context_t *ctx, int nops, expr_t ops[],
		var_t *result);
static int eval_call_op(parse_context_t *ctx, const char name[], int nops,
		expr_t ops[], var_t *result);
static int compare_variables(TOKENS_TYPE operation, var_t lhs, var_t rhs);
static var_t eval_concat(parse_context_t *ctx, int nops, expr_t ops[]);
static int add_expr_op(expr_t *expr, const expr_t *arg);
static void free_expr(const expr_t *expr);
static expr_t parse_or_expr(parse_context_t *ctx, const char **in);
static expr_t parse_and_expr(parse_context_t *ctx, const char **in);
static expr_t parse_comp_expr(parse_context_t *ctx, const char **in);
static int is_comparison_operator(TOKENS_TYPE type);
static expr_t parse_factor(parse_context_t *ctx, const char **in);
static expr_t parse_concat_expr(parse_context_t *ctx, const char **in);
static expr_t parse_term(parse_context_t *ctx, const char **in);
static expr_t parse_signed_number(parse_context_t *ctx, const char **in);
static var_t parse_number(parse_context_t *ctx, const char **in);
static var_t parse_singly_quoted_string(parse_context_t *ctx, const char **in);
static int parse_singly_quoted_char(parse_context_t *ctx, const char **in,
		sbuffer *sbuf);
static var_t parse_doubly_quoted_string(parse_context_t *ctx, const char **in);
static int parse_doubly_quoted_char(parse_context_t *ctx, const char **in,
		sbuffer *sbuf);
static var_t eval_envvar(parse_context_t *ctx, const char **in);
static var_t eval_builtinvar(parse_context_t *ctx, const char **in);
static var_t eval_opt(parse_context_t *ctx, const char **in);
static expr_t parse_logical_not(parse_context_t *ctx, const char **in);
static int parse_sequence(parse_context_t *ctx, const char **in,
		const char first[], const char other[], size_t buf_len, char buf[]);
static expr_t parse_funccall(parse_context_t *ctx, const char **in);
static void parse_arglist(parse_context_t *ctx, const char **in,
		expr_t *call_expr);
static void skip_whitespace_tokens(parse_context_t *ctx, const char **in);
static void get_next(parse_context_t *ctx, const char **in);

static int initialized;
static getenv_func getenv_fu;

/* Empty expression to be returned on errors. */
static expr_t null_expr;

/* Public interface --------------------------------------------------------- */

void
vle_parser_init(getenv_func getenv_f)
{
	getenv_fu = getenv_f;
	initialized = 1;
}

parsing_result_t
vle_parser_eval(const char input[], int interactive)
{
	return parse_from(input, &parse_or_expr, /*strict=*/0, interactive);
}

parsing_result_t
vle_parser_eval_call(const char input[])
{
	/* Unlike in Vim, don't execute call expression followed by trailing
	 * characters. */
	return parse_from(input, &parse_funccall, /*strict=*/1, /*interactive=*/1);
}

/* Performs parsing and evaluation.  Accepts top-level production.  Non-strict
 * parsing means evaluation of an expression followed by trailing characters.
 * Returns structure describing the outcome.  Field value of the result should
 * be freed by the caller. */
static
parsing_result_t parse_from(const char input[],
		expr_t (*production)(parse_context_t *ctx, const char **in),
		int strict, int interactive)
{
	assert(initialized && "Parser must be initialized before use.");

	parsing_result_t result = {};

	parse_context_t ctx = {
		.interactive = interactive,

		.last_error = PE_NO_ERROR,
		.last_token.type = BEGIN,
		.last_position = input,
	};

	get_next(&ctx, &ctx.last_position);
	expr_t expr_root = production(&ctx, &ctx.last_position);
	result.last_parsed_char = ctx.last_position;

	result.value = var_error();

	if(ctx.last_token.type != END)
	{
		if(result.last_parsed_char > input)
		{
			--result.last_parsed_char;
		}
		if(ctx.last_error == PE_NO_ERROR)
		{
			if(ctx.last_token.type == DQ && strchr(ctx.last_position, '"') == NULL)
			{
				/* This is a comment, just ignore it. */
				ctx.last_position += strlen(ctx.last_position);
			}
			else if(!strict && eval_expr(&ctx, &expr_root) == 0)
			{
				result.value = var_clone(expr_root.value);
				ctx.last_error = PE_INVALID_EXPRESSION;
			}
			else if(strict)
			{
				ctx.last_error = PE_INVALID_EXPRESSION;
			}
		}
	}

	if(ctx.last_error == PE_NO_ERROR)
	{
		if(eval_expr(&ctx, &expr_root) == 0)
		{
			result.value = var_clone(expr_root.value);
		}
	}

	if(ctx.last_error == PE_INVALID_EXPRESSION)
	{
		ctx.last_position = skip_whitespace(input);
	}

	free_expr(&expr_root);

	result.ends_with_whitespace = (ctx.prev_token.type == WHITESPACE);
	result.last_position = ctx.last_position;
	result.error = ctx.last_error;

	return result;
}

/* Expression evaluation ---------------------------------------------------- */

/* Evaluates values of an expression.  Returns zero on success, which means that
 * expr->value is now correct, otherwise non-zero is returned. */
static int
eval_expr(parse_context_t *ctx, expr_t *expr)
{
	int result = 1;
	switch(expr->op_type)
	{
		case OP_NONE:
			/* Do nothing, value is already available. */
			return 0;
		case OP_OR:
			result = eval_or_op(ctx, expr->nops, expr->ops, &expr->value);
			break;
		case OP_AND:
			result = eval_and_op(ctx, expr->nops, expr->ops, &expr->value);
			break;
		case OP_CALL:
			assert(expr->func != NULL && "Function must have a name.");
			result = eval_call_op(ctx, expr->func, expr->nops, expr->ops,
					&expr->value);
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
eval_or_op(parse_context_t *ctx, int nops, expr_t ops[], var_t *result)
{
	int val;
	int i;

	if(nops == 0)
	{
		*result = var_true();
		return 0;
	}

	if(eval_expr(ctx, &ops[0]) != 0)
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
	val = var_to_int(ops[0].value);

	for(i = 1; i < nops && !val; ++i)
	{
		if(eval_expr(ctx, &ops[i]) != 0)
		{
			return 1;
		}
		val |= var_to_int(ops[i].value);
	}

	*result = var_from_bool(val);
	return 0;
}

/* Evaluates logical AND operation.  All operands are evaluated lazily from left
 * to right.  Returns zero on success, otherwise non-zero is returned. */
static int
eval_and_op(parse_context_t *ctx, int nops, expr_t ops[], var_t *result)
{
	int val;
	int i;

	if(nops == 0)
	{
		*result = var_false();
		return 0;
	}

	if(eval_expr(ctx, &ops[0]) != 0)
	{
		return 1;
	}

	if(nops == 1)
	{
		*result = var_clone(ops[0].value);
		return 0;
	}

	/* TODO: replace with var_to_bool() when it's OK to change semantics of
	 *       strings by themselves. */
	val = (var_to_int(ops[0].value) != 0);

	for(i = 1; i < nops && val; ++i)
	{
		if(eval_expr(ctx, &ops[i]) != 0)
		{
			return 1;
		}
		/* TODO: replace with var_to_bool() when it's OK to change semantics of
		 *       strings by themselves. */
		val &= (var_to_int(ops[i].value) != 0);
	}

	*result = var_from_bool(val);
	return 0;
}

/* Evaluates invocation operation.  All operands are evaluated beforehand.
 * Returns zero on success, otherwise non-zero is returned. */
static int
eval_call_op(parse_context_t *ctx, const char name[], int nops, expr_t ops[],
		var_t *result)
{
	int i;

	for(i = 0; i < nops; ++i)
	{
		if(eval_expr(ctx, &ops[i]) != 0)
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
		*result = eval_concat(ctx, nops, ops);
	}
	else if(strcmp(name, "!") == 0)
	{
		assert(nops == 1 && "Must be single argument.");
		*result = var_from_bool(!var_to_int(ops[0].value));
	}
	else if(strcmp(name, "-") == 0 || strcmp(name, "+") == 0)
	{
		if(nops == 1)
		{
			const int val = var_to_int(ops[0].value);
			*result = var_from_int(name[0] == '-' ? -val : val);
		}
		else
		{
			assert(nops == 2 && "Must be two arguments.");
			const int a = var_to_int(ops[0].value);
			const int b = var_to_int(ops[1].value);
			*result = var_from_int(name[0] == '-' ? a - b : a + b);
		}
	}
	else
	{
		int i;
		call_info_t call_info;
		function_call_info_init(&call_info, ctx->interactive);

		for(i = 0; i < nops; ++i)
		{
			function_call_info_add_arg(&call_info, var_clone(ops[i].value));
		}

		*result = function_call(name, &call_info);
		if(result->type == VTYPE_ERROR)
		{
			ctx->last_error = PE_INVALID_EXPRESSION;
			var_free(*result);
			*result = var_false();
		}
		function_call_info_free(&call_info);
	}

	return (ctx->last_error != PE_NO_ERROR);
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
		const int lhs_int = var_to_int(lhs);
		const int rhs_int = var_to_int(rhs);
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
eval_concat(parse_context_t *ctx, int nops, expr_t ops[])
{
	char res[CMD_LINE_LENGTH_MAX + 1];
	size_t res_len = 0U;
	int i;

	assert(nops > 0 && "Must be at least one argument.");

	if(nops == 1)
	{
		return var_clone(ops[0].value);
	}

	res[0] = '\0';

	for(i = 0; i < nops; ++i)
	{
		char *const str_val = var_to_str(ops[i].value);
		if(str_val == NULL)
		{
			ctx->last_error = PE_INTERNAL;
			break;
		}

		copy_str(res + res_len, sizeof(res) - res_len, str_val);
		res_len += strlen(res + res_len);
		free(str_val);
	}

	return (ctx->last_error == PE_NO_ERROR ? var_from_str(res) : var_error());
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
parse_or_expr(parse_context_t *ctx, const char **in)
{
	expr_t result = { .op_type = OP_OR };

	while(ctx->last_error == PE_NO_ERROR)
	{
		const expr_t op = parse_and_expr(ctx, in);
		if(ctx->last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			break;
		}

		if(ctx->last_token.type != OR)
		{
			/* Return partial result. */
			break;
		}

		get_next(ctx, in);
	}

	if(ctx->last_error == PE_INTERNAL)
	{
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* and_expr ::= comp_expr | comp_expr '&&' and_expr */
static expr_t
parse_and_expr(parse_context_t *ctx, const char **in)
{
	expr_t result = { .op_type = OP_AND };

	while(ctx->last_error == PE_NO_ERROR)
	{
		const expr_t op = parse_comp_expr(ctx, in);
		if(ctx->last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			break;
		}

		if(ctx->last_token.type != AND)
		{
			/* Return partial result. */
			break;
		}

		get_next(ctx, in);
	}

	if(ctx->last_error == PE_INTERNAL)
	{
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* comp_expr ::= factor | factor op factor
 * op ::= '==' | '!=' | '<' | '<=' | '>' | '>=' */
static expr_t
parse_comp_expr(parse_context_t *ctx, const char **in)
{
	expr_t lhs;
	expr_t rhs;
	expr_t result = { .op_type = OP_CALL };

	lhs = parse_factor(ctx, in);
	if(ctx->last_error != PE_NO_ERROR ||
			!is_comparison_operator(ctx->last_token.type))
	{
		return lhs;
	}

	result.func = strdup(ctx->last_token.str);

	if(add_expr_op(&result, &lhs) != 0 || result.func == NULL)
	{
		free_expr(&result);
		ctx->last_error = PE_INTERNAL;
		return null_expr;
	}

	get_next(ctx, in);
	rhs = parse_factor(ctx, in);
	if(add_expr_op(&result, &rhs) != 0)
	{
		free_expr(&result);
		ctx->last_error = PE_INTERNAL;
		return null_expr;
	}

	if(ctx->last_error != PE_NO_ERROR)
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

/* factor ::= concat_expr { op concat_expr }
 * op ::= '+' | '-' */
static expr_t
parse_factor(parse_context_t *ctx, const char **in)
{
	expr_t result = parse_concat_expr(ctx, in);

	while(ctx->last_error == PE_NO_ERROR &&
			(ctx->last_token.type == PLUS || ctx->last_token.type == MINUS))
	{
		expr_t intermediate = { .op_type = OP_CALL };
		expr_t next;

		intermediate.func = strdup(ctx->last_token.str);
		if(add_expr_op(&intermediate, &result) != 0 || intermediate.func == NULL)
		{
			ctx->last_error = PE_INTERNAL;
			free_expr(&intermediate);
			return null_expr;
		}

		get_next(ctx, in);
		next = parse_concat_expr(ctx, in);
		if(ctx->last_error != PE_NO_ERROR)
		{
			free_expr(&next);
			free_expr(&intermediate);
			return null_expr;
		}

		if(add_expr_op(&intermediate, &next) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			free_expr(&intermediate);
			return null_expr;
		}

		result = intermediate;
	}

	if(ctx->last_error == PE_INTERNAL)
	{
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* concat_expr ::= term { '.' term } */
static expr_t
parse_concat_expr(parse_context_t *ctx, const char **in)
{
	expr_t result = { .op_type = OP_CALL };

	result.func = strdup(".");
	if(result.func == NULL)
	{
		ctx->last_error = PE_INTERNAL;
	}

	while(ctx->last_error == PE_NO_ERROR)
	{
		expr_t op;

		skip_whitespace_tokens(ctx, in);
		op = parse_term(ctx, in);
		skip_whitespace_tokens(ctx, in);

		if(ctx->last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(&result, &op) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			break;
		}

		if(ctx->last_token.type != DOT)
		{
			/* Return partial result. */
			break;
		}

		get_next(ctx, in);
	}

	if(ctx->last_error == PE_INTERNAL)
	{
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* term ::= signed_number | number | sqstr | dqstr | envvar | builtinvar |
 *          funccall | opt | logical_not | '(' or_expr ')' */
static expr_t
parse_term(parse_context_t *ctx, const char **in)
{
	expr_t result = { .op_type = OP_NONE };
	const char *old_in = *in - 1;

	switch(ctx->last_token.type)
	{
		case MINUS:
		case PLUS:
			result = parse_signed_number(ctx, in);
			break;
		case DIGIT:
			result.value = parse_number(ctx, in);
			break;
		case SQ:
			get_next(ctx, in);
			result.value = parse_singly_quoted_string(ctx, in);
			break;
		case DQ:
			get_next(ctx, in);
			result.value = parse_doubly_quoted_string(ctx, in);
			break;
		case DOLLAR:
			get_next(ctx, in);
			result.value = eval_envvar(ctx, in);
			break;
		case AMPERSAND:
			get_next(ctx, in);
			result.value = eval_opt(ctx, in);
			break;
		case EMARK:
			get_next(ctx, in);
			result = parse_logical_not(ctx, in);
			break;
		case LPAREN:
			get_next(ctx, in);
			result = parse_or_expr(ctx, in);
			if(ctx->last_token.type == RPAREN)
			{
				get_next(ctx, in);
			}
			else
			{
				free_expr(&result);
				result = null_expr;
				result.value = var_error();
				ctx->last_error = PE_MISSING_PAREN;
				ctx->last_position = old_in;
			}
			break;

		case SYM:
			if(char_is_one_of("abcdefghijklmnopqrstuvwxyz_",
						tolower(ctx->last_token.c)))
			{
				if(**in == ':')
				{
					result.value = eval_builtinvar(ctx, in);
				}
				else
				{
					result = parse_funccall(ctx, in);
				}
				break;
			}
			/* break is omitted intentionally. */

		default:
			--*in;
			ctx->last_error = PE_INVALID_EXPRESSION;
			result.value = var_error();
			break;
	}
	return result;
}

/* signed_number ::= ( + | - ) { + | - } term */
static expr_t
parse_signed_number(parse_context_t *ctx, const char **in)
{
	const int sign = (ctx->last_token.type == MINUS) ? -1 : 1;
	expr_t result = { .op_type = OP_CALL };
	expr_t op;

	get_next(ctx, in);
	skip_whitespace_tokens(ctx, in);

	op = parse_term(ctx, in);
	if(ctx->last_error != PE_NO_ERROR)
	{
		free_expr(&op);
		return null_expr;
	}

	result.func = strdup((sign == 1) ? "+" : "-");
	if(add_expr_op(&result, &op) != 0 || result.func == NULL)
	{
		free_expr(&result);
		ctx->last_error = PE_INTERNAL;
		return null_expr;
	}

	return result;
}

/* number ::= num { num } */
static var_t
parse_number(parse_context_t *ctx, const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX];
	size_t len = 0U;
	buffer[0] = '\0';

	do
	{
		if(sstrappendch(buffer, &len, sizeof(buffer), ctx->last_token.c) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			return var_false();
		}
		get_next(ctx, in);
	}
	while(ctx->last_token.type == DIGIT);

	return var_from_int(str_to_int(buffer));
}

/* sqstr ::= ''' sqchar { sqchar } ''' */
static var_t
parse_singly_quoted_string(parse_context_t *ctx, const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX + 1];
	const char *old_in = *in - 2;
	sbuffer sbuf = { .data = buffer, .size = sizeof(buffer) };
	buffer[0] = '\0';
	while(parse_singly_quoted_char(ctx, in, &sbuf));

	if(ctx->last_error != PE_NO_ERROR)
	{
		return var_false();
	}

	if(ctx->last_token.type == SQ)
	{
		get_next(ctx, in);
		return var_from_str(buffer);
	}

	ctx->last_error = PE_MISSING_QUOTE;
	ctx->last_position = old_in;
	return var_false();
}

/* sqchar
 * Returns non-zero if there are more characters in the string. */
static int
parse_singly_quoted_char(parse_context_t *ctx, const char **in, sbuffer *sbuf)
{
	int double_sq;
	int sq_char;

	double_sq = (ctx->last_token.type == SQ && **in == '\'');
	sq_char = ctx->last_token.type != SQ && ctx->last_token.type != END;
	if(!sq_char && !double_sq)
	{
		return 0;
	}

	if(sstrappend(sbuf->data, &sbuf->len, sbuf->size, ctx->last_token.str) != 0)
	{
		ctx->last_error = PE_INTERNAL;
		return 0;
	}
	get_next(ctx, in);

	if(double_sq)
	{
		get_next(ctx, in);
	}
	return 1;
}

/* dqstr ::= ''' dqchar { dqchar } ''' */
static var_t
parse_doubly_quoted_string(parse_context_t *ctx, const char **in)
{
	char buffer[CMD_LINE_LENGTH_MAX + 1];
	const char *old_in = *in - 2;
	sbuffer sbuf = { .data = buffer, .size = sizeof(buffer) };
	buffer[0] = '\0';
	while(parse_doubly_quoted_char(ctx, in, &sbuf));

	if(ctx->last_error != PE_NO_ERROR)
	{
		return var_false();
	}

	if(ctx->last_token.type == DQ)
	{
		get_next(ctx, in);
		return var_from_str(buffer);
	}

	ctx->last_error = PE_MISSING_QUOTE;
	ctx->last_position = old_in;
	return var_false();
}

/* dqchar
 * Returns non-zero if there are more characters in the string. */
int
parse_doubly_quoted_char(parse_context_t *ctx, const char **in, sbuffer *sbuf)
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

	int ok;

	if(ctx->last_token.type == DQ || ctx->last_token.type == END)
	{
		return 0;
	}

	if(ctx->last_token.c == '\\')
	{
		get_next(ctx, in);
		if(ctx->last_token.type == END)
		{
			ctx->last_error = PE_INVALID_EXPRESSION;
			return 0;
		}
		ok = sstrappendch(sbuf->data, &sbuf->len, sbuf->size,
				table[(int)ctx->last_token.c]);
	}
	else
	{
		ok = sstrappend(sbuf->data, &sbuf->len, sbuf->size, ctx->last_token.str);
	}

	if(ok != 0)
	{
		ctx->last_error = PE_INTERNAL;
		return 0;
	}

	get_next(ctx, in);
	return 1;
}

/* envvar ::= '$' envvarname */
static var_t
eval_envvar(parse_context_t *ctx, const char **in)
{
	char name[VAR_NAME_LENGTH_MAX + 1];
	if(!parse_sequence(ctx, in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
		sizeof(name), name))
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	return var_from_str(getenv_fu(name));
}

/* builtinvar ::= 'v:' varname */
static var_t
eval_builtinvar(parse_context_t *ctx, const char **in)
{
	var_t var_value;
	char name[VAR_NAME_LENGTH_MAX + 1];
	strcpy(name, "v:");

	if(ctx->last_token.c != 'v' || **in != ':')
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	get_next(ctx, in);
	get_next(ctx, in);

	/* XXX: re-using environment variable constants, but could make new ones. */
	if(!parse_sequence(ctx, in, ENV_VAR_NAME_FIRST_CHAR, ENV_VAR_NAME_CHARS,
				sizeof(name) - 2U, &name[2]))
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	var_value = getvar(name);
	if(var_value.type == VTYPE_ERROR)
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	return var_clone(var_value);
}

/* envvar ::= '&' [ 'l:' | 'g:' ] optname */
static var_t
eval_opt(parse_context_t *ctx, const char **in)
{
	OPT_SCOPE scope = OPT_ANY;
	const opt_t *option;

	char name[OPTION_NAME_MAX + 1];

	if((ctx->last_token.c == 'l' || ctx->last_token.c == 'g') && **in == ':')
	{
		scope = (ctx->last_token.c == 'l') ? OPT_LOCAL : OPT_GLOBAL;
		get_next(ctx, in);
		get_next(ctx, in);
	}

	if(!parse_sequence(ctx, in, OPT_NAME_FIRST_CHAR, OPT_NAME_CHARS, sizeof(name),
		name))
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	option = vle_opts_find(name, scope);
	if(option == NULL)
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return var_false();
	}

	switch(option->type)
	{
		case OPT_STR:
		case OPT_STRLIST:
		case OPT_CHARSET:
			return var_from_str(option->val.str_val);

		case OPT_BOOL:
			return var_from_bool(option->val.bool_val);

		case OPT_INT:
			return var_from_int(option->val.int_val);

		case OPT_ENUM:
		case OPT_SET:
			return var_from_str(vle_opt_to_string(option));

		default:
			assert(0 && "Unexpected option type");
			return var_false();
	}
}

/* logical_not ::= '!' term */
static expr_t
parse_logical_not(parse_context_t *ctx, const char **in)
{
	expr_t result = { .op_type = OP_CALL };
	expr_t op;

	skip_whitespace_tokens(ctx, in);

	op = parse_term(ctx, in);
	if(ctx->last_error != PE_NO_ERROR)
	{
		free_expr(&op);
		return null_expr;
	}

	if(add_expr_op(&result, &op) != 0)
	{
		ctx->last_error = PE_INTERNAL;
		return null_expr;
	}

	result.func = strdup("!");
	if(result.func == NULL)
	{
		ctx->last_error = PE_INTERNAL;
		free_expr(&result);
		return null_expr;
	}

	return result;
}

/* sequence ::= first { other }
 * Returns zero on failure, otherwise non-zero is returned. */
static int
parse_sequence(parse_context_t *ctx, const char **in, const char first[],
		const char other[], size_t buf_len, char buf[])
{
	if(buf_len == 0UL || !char_is_one_of(first, ctx->last_token.c))
	{
		return 0;
	}

	buf[0] = '\0';

	do
	{
		strcatch(buf, ctx->last_token.c);
		get_next(ctx, in);
	}
	while(--buf_len > 1UL && char_is_one_of(other, ctx->last_token.c));

	return 1;
}

/* funccall ::= varname '(' [arglist] ')' */
static expr_t
parse_funccall(parse_context_t *ctx, const char **in)
{
	char *name;
	size_t name_len;
	expr_t result = { .op_type = OP_CALL };

	if(!isalpha(ctx->last_token.c))
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
		return null_expr;
	}

	name = strdup(ctx->last_token.str);
	name_len = strlen(name);
	get_next(ctx, in);
	while(ctx->last_token.type == SYM && isalnum(ctx->last_token.c))
	{
		if(strappendch(&name, &name_len, ctx->last_token.c) != 0)
		{
			free(name);
			ctx->last_error = PE_INTERNAL;
			return null_expr;
		}
		get_next(ctx, in);
	}

	if(ctx->last_token.type != LPAREN || !function_registered(name))
	{
		free(name);
		ctx->last_error = PE_INVALID_EXPRESSION;
		return null_expr;
	}

	result.func = name;

	get_next(ctx, in);
	skip_whitespace_tokens(ctx, in);

	/* If argument list is not empty. */
	if(ctx->last_token.type != RPAREN)
	{
		const char *old_in = *in - 1;
		parse_arglist(ctx, in, &result);
		if(ctx->last_error != PE_NO_ERROR)
		{
			*in = old_in;
			free_expr(&result);
			return null_expr;
		}
	}

	skip_whitespace_tokens(ctx, in);
	if(ctx->last_token.type != RPAREN)
	{
		ctx->last_error = PE_INVALID_EXPRESSION;
	}

	get_next(ctx, in);
	skip_whitespace_tokens(ctx, in);

	return result;
}

/* arglist ::= or_expr { ',' or_expr } */
static void
parse_arglist(parse_context_t *ctx, const char **in, expr_t *call_expr)
{
	do
	{
		const expr_t op = parse_or_expr(ctx, in);
		if(ctx->last_error != PE_NO_ERROR)
		{
			free_expr(&op);
			break;
		}

		if(add_expr_op(call_expr, &op) != 0)
		{
			ctx->last_error = PE_INTERNAL;
			break;
		}

		skip_whitespace_tokens(ctx, in);

		if(ctx->last_token.type != COMMA)
		{
			break;
		}
		get_next(ctx, in);
	}
	while(ctx->last_error == PE_NO_ERROR);

	if(ctx->last_error == PE_INVALID_EXPRESSION)
	{
		ctx->last_error = PE_INVALID_SUBEXPRESSION;
	}
}

/* Input tokenization ------------------------------------------------------- */

/* Skips series of consecutive whitespace. */
static void
skip_whitespace_tokens(parse_context_t *ctx, const char **in)
{
	while(ctx->last_token.type == WHITESPACE)
	{
		get_next(ctx, in);
	}
}

/* Gets next token from input.  Configures last_token variable. */
static void
get_next(parse_context_t *ctx, const char **in)
{
	const char *const start = *in;
	TOKENS_TYPE tt;

	if(ctx->last_token.type == END)
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
	ctx->prev_token = ctx->last_token;
	ctx->last_token.c = **in;
	ctx->last_token.type = tt;

	if(tt != END)
		++*in;

	strncpy(ctx->last_token.str, start, *in - start);
	ctx->last_token.str[*in - start] = '\0';
}

void
vle_parser_report(const parsing_result_t *result)
{
	switch(result->error)
	{
		case PE_NO_ERROR:
			/* Not an error. */
			break;
		case PE_INVALID_EXPRESSION:
			vle_tb_append_linef(vle_err, "%s: %s", "Invalid expression",
					result->last_position);
			break;
		case PE_INVALID_SUBEXPRESSION:
			vle_tb_append_linef(vle_err, "%s: %s", "Invalid subexpression",
					result->last_position);
			break;
		case PE_MISSING_QUOTE:
			vle_tb_append_linef(vle_err, "%s: %s",
					"Expression is missing closing quote", result->last_position);
			break;
		case PE_MISSING_PAREN:
			vle_tb_append_linef(vle_err, "%s: %s",
					"Expression is missing closing parenthesis", result->last_position);
			break;
		case PE_INTERNAL:
			vle_tb_append_line(vle_err, "Internal error");
			break;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
