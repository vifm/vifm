#ifndef __VIEWCOLUMNS_PARSER_H__
#define __VIEWCOLUMNS_PARSER_H__

#include "column_view.h"

/* Column add callback, the column argument is the same as in parse_columns. */
typedef void (*add_column_cb)(columns_t columns, column_info_t column_info);
/* Column name to column id mapping callback. Should return column id (a
 * positive number or zero) or a negative number. */
typedef int (*map_name_cb)(const char *name);

/* Parses str and calls cb function with columns as its parameter for each
 * column found. Returns non-zero when str is ill-formed. */
int parse_columns(columns_t columns, add_column_cb ac, map_name_cb cn,
		const char *str);

#endif /* __VIEWCOLUMNS_PARSER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
