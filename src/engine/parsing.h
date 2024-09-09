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

#ifndef VIFM__ENGINE__PARSING_H__
#define VIFM__ENGINE__PARSING_H__

#include "var.h"

/* An enumeration of possible parsing errors. */
typedef enum
{
	PE_NO_ERROR,              /* No error occurred. */
	PE_INVALID_EXPRESSION,    /* Wrong expression construct. */
	PE_INVALID_SUBEXPRESSION, /* Wrong subexpression construct. */
	PE_MISSING_QUOTE,         /* Missing closing quote. */
	PE_MISSING_PAREN,         /* Missing closing paren. */
	PE_INTERNAL,              /* Internal error (e.g. not enough memory). */
}
ParsingErrors;

/* Describes result of parsing and evaluating an expression. */
typedef struct
{
	/* Result of evaluation. */
	var_t value;
	/* Actual position in a string, where parser has stopped. */
	const char *last_parsed_char;
	/* Logical (e.g. beginning of wrong expression) position in a string,
	 * where parser has stopped. */
	const char *last_position;
	/* Non-zero if the penultimate read token was whitespace. */
	char ends_with_whitespace;
	/* Error code. */
	ParsingErrors error;
}
parsing_result_t;

/* A type of function that will be used to resolve environment variable
 * value. If variable doesn't exist the function should return an empty
 * string. The function should not allocate new string. */
typedef const char * (*getenv_func)(const char *envname);

/* A type of function that will be used to print error messages. */
typedef void (*print_error_func)(const char msg[]);

/* Can be called several times.  getenv_f can be NULL. */
void vle_parser_init(getenv_func getenv_f);

/* Performs parsing and evaluation.  Returns structure describing the outcome.
 * Field value of the result should be freed by the caller. */
parsing_result_t vle_parser_eval(const char input[], int interactive);

/* Same as vle_parser_eval(), but uses call expression as top-level
 * production. */
parsing_result_t vle_parser_eval_call(const char input[]);

/* Appends error message with details to the error stream. */
void vle_parser_report(const parsing_result_t *result);

#endif /* VIFM__ENGINE__PARSING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
