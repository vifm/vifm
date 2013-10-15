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

#include "column_view.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* malloc() realloc() free() */
#include <string.h> /* memmove() memset() strlen() */
#include <wchar.h> /* wcswidth() */

#include "utils/macros.h"
#include "utils/str.h"
#include "utils/utf8.h"
/* For wcswidth() stub. */
#ifdef _WIN32
#include "utils/utils.h"
#endif

#define MAX_ELLIPSIS_DOT_COUNT 3
#define GAP_FILL_CHAR ' '

/* Holds general column information. */
typedef struct
{
	int column_id; /* Unique column id. */
	column_func func; /* Function, which prints column value. */
}
column_desc_t;

/* Column information including calculated values. */
typedef struct
{
	column_info_t info; /* Column properties specified by the client. */
	size_t start; /* Start position of the column. */
	size_t width; /* Calculated width of the column. */
	size_t print_width; /* Print width (less or equal to the width field). */
	column_func func; /* Cached column print function of column_desc_t. */
}
column_t;

/* Column view description structure.  Typedef is in the header file. */
struct columns_list_t
{
	size_t max_width; /* Maximum width of one line of the view. */
	size_t count; /* Number of columns in the list. */
	column_t *list; /* Array of columns of count length. */
};

static int extend_column_desc_list(void);
static void init_new_column_desc(column_desc_t *desc, int column_id,
		column_func func);
static int column_id_present(int column_id);
static int extend_column_list(columns_t cols);
static void init_new_column(column_t *col, column_info_t info);
static void mark_for_recalculation(columns_t cols);
static column_func get_column_func(int column_id);
static void decorate_output(const column_t *col, char buf[],
		size_t max_line_width);
static void add_ellipsis(AlignType align, char buf[]);
static size_t calculate_max_width(const column_t *col, size_t len,
		size_t max_line_width);
static size_t calculate_start_pos(const column_t *col, const char buf[]);
static void fill_gap_pos(const void *data, size_t from, size_t to);
static size_t get_width_on_screen(const char str[]);
static void recalculate_if_needed(columns_t cols, size_t max_width);
static int recalculation_is_needed(columns_t cols, size_t max_width);
static void recalculate(columns_t cols, size_t max_width);
static void update_widths(columns_t cols, size_t max_width);
static int update_abs_and_rel_widths(columns_t cols, size_t *max_width);
static void update_auto_widths(columns_t cols, size_t auto_count,
		size_t max_width);
static void update_start_positions(columns_t cols);

/* Number of registered column descriptors. */
static size_t col_desc_count;
/* List of column descriptors. */
static column_desc_t *col_descs;
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
		init_new_column_desc(&col_descs[col_desc_count - 1], column_id, func);
		return 0;
	}
	return 1;
}

/* Extends column descriptors list by one element, but doesn't initialize it at
 * all.  Returns zero on success. */
static int
extend_column_desc_list(void)
{
	const size_t new_size = (col_desc_count + 1)*sizeof(*col_descs);
	column_desc_t *const mem_ptr = realloc(col_descs, new_size);
	if(mem_ptr == NULL)
	{
		return 1;
	}
	col_descs = mem_ptr;
	col_desc_count++;
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
	struct columns_list_t *const result = malloc(sizeof(struct columns_list_t));
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
columns_free(columns_t cols)
{
	if(cols != NULL_COLUMNS)
	{
		columns_clear(cols);
		free(cols);
	}
}

void
columns_clear(columns_t cols)
{
	free(cols->list);
	cols->list = NULL;
	cols->count = 0;
}

void
columns_clear_column_descs(void)
{
	free(col_descs);
	col_descs = NULL;
	col_desc_count = 0;
}

void
columns_add_column(columns_t cols, column_info_t info)
{
	assert(info.text_width <= info.full_width &&
			"Text width should be bigger than full width.");
	assert(column_id_present(info.column_id) && "Unknown column id.");
	if(extend_column_list(cols) == 0)
	{
		init_new_column(&cols->list[cols->count - 1], info);
		mark_for_recalculation(cols);
	}
}

/* Checks whether a column with such column_id is already in the list of column
 * descriptors. Returns non-zero if yes. */
static int
column_id_present(int column_id)
{
	size_t i;
	/* Validate column_id. */
	for(i = 0; i < col_desc_count; i++)
	{
		if(col_descs[i].column_id == column_id)
		{
			return 1;
		}
	}
	return 0;
}

/* Extends columns list by one element, but doesn't initialize it at all.
 * Returns zero on success. */
static int
extend_column_list(columns_t cols)
{
	static column_t *mem_ptr;
	mem_ptr = realloc(cols->list, (cols->count + 1)*sizeof(column_t));
	if(mem_ptr == NULL)
	{
		return 1;
	}
	cols->list = mem_ptr;
	cols->count++;
	return 0;
}

/* Marks columns structure as one that need to be recalculated. */
static void
mark_for_recalculation(columns_t cols)
{
	cols->max_width = -1UL;
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

/* Returns a pointer to column formatting function by the column id or NULL on
 * unknown column_id. */
static column_func
get_column_func(int column_id)
{
	size_t i;
	for(i = 0; i < col_desc_count; i++)
	{
		column_desc_t *col_desc = &col_descs[i];
		if(col_desc->column_id == column_id)
		{
			return col_desc->func;
		}
	}
	assert(0 && "Unknown column id");
	return NULL;
}

void
columns_format_line(const columns_t cols, const void *data,
		size_t max_line_width)
{
	size_t i;
	size_t prev_col_end = 0;

	recalculate_if_needed(cols, max_line_width);

	for(i = 0; i < cols->count; i++)
	{
		/* Use big buffer to hold whole item so there will be no issues with right
		 * aligned fields. */
		char col_buffer[1024 + 1];
		size_t cur_col_start;
		const column_t *const col = &cols->list[i];

		col->func(col->info.column_id, data, ARRAY_LEN(col_buffer), col_buffer);
		decorate_output(col, col_buffer, max_line_width);
		cur_col_start = calculate_start_pos(col, col_buffer);

		fill_gap_pos(data, prev_col_end, cur_col_start);
		print_func(data, col->info.column_id, col_buffer, cur_col_start);

		prev_col_end = cur_col_start + get_width_on_screen(col_buffer);
	}

	fill_gap_pos(data, prev_col_end, max_line_width);
}

/* Adds decorations like ellipsis to the output. */
static void
decorate_output(const column_t *col, char buf[], size_t max_line_width)
{
	const size_t len = get_width_on_screen(buf);
	const size_t max_col_width = calculate_max_width(col, len, max_line_width);
	const int too_long = len > max_col_width;

	if(!too_long)
	{
		return;
	}

	if(col->info.align == AT_LEFT)
	{
		const size_t truncate_pos = get_real_string_width(buf, max_col_width);
		buf[truncate_pos] = '\0';
	}
	else
	{
		const size_t truncate_pos = get_real_string_width(buf, len - max_col_width);
		const char *const new_beginning = buf + truncate_pos;
		memmove(buf, new_beginning, strlen(new_beginning) + 1);
		assert(get_width_on_screen(buf) == max_col_width && "Column isn't filled.");
	}

	if(col->info.cropping == CT_ELLIPSIS)
	{
		add_ellipsis(col->info.align, buf);
	}
}

/* Adds ellipsis to the string in buf not changing its length (at most three
 * first or last characters are replaced). */
static void
add_ellipsis(AlignType align, char buf[])
{
	const size_t len = get_width_on_screen(buf);
	const size_t dot_count = MIN(len, MAX_ELLIPSIS_DOT_COUNT);
	if(align == AT_LEFT)
	{
		const size_t width_limit = len - dot_count;
		const size_t pos = get_real_string_width(buf, width_limit);
		memset(buf + pos, '.', dot_count);
		buf[pos + dot_count] = '\0';
	}
	else
	{
		const size_t beginning_shift = get_real_string_width(buf, dot_count);
		const char *const new_beginning = buf + beginning_shift;
		memmove(buf + dot_count, new_beginning, strlen(new_beginning) + 1);
		memset(buf, '.', dot_count);
	}
}

/* Calculates maximum width for outputting content of the col. */
static size_t
calculate_max_width(const column_t *col, size_t len, size_t max_line_width)
{
	if(col->info.cropping == CT_NONE)
	{
		const size_t left_bound = (col->info.align == AT_LEFT) ? col->start : 0;
		const size_t max_col_width = max_line_width - left_bound;
		return MIN(len, max_col_width);
	}
	else
	{
		return col->print_width;
	}
}

/* Calculates start position for outputting content of the col. */
static size_t
calculate_start_pos(const column_t *col, const char buf[])
{
	if(col->info.align == AT_LEFT)
	{
		return col->start;
	}
	else
	{
		const size_t end = col->start + col->width;
		const size_t len = get_width_on_screen(buf);
		return (end > len) ? (end - len) : 0;
	}
}

/* Prints gap filler (GAP_FILL_CHAR) in place of gaps.  Does nothing if to less
 * or equal to from. */
static void
fill_gap_pos(const void *data, size_t from, size_t to)
{
	if(to > from)
	{
		char gap[to - from + 1];
		memset(gap, GAP_FILL_CHAR, to - from);
		gap[to - from] = '\0';
		print_func(data, FILL_COLUMN_ID, gap, from);
	}
}

/* Returns number of character positions allocated by the string on the
 * screen.  On issues will try to do the best, to make visible at least
 * something. */
static size_t
get_width_on_screen(const char str[])
{
	size_t length = (size_t)-1;
	wchar_t *const wide = to_wide(str);
	if(wide != NULL)
	{
		length = wcswidth(wide, (size_t)-1);
		free(wide);
	}
	if(length == (size_t)-1)
	{
		length = get_normal_utf8_string_length(str);
	}
	return length;
}

/* Checks if recalculation is needed and runs it if yes. */
static void
recalculate_if_needed(columns_t cols, size_t max_width)
{
	if(recalculation_is_needed(cols, max_width))
	{
		recalculate(cols, max_width);
	}
}

/* Checks if recalculation is needed, and returns non-zero if so. */
static int
recalculation_is_needed(columns_t cols, size_t max_width)
{
	return cols->max_width != max_width;
}

/* Recalculates column widths and start offsets. */
static void
recalculate(columns_t cols, size_t max_width)
{
	update_widths(cols, max_width);
	update_start_positions(cols);

	cols->max_width = max_width;
}

/* Recalculates column widths. */
static void
update_widths(columns_t cols, size_t max_width)
{
	size_t width_left = max_width;
	const size_t auto_count = update_abs_and_rel_widths(cols, &width_left);
	update_auto_widths(cols, auto_count, width_left);
}

/* Recalculates widths of columns with absolute or percent widths.  Returns
 * number of columns with auto size type. */
static int
update_abs_and_rel_widths(columns_t cols, size_t *max_width)
{
	size_t i;
	size_t auto_count = 0;
	size_t width_left = *max_width;
	for(i = 0; i < cols->count; i++)
	{
		size_t effective_width;
		column_t *col = &cols->list[i];
		if(col->info.sizing == ST_ABSOLUTE)
		{
			effective_width = MIN(col->info.full_width, width_left);
		}
		else if(col->info.sizing == ST_PERCENT)
		{
			effective_width = MIN(col->info.full_width**max_width/100, width_left);
		}
		else
		{
			auto_count++;
			continue;
		}

		width_left -= effective_width;
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
	*max_width = width_left;
	return auto_count;
}

/* Recalculates widths of columns with automatic widths. */
static void
update_auto_widths(columns_t cols, size_t auto_count, size_t max_width)
{
	size_t i;
	size_t auto_left = auto_count;
	size_t left = max_width;
	for(i = 0; i < cols->count; i++)
	{
		column_t *col = &cols->list[i];
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
update_start_positions(columns_t cols)
{
	size_t i;
	for(i = 0; i < cols->count; i++)
	{
		if(i == 0)
		{
			cols->list[i].start = 0;
		}
		else
		{
			column_t *prev_col = &cols->list[i - 1];
			cols->list[i].start = prev_col->start + prev_col->width;
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
