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

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdlib.h> /* malloc() realloc() free() */
#include <string.h> /* memmove() memset() strlen() */

#include "utils/macros.h"
#include "utils/utf8.h"

#include "column_view.h"

/* Holds general column information. */
typedef struct
{
	int column_id;
	column_func func;
}
column_desc_t;

/* Column information including calculated values. */
typedef struct
{
	column_info_t info;
	size_t start;
	size_t width;
	size_t print_width;
	column_func func;
}
column_t;

/* Column view description structure. */
typedef struct cols_t
{
	size_t max_width;
	size_t count;
	column_t *list;
}
cols_t;

static int extend_column_desc_list(void);
static void init_new_column_desc(column_desc_t *desc, int column_id,
		column_func func);
static int column_id_present(int column_id);
static int extend_column_list(columns_t columns);
static void init_new_column(column_t *col, column_info_t info);
static void mark_for_recalculation(columns_t columns);
static column_func get_column_func(int column_id);
static void decorate_output(column_t *column, char *buf, size_t max_width);
static size_t calculate_max_width(column_t *column, size_t buf_max);
static size_t calculate_start_pos(column_t *column, const char *buf);
static void fill_gap_pos(const void *data, size_t from, size_t to);
static void recalculate_if_needed(columns_t columns, size_t max_width);
static int recalculation_is_needed(columns_t columns, size_t max_width);
static void recalculate(columns_t columns, size_t max_width);
static void update_widths(columns_t columns, size_t max_width);
static int update_abs_and_rel_widths(columns_t columns, size_t *max_width);
static void update_auto_widths(columns_t columns, size_t auto_count,
		size_t max_width);
static void update_start_positions(columns_t columns);

/* Number of registered column descriptors. */
static size_t column_desc_count;
/* List of column descriptors. */
static column_desc_t *column_descs;
/* Column print function. */
static column_line_print_func print_func;

void
columns_set_line_print_func(column_line_print_func func)
{
	print_func = func;
}

int
columns_add_column_desc(int column_id, column_func func)
{
	if(!column_id_present(column_id) && extend_column_desc_list() == 0)
	{
		init_new_column_desc(&column_descs[column_desc_count - 1], column_id, func);
		return 0;
	}
	return 1;
}

/* Extends column descriptors list by one element, but doesn't initialize it at
 * all. Returns zero on success. */
static int
extend_column_desc_list(void)
{
	static column_desc_t *mem_ptr;
	mem_ptr = realloc(column_descs,
			(column_desc_count + 1)*sizeof(column_desc_t));
	if(mem_ptr == NULL)
	{
		return 1;
	}
	column_descs = mem_ptr;
	column_desc_count++;
	return 0;
}

/* Fills column description structure with initial values. */
static void
init_new_column_desc(column_desc_t *desc, int column_id, column_func func)
{
	desc->column_id = column_id;
	desc->func = func;
}

columns_t
columns_create(void)
{
	cols_t *result = malloc(sizeof(cols_t));
	if(result == NULL)
	{
		return NULL_COLUMNS;
	}
	result->count = 0;
	result->list = NULL;
	mark_for_recalculation(result);
	return result;
}

void
columns_free(columns_t columns)
{
	if(columns != NULL_COLUMNS)
	{
		columns_clear(columns);
		free(columns);
	}
}

void
columns_clear(columns_t columns)
{
	columns->count = 0;
	free(columns->list);
	columns->list = NULL;
}

void
columns_clear_column_descs(void)
{
	column_desc_count = 0;
	free(column_descs);
	column_descs = NULL;
}

void
columns_add_column(columns_t columns, column_info_t info)
{
	assert(info.text_width <= info.full_width);
	assert(column_id_present(info.column_id));
	if(extend_column_list(columns) == 0)
	{
		init_new_column(&columns->list[columns->count - 1], info);
		mark_for_recalculation(columns);
	}
}

/* Checks whether a column with such column_id is already in the list of column
 * descriptors. Returns non-zero if yes. */
static int
column_id_present(int column_id)
{
	size_t i;
	/* validate column_id */
	for(i = 0; i < column_desc_count; i++)
	{
		if(column_descs[i].column_id == column_id)
		{
			return 1;
		}
	}
	return 0;
}

/* Extends columns list by one element, but doesn't initialize it at all.
 * Returns zero on success. */
static int
extend_column_list(columns_t columns)
{
	static column_t *mem_ptr;
	mem_ptr = realloc(columns->list, (columns->count + 1)*sizeof(column_t));
	if(mem_ptr == NULL)
	{
		return 1;
	}
	columns->list = mem_ptr;
	columns->count++;
	return 0;
}

/* Marks columns structure as one that need to be recalculated. */
static void
mark_for_recalculation(columns_t columns)
{
		columns->max_width = -1UL;
}

/* Fills column structure with initial values. */
static void
init_new_column(column_t *col, column_info_t info)
{
	col->info = info;
	col->start = -1UL;
	col->width = -1UL;
	col->print_width = -1UL;
	col->func = get_column_func(info.column_id);
}

/* Returns a pointer to column formatting function by the column id. */
static column_func
get_column_func(int column_id)
{
	size_t i;
	for(i = 0; i < column_desc_count; i++)
	{
		column_desc_t *col_desc = &column_descs[i];
		if(col_desc->column_id == column_id)
		{
			return col_desc->func;
		}
	}
	assert(0);
}

void
columns_format_line(const columns_t columns, const void *data, size_t max_width)
{
	size_t i;
	size_t end = 0;
	recalculate_if_needed(columns, max_width);
	for(i = 0; i < columns->count; i++)
	{
		/* Use big buffer to hold whole item so there will be no issues with right
		 * aligned fields. */
		char format_buffer[1024 + 1];
		size_t start;
		size_t max_field_width;
		column_t *col = &columns->list[i];
		col->func(col->info.column_id, data, ARRAY_LEN(format_buffer),
				format_buffer);
		max_field_width = max_width;
		max_field_width -= ((col->info.align == AT_LEFT) ? col->start : 0);
		decorate_output(col, format_buffer, max_field_width);
		start = calculate_start_pos(col, format_buffer);

		if(start > end)
		{
			fill_gap_pos(data, end, start);
		}
		print_func(data, col->info.column_id, format_buffer, start);
		end = start + get_normal_utf8_string_length(format_buffer);
	}
	if(max_width > end)
	{
		fill_gap_pos(data, end, max_width);
	}
}

/* Adds decorations like ellipsis to the output. */
static void
decorate_output(column_t *column, char *buf, size_t max_width)
{
	size_t len = get_normal_utf8_string_length(buf);
	size_t max_col_width = calculate_max_width(column, MIN(len, max_width));
	int truncated = len > max_col_width;
	if(truncated)
	{
		if(column->info.align == AT_LEFT)
		{
			size_t pos = get_real_string_width(buf, max_col_width);
			buf[pos] = '\0';
		}
		else
		{
			size_t pos = get_real_string_width(buf, len - max_col_width);
			memmove(buf, buf + pos, strlen(buf + pos) + 1);
			assert(get_normal_utf8_string_length(buf) == max_col_width);
		}

		if(column->info.cropping == CT_ELLIPSIS)
		{
			size_t truncated_len = get_normal_utf8_string_length(buf);
			size_t count = MIN(truncated_len, 3);
			if(column->info.align == AT_LEFT)
			{
				size_t pos = get_real_string_width(buf, truncated_len - count);
				buf[pos] = '\0';
				while(count-- > 0)
				{
					strcat(buf, ".");
				}
			}
			else
			{
				size_t diff = get_real_string_width(buf, count);
				memmove(buf + count, buf + diff, strlen(buf + diff) + 1);
				memset(buf, '.', count);
			}
		}
	}
}

/* Calculates maximum width for outputting content of the column. */
static size_t
calculate_max_width(column_t *column, size_t buf_max)
{
	if(column->info.cropping == CT_NONE)
	{
		return buf_max;
	}
	else
	{
		return column->print_width;
	}
}

/* Calculates start position for outputting content of the column. */
static size_t
calculate_start_pos(column_t *column, const char *buf)
{
	if(column->info.align == AT_LEFT)
	{
		return column->start;
	}
	else
	{
		size_t end = column->start + column->width;
		size_t len = get_normal_utf8_string_length(buf);
		return (end > len) ? (end - len) : 0;
	}
}

/* Prints spaces in place of gaps. */
static void
fill_gap_pos(const void *data, size_t from, size_t to)
{
	char spaces[to - from + 1];
	memset(spaces, ' ', to - from);
	spaces[to - from] = '\0';
	print_func(data, FILL_COLUMN_ID, spaces, from);
}

/* Checks if recalculation is needed and runs it if yes. */
static void
recalculate_if_needed(columns_t columns, size_t max_width)
{
	if(recalculation_is_needed(columns, max_width))
	{
		recalculate(columns, max_width);
	}
}

/* Checks if recalculation is needed. */
static int
recalculation_is_needed(columns_t columns, size_t max_width)
{
	return columns->max_width != max_width;
}

/* Recalculates column widths and start offsets. */
static void
recalculate(columns_t columns, size_t max_width)
{
	update_widths(columns, max_width);
	update_start_positions(columns);

	columns->max_width = max_width;
}

/* Recalculates column widths. */
static void
update_widths(columns_t columns, size_t max_width)
{
	size_t left = max_width;
	size_t auto_count = update_abs_and_rel_widths(columns, &left);
	update_auto_widths(columns, auto_count, left);
}

/* Recalculates widths of columns with absolute or percent widths. */
static int
update_abs_and_rel_widths(columns_t columns, size_t *max_width)
{
	size_t i;
	size_t auto_count = 0;
	size_t left = *max_width;
	for(i = 0; i < columns->count; i++)
	{
		size_t effective_width;
		column_t *col = &columns->list[i];
		if(col->info.sizing == ST_ABSOLUTE)
		{
			effective_width = MIN(col->info.full_width, left);
		}
		else if(col->info.sizing == ST_PERCENT)
		{
			effective_width = MIN(col->info.full_width**max_width/100, left);
		}
		else
		{
			auto_count++;
			continue;
		}

		left -= effective_width;
		col->width = effective_width;
		if(col->info.sizing == ST_ABSOLUTE)
		{
			col->print_width = MIN(effective_width, col->info.text_width);
		}
		else
		{
			col->print_width = col->width;
		}
	}
	*max_width = left;
	return auto_count;
}

/* Recalculates widths of columns with automatic widths. */
static void
update_auto_widths(columns_t columns, size_t auto_count, size_t max_width)
{
	size_t i;
	size_t auto_left = auto_count;
	size_t left = max_width;
	for(i = 0; i < columns->count; i++)
	{
		column_t *col = &columns->list[i];
		if(col->info.sizing == ST_AUTO)
		{
			auto_left--;

			col->width = (auto_left > 0) ? max_width/auto_count : left;
			col->print_width = col->width;

			left -= col->width;
		}
	}
}

/* Should be used after updating column widths. */
static void
update_start_positions(columns_t columns)
{
	size_t i;
	for(i = 0; i < columns->count; i++)
	{
		if(i == 0)
		{
			columns->list[i].start = 0;
		}
		else
		{
			column_t *prev_col = &columns->list[i - 1];
			columns->list[i].start = prev_col->start + prev_col->width;
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
