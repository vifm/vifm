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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/str.h"

#include "parsing.h"

#define ENVVAR_NAME_LENGTH_MAX 1024
#define CMD_LINE_LENGTH_MAX 4096

/* Supported types of tokens. */
typedef enum
{
	BEGIN, SQ, DQ, DOT, DOLLAR, WHITESPACE, CHAR, END
}
TOKEN_TYPE;

static const int whitespace_allowed = 1;

static const char * eval_expression(const char **in);
static void skip_whitespace_tokens(const char **in);
static void eval_term(const char **in);
static void eval_single_quoted_string(const char **in);
static int eval_single_quoted_char(const char **in);
static void eval_double_quoted_string(const char **in);
static int eval_double_quoted_char(const char **in);
static void eval_envvar(const char **in);
static void get_next(const char **in);

/* This contains information about the last token read. */
struct
{
	TOKEN_TYPE type;
	char c;
}last_token;

static int initialized;
static getenv_func getenv_fu;
static char buffer[CMD_LINE_LENGTH_MAX];
static ParsingErrors last_error;
static const char *last_position;
static const char *last_parsed_char;

void
init_parser(getenv_func getenv_f)
{
	getenv_fu = getenv_f;
	initialized = 1;
}

ParsingErrors
get_parsing_error(void)
{
	assert(initialized);
	return last_error;
}

const char *
get_last_position(void)
{
	assert(initialized);
	return last_position;
}

const char *
get_last_parsed_char(void)
{
	assert(initialized);
	return last_parsed_char;
}

const char *
parse(const char *input)
{
	assert(initialized);

	last_error = PE_NO_ERROR;
	last_token.type = BEGIN;

	buffer[0] = '\0';
	last_position = input;
	get_next(&last_position);
	last_parsed_char = eval_expression(&last_position);

	return (last_error == PE_NO_ERROR) ? buffer : NULL;
}

const char *
get_parsing_result(void)
{
	assert(initialized);
	return buffer;
}

/* expr ::= term { '.' term }
 * Returns last position. */
static const char *
eval_expression(const char **in)
{
	const char *expr_begin = *in - 1;
	while(last_error == PE_NO_ERROR)
	{
		skip_whitespace_tokens(in);
		eval_term(in);
		skip_whitespace_tokens(in);
		if(last_token.type == WHITESPACE && !whitespace_allowed)
		{
			break;
		}
		else if(last_token.type != END && last_token.type != DOT)
		{
			--*in;
			last_error = PE_INVALID_EXPRESSION;
		}
		else if(last_token.type == DOT)
		{
			get_next(in);
		}
		else if(last_token.type == END)
		{
			break;
		}
	}

	if(last_error == PE_INVALID_EXPRESSION)
	{
		const char *last_pos = *in;
		*in = skip_whitespace(expr_begin);
		return last_pos;
	}
	return *in;
}

/* Skips series of consecutive whitespace. */
static void
skip_whitespace_tokens(const char **in)
{
	if(!whitespace_allowed)
		return;
	while(last_token.type == WHITESPACE)
		get_next(in);
}

/* term ::= sqstr | dqstr | envvar */
static void
eval_term(const char **in)
{
	switch(last_token.type)
	{
		case SQ:
			get_next(in);
			eval_single_quoted_string(in);
			break;
		case DQ:
			get_next(in);
			eval_double_quoted_string(in);
			break;
		case DOLLAR:
			get_next(in);
			eval_envvar(in);
			break;

		default:
			last_error = PE_INVALID_EXPRESSION;
			break;
	}
}

/* sqstr ::= ''' sqchar { sqchar } ''' */
static void
eval_single_quoted_string(const char **in)
{
	while(eval_single_quoted_char(in));

	if(last_token.type == END)
	{
		last_error = PE_MISSING_QUOTE;
	}
	else if(last_token.type == SQ)
	{
		get_next(in);
	}
}

/* sqchar
 * Returns non-zero if there are more characters in the string. */
static int
eval_single_quoted_char(const char **in)
{
	int double_sq;
	int sq_char;

	double_sq = (last_token.type == SQ && **in == '\'');
	sq_char = last_token.type != SQ && last_token.type != END;
	if(!sq_char && !double_sq)
		return 0;

	strcatch(buffer, last_token.c);
	get_next(in);

	if(double_sq)
		get_next(in);
	double_sq = (last_token.type == SQ && **in == '\'');
	return sq_char || double_sq;
}

/* dqstr ::= ''' dqchar { dqchar } ''' */
static void
eval_double_quoted_string(const char **in)
{
	while(eval_double_quoted_char(in));

	if(last_token.type == END)
	{
		last_error = PE_MISSING_QUOTE;
	}
	else if(last_token.type == DQ)
	{
		get_next(in);
	}
}

/* dqchar
 * Returns non-zero if there are more characters in the string. */
int
eval_double_quoted_char(const char **in)
{
	static const char table[] =
						/* 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
	/* 00 */	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	/* 10 */	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	/* 20 */	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	/* 30 */	"\x00\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	/* 40 */	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	/* 50 */	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	/* 60 */	"\x60\x07\x0b\x63\x64\x65\x0c\x67\x68\x69\x6a\x6b\x6c\x6d\x0a\x6f"
	/* 70 */	"\x70\x71\x0d\x73\x09\x75\x0b\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
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
static void
eval_envvar(const char **in)
{
	char name[ENVVAR_NAME_LENGTH_MAX];
	name[0] = '\0';
	while(last_token.type == CHAR)
	{
		strcatch(name, last_token.c);
		get_next(in);
	}
	strcat(buffer, getenv_fu(name));
}

/* Gets next token from input. Configures last_token global variable. */
static void
get_next(const char **in)
{
	TOKEN_TYPE tt;

	if(last_token.type == END)
		return;

	switch(**in)
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
		case ' ':
		case '\t':
			tt = WHITESPACE;
			break;
		case '\0':
			tt = END;
			break;

		default:
			tt = CHAR;
			break;
	}
	last_token.c = **in;
	last_token.type = tt;

	if(tt != END)
		++*in;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
