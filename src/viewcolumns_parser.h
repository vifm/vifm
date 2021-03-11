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

#ifndef VIFM__UI__VIEWCOLUMNS_PARSER_H__
#define VIFM__UI__VIEWCOLUMNS_PARSER_H__

#include "ui/column_view.h"

/* Column add callback, the column argument is the same as in parse_columns. */
typedef void (*add_column_cb)(columns_t *columns, column_info_t column_info);
/* Column name to column id mapping callback.  Should return column id (a
 * positive number or zero) or a negative number. */
typedef int (*map_name_cb)(const char name[], void *arg);

/* Parses str and calls cb function with columns as its parameter for each
 * column found.  Returns non-zero when str is ill-formed. */
int parse_columns(columns_t *columns, add_column_cb ac, map_name_cb cn,
		const char str[], void *arg);

#endif /* VIFM__UI__VIEWCOLUMNS_PARSER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
