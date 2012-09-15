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

#ifndef __PARSING_H__
#define __PARSING_H__

/* An enumeration of possible parsing errors. */
typedef enum
{
	PE_NO_ERROR,
	PE_INVALID_EXPRESSION,
	PE_MISSING_QUOTE,
}ParsingErrors;

/* A type of function that will be used to resolve environment variable
 * value. If variable doesn't exist the function should return an empty
 * string. The function should not allocate new string. */
typedef const char * (*getenv_func)(const char *envname);

/* Can be called several times. */
/* getenv_f can be NULL */
void init_parser(getenv_func getenv_f);

/* Returns the last error occurred during parsing. */
ParsingErrors get_parsing_error(void);

/* Returns logical (e.g. beginning of wrong expression) position in a string,
 * where parser has stopped. */
const char * get_last_position(void);

/* Returns actual position in a string, where parser has stopped. */
const char * get_last_parsed_char(void);

/* Performs parsing. After calling this function get_parsing_error() and
 * get_last_position() will return useful information.
 * Returns a pointer to a statically allocated buffer, that contains result of
 * expression evaluation or NULL on error. */
const char * parse(const char *input);

/* Returns evaluation result, may be to get value on error. */
const char * get_parsing_result(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
