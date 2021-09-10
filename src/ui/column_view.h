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

#ifndef VIFM__UI__COLUMN_VIEW_H__
#define VIFM__UI__COLUMN_VIEW_H__

#include <stddef.h> /* size_t */

/* Special reserved column id for gap filling request. */
#define FILL_COLUMN_ID ~0

/* Type of text alignment within a column. */
typedef enum
{
	AT_LEFT,  /* Left alignment. */
	AT_RIGHT, /* Right alignment. */
	AT_DYN    /* Left alignment, if the text fits if the text is longer, make sure
	             the end of field is always visible. */
}
AlignType;

/* Type of size units of a column. */
typedef enum
{
	ST_ABSOLUTE, /* Column width is specified in characters. */
	ST_PERCENT,  /* Column width is specified in percents of view width. */
	ST_AUTO      /* Column width is automatically determined. */
}
SizingType;

/* Type of text truncation if it doesn't fix in the column. */
typedef enum
{
	CT_TRUNCATE, /* Text is truncated. */
	CT_ELLIPSIS, /* Ellipsis are added. */
	CT_NONE      /* Text can pass column boundaries. */
}
CropType;

/* Opaque columns handle. */
typedef struct columns_t columns_t;

/* Structure containing column data passed to handlers. */
typedef struct format_info_t format_info_t;
struct format_info_t
{
	void *data; /* User data passed to columns_format_line(). */
	int id;     /* Id of the column. */
	int width;  /* Calculated width of the column. */
};

/* A column callback function, which should fill the buf with column text. */
typedef void (*column_func)(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

/* A callback function, for displaying column contents.  Alignment specifies
 * actual alignment of current column (AT_DYN won't appear here). */
typedef void (*column_line_print_func)(const char buf[], size_t offset,
		AlignType align, const char full_column[], const format_info_t *info);

/* Structure containing various column display properties. */
typedef struct
{
	char *literal;     /* Fixed contents of the column. */
	int column_id;     /* Unique id of existing column. */
	size_t full_width; /* Full width of the column, units depend on size type. */
	size_t text_width; /* Text width, ignored unless size type is ST_ABSOLUTE. */
	AlignType align;   /* Specifies type of text alignment. */
	SizingType sizing; /* Specifies type of sizing. */
	CropType cropping; /* Specifies type of text cropping. */
}
column_info_t;

/* Registers column print function. */
void columns_set_line_print_func(column_line_print_func func);

/* Sets string to be used in place of ellipsis.  The argument is used directly,
 * no copy is made. */
void columns_set_ellipsis(const char ell[]);

/* Registers column func by its unique column_id.
 * Returns zero on success and non-zero otherwise. */
int columns_add_column_desc(int column_id, column_func func, void *data);

/* Unregisters all column functions. */
void columns_clear_column_descs(void);

/* Creates column view.  Returns NULL on error. */
columns_t * columns_create(void);

/* Frees previously allocated columns.  Passing in NULL is OK. */
void columns_free(columns_t *cols);

/* Adds one column to the cols as the most right one. Duplicates are
 * allowed. */
void columns_add_column(columns_t *cols, column_info_t info);

/* Clears list of columns of the cols. */
void columns_clear(columns_t *cols);

/* Performs actual formatting of columns. */
void columns_format_line(columns_t *cols, void *format_data,
		size_t max_line_width);

/* Checks if recalculation is needed.  Returns non-zero if so, otherwise zero is
 * returned. */
int columns_matches_width(const columns_t *cols, size_t max_width);

#endif /* VIFM__UI__COLUMN_VIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
