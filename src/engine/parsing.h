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

/* A type of function that will be used to resolve environment variable
 * value. If variable doesn't exist the function should return an empty
 * string. The function should not allocate new string. */
typedef const char * (*getenv_func)(const char *envname);

/* A type of function that will be used to print error messages. */
typedef void (*print_error_func)(const char msg[]);

/* Can be called several times.  getenv_f can be NULL. */
void init_parser(getenv_func getenv_f);

/* Returns logical (e.g. beginning of wrong expression) position in a string,
 * where parser has stopped. */
const char * get_last_position(void);

/* Returns actual position in a string, where parser has stopped. */
const char * get_last_parsed_char(void);

/* Performs parsing.  After calling this function get_last_position() will
 * return useful information.  Returns error code and puts result of expression
 * evaluation in the result parameter. */
ParsingErrors parse(const char input[], int interactive, var_t *result);

/* Returns evaluation result, may be used to get value on error. */
var_t get_parsing_result(void);

/* Returns non-zero if previously read token was whitespace. */
int is_prev_token_whitespace(void);

/* Appends error message with details to the error stream. */
void report_parsing_error(ParsingErrors error);

#endif /* VIFM__ENGINE__PARSING_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
