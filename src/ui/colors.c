/* vifm
 * Copyright (C) 2026 xaizek.
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

#include "colors.h"

#include <curses.h> /* COLORS */

#include <ctype.h> /* isdigit() */
#include <stdlib.h> /* atoi() */
#include <stdio.h> /* sscanf() */
#include <string.h> /* strcmp() strcasecmp() strlen() strspn() */

#include "../utils/macros.h"
#include "../utils/string_array.h"
#include "../status.h"
#include "color_scheme.h"

static int is_def_color(const char str[]);

int
cols_parse_value(const char str[], int is_fg, int *attr)
{
	if(is_def_color(str))
	{
		return -1;
	}

	const int light_col_pos = string_array_pos_case(LIGHT_COLOR_NAMES,
			ARRAY_LEN(LIGHT_COLOR_NAMES), str);
	if(light_col_pos >= 0 && COLORS < 16)
	{
		*attr |= (!is_fg && curr_stats.exec_env_type == EET_LINUX_NATIVE) ?
				A_BLINK : A_BOLD;
		return light_col_pos;
	}

	const int col_pos = string_array_pos_case(XTERM256_COLOR_NAMES,
			ARRAY_LEN(XTERM256_COLOR_NAMES), str);
	if(col_pos >= 0)
	{
		if(!is_fg && curr_stats.exec_env_type == EET_LINUX_NATIVE)
		{
			*attr &= ~A_BLINK;
		}
		return col_pos;
	}

	const int col_num = isdigit(*str) ? atoi(str) : -1;
	if(col_num >= 0 && col_num < COLORS)
	{
		return col_num;
	}

	/* Fail if all possible parsing ways failed. */
	return -2;
}

int
cols_parse_gui_value(const char str[], int *color)
{
	const char *hex_digits = "0123456789abcdefABCDEF";

	if(is_def_color(str))
	{
		*color = -1;
		return 0;
	}

	*color = string_array_pos_case(XTERM256_COLOR_NAMES,
			ARRAY_LEN(XTERM256_COLOR_NAMES), str);
	if(*color >= 0 && *color < 8)
	{
		return 0;
	}

	if(str[0] != '#' || strlen(str) != 7 || strspn(str + 1, hex_digits) != 6)
	{
		return 1;
	}

	unsigned int value;
	if(sscanf(str, "#%x", &value) != 1)
	{
		return 1;
	}

	*color = value;
	return 0;
}

/* Checks whether a string signifies a default color.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_def_color(const char str[])
{
	return (strcmp(str, "-1") == 0)
	    || (strcasecmp(str, "default") == 0)
	    || (strcasecmp(str, "none") == 0);
}

int
cols_parse_attr(const char str[], int *attrs, int *combine_attrs)
{
#ifdef HAVE_A_ITALIC_DECL
	const int italic_attr = A_ITALIC;
#else
	/* If A_ITALIC is missing (it's an extension), use A_REVERSE instead. */
	const int italic_attr = A_REVERSE;
#endif

	if(strcasecmp(str, "bold") == 0)
		*attrs |= A_BOLD;
	else if(strcasecmp(str, "underline") == 0)
		*attrs |= A_UNDERLINE;
	else if(strcasecmp(str, "reverse") == 0)
		*attrs |= A_REVERSE;
	else if(strcasecmp(str, "inverse") == 0)
		*attrs |= A_REVERSE;
	else if(strcasecmp(str, "standout") == 0)
		*attrs |= A_STANDOUT;
	else if(strcasecmp(str, "italic") == 0)
		*attrs |= italic_attr;
	else if(strcasecmp(str, "none") == 0)
		*attrs = *combine_attrs = 0;
	else if(strcasecmp(str, "combine") == 0)
		*combine_attrs = 1;
	else
		return -1;

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
