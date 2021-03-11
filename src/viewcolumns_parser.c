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

#include "viewcolumns_parser.h"

#include <ctype.h> /* isdigit() */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strdup() strtok_r() */

#include "compat/reallocarray.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/utf8.h"

static column_info_t * parse_all(map_name_cb cn, const char str[], size_t *len,
		void *arg);
static void free_list(column_info_t *list, size_t list_len);
static int parse(map_name_cb cn, const char str[], column_info_t *info,
		void *arg);
static void load_defaults(column_info_t *info);
static const char * parse_align(const char str[], column_info_t *info);
static const char * parse_width(const char str[], column_info_t *info,
		int *present);
static const char * parse_name(map_name_cb cn, const char str[],
		column_info_t *info, void *arg);
static const char * parse_cropping(const char str[], column_info_t *info,
		int *present);
static int extend_column_list(column_info_t **list, size_t *len);
static void add_all(columns_t *columns, add_column_cb ac,
		const column_info_t *list, size_t len);

int
parse_columns(columns_t *columns, add_column_cb ac, map_name_cb cn,
		const char str[], void *arg)
{
	column_info_t *list;
	size_t list_len;
	if((list = parse_all(cn, str, &list_len, arg)) != NULL)
	{
		add_all(columns, ac, list, list_len);
		free_list(list, list_len);
		return 0;
	}
	return 1;
}

/* Parses format string.  Returns list of size *len or NULL on error. */
static column_info_t *
parse_all(map_name_cb cn, const char str[], size_t *len, void *arg)
{
	char *saveptr;
	char *str_copy;
	char *token;
	column_info_t *list = NULL;
	size_t list_len = 0;
	size_t percents = 0;

	str_copy = strdup(str);
	if(str_copy == NULL)
	{
		return NULL;
	}

	saveptr = NULL;
	for(token = str_copy; (token = strtok_r(token, ",", &saveptr)); token = NULL)
	{
		column_info_t info;
		if(parse(cn, token, &info, arg) != 0)
		{
			break;
		}
		if(info.sizing == ST_PERCENT && (percents += info.full_width) > 100)
		{
			break;
		}
		if(extend_column_list(&list, &list_len) == 0)
		{
			list[list_len - 1] = info;
		}
	}
	free(str_copy);

	if(token != NULL)
	{
		free_list(list, list_len);
		return NULL;
	}
	else
	{
		*len = list_len;
		return list;
	}
}

/* Frees list of column info structures. */
static void
free_list(column_info_t *list, size_t list_len)
{
	if(list == NULL)
	{
		return;
	}

	size_t i;
	for(i = 0U; i < list_len; ++i)
	{
		free(list[i].literal);
	}
	free(list);
}

/* Parses single column description.  Returns zero on successful parsing. */
static int
parse(map_name_cb cn, const char str[], column_info_t *info, void *arg)
{
	load_defaults(info);

	int width_present = 0, cropping_present = 0;
	if((str = parse_align(str, info)) != NULL)
	{
		if((str = parse_width(str, info, &width_present)) != NULL)
		{
			if((str = parse_name(cn, str, info, arg)) != NULL)
			{
				str = parse_cropping(str, info, &cropping_present);
			}
		}
	}

	if(info->literal != NULL)
	{
		if(!width_present)
		{
			info->sizing = ST_ABSOLUTE;
			info->text_width = utf8_strsw(info->literal);
			info->full_width = info->text_width;
		}
		if(!cropping_present)
		{
			info->cropping = CT_TRUNCATE;
		}
	}

	return str == NULL || *str != '\0';
}

/* Initializes info structure with default values. */
static void
load_defaults(column_info_t *info)
{
	info->literal = NULL;
	info->column_id = FILL_COLUMN_ID;
	info->full_width = -1;
	info->text_width = -1;
	info->align = AT_RIGHT;
	info->sizing = ST_AUTO;
	info->cropping = CT_NONE;
}

/* Parses alignment type part of format string. Returns pointer to next char to
 * parse or NULL on error. */
static const char *
parse_align(const char str[], column_info_t *info)
{
	if(*str == '-')
	{
		info->align = AT_LEFT;
		++str;
	}
	else if(*str == '*')
	{
		info->align = AT_DYN;
		++str;
	}
	return str;
}

/* Parses width part of format string.  Always sets *present to indicate whether
 * width field was present.  Returns pointer to next char to parse or NULL on
 * error. */
static const char *
parse_width(const char str[], column_info_t *info, int *present)
{
	const char *orig = str;
	*present = 0;

	if(isdigit(*str))
	{
		char *end;
		info->full_width = strtol(str, &end, 10);
		info->text_width = info->full_width;
		str = end;
		info->sizing = ST_ABSOLUTE;

		if(*str == '%')
		{
			str++;
			if(info->full_width > 100)
			{
				return NULL;
			}
			info->sizing = ST_PERCENT;
		}
		else if(*str == '.')
		{
			char *end;
			if(!isdigit(*++str))
			{
				return NULL;
			}

			info->text_width = strtol(str, &end, 10);
			str = end;
		}
	}

	if(info->full_width < info->text_width ||
			info->full_width*info->text_width == 0)
	{
		return NULL;
	}

	*present = (str != orig);
	return str;
}

/* Parses name part of format string. Returns pointer to next char to parse or
 * NULL on error. */
static const char *
parse_name(map_name_cb cn, const char str[], column_info_t *info, void *arg)
{
	if(*str == '{')
	{
		const char *closing_brace = strchr(str + 1, '}');
		if(closing_brace != NULL)
		{
			char name[16];
			const size_t len = MIN(sizeof(name),
					(size_t)(closing_brace - (str + 1) + 1));
			(void)copy_str(name, len, str + 1);

			if(name[0] == '#')
			{
				info->literal = strdup(name + 1);
				return closing_brace + 1;
			}

			if((info->column_id = cn(name, arg)) >= 0)
			{
				return closing_brace + 1;
			}
		}
	}
	return NULL;
}

/* Parses cropping part of format string.  Always sets *present to indicate
 * whether cropping field was present.  RReturns pointer to next char to parse
 * or NULL on error. */
static const char *
parse_cropping(const char str[], column_info_t *info, int *present)
{
	int dot_count = strspn(str, ".");
	*present = (dot_count > 0);

	switch(dot_count)
	{
		case 1:
			info->cropping = CT_TRUNCATE;
			break;
		case 2:
			info->cropping = CT_ELLIPSIS;
			break;
		case 0:
		case 3:
			info->cropping = CT_NONE;
			break;

		default:
			return NULL;
	}
	return str + dot_count;
}

/* Extends columns list by one element, but doesn't initialize it at all.
 * Returns zero on success. */
static int
extend_column_list(column_info_t **list, size_t *len)
{
	static column_info_t *mem_ptr;
	mem_ptr = reallocarray(*list, *len + 1, sizeof(column_info_t));
	if(mem_ptr == NULL)
	{
		return 1;
	}
	*list = mem_ptr;
	++*len;
	return 0;
}

/* Adds all columns by calling ac callback for each column. */
static void
add_all(columns_t *columns, add_column_cb ac, const column_info_t *list,
		size_t len)
{
	size_t i;
	for(i = 0U; i < len; ++i)
	{
		ac(columns, list[i]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
