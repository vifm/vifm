/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#include "fileview.h"

#include <curses.h>

#include <regex.h> /* regmatch_t regcomp() regexec() */

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* abs() */
#include <string.h> /* strcpy() strlen() */

#include "../cfg/config.h"
#include "../utils/fs.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/regexp.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../flist_pos.h"
#include "../opt_handlers.h"
#include "../sort.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "column_view.h"
#include "quickview.h"
#include "statusline.h"

/* Mark for a cursor position of inactive pane. */
#define INACTIVE_CURSOR_MARK "*"

/* Packet set of parameters to pass as user data for processing columns. */
typedef struct
{
	FileView *view;    /* View on which cell is being drawn. */
	size_t line_pos;   /* File position in the file list (the view). */
	int line_hi_group; /* Cached line highlight (avoid per-column calculation). */
	int is_current;    /* Whether this file is selected with the cursor. */

	size_t current_line;  /* Line of the cell. */
	size_t column_offset; /* Offset in characters of the column. */

	size_t *prefix_len; /* Data prefix length (should be drawn in neutral color).
	                     * A pointer to allow changing value in const struct.
	                     * Should be zero first time, then auto reset. */
}
column_data_t;

static void calculate_table_conf(FileView *view, size_t *count, size_t *width);
static void calculate_number_width(FileView *view);
static int count_digits(int num);
static int calculate_top_position(FileView *view, int top);
static int get_line_color(const FileView *view, int pos);
static size_t calculate_print_width(const FileView *view, int i,
		size_t max_width);
static void draw_cell(const FileView *view, const column_data_t *cdt,
		size_t col_width, size_t print_width);
static columns_t * get_view_columns(const FileView *view);
static void consider_scroll_bind(FileView *view);
static void put_inactive_mark(FileView *view);
static int prepare_inactive_color(FileView *view, dir_entry_t *entry,
		int line_color);
static void clear_current_line_bar(FileView *view, int is_current);
static size_t get_effective_scroll_offset(const FileView *view);
static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[]);
static void draw_line_number(const column_data_t *cdt, int column);
static void highlight_search(FileView *view, dir_entry_t *entry,
		const char full_column[], char buf[], size_t buf_len, AlignType align,
		int line, int col, int line_attrs);
static int prepare_col_color(const FileView *view, dir_entry_t *entry,
		int primary, int line_color, int current);
static void mix_in_file_hi(const FileView *view, dir_entry_t *entry,
		int type_hi, col_attr_t *col);
static void mix_in_file_name_hi(const FileView *view, dir_entry_t *entry,
		col_attr_t *col);
TSTATIC void format_name(int id, const void *data, size_t buf_len, char buf[]);
static void format_size(int id, const void *data, size_t buf_len, char buf[]);
static void format_nitems(int id, const void *data, size_t buf_len, char buf[]);
static void format_primary_group(int id, const void *data, size_t buf_len,
		char buf[]);
static void format_type(int id, const void *data, size_t buf_len, char buf[]);
static void format_target(int id, const void *data, size_t buf_len, char buf[]);
static void format_ext(int id, const void *data, size_t buf_len, char buf[]);
static void format_fileext(int id, const void *data, size_t buf_len,
		char buf[]);
static void format_time(int id, const void *data, size_t buf_len, char buf[]);
static void format_dir(int id, const void *data, size_t buf_len, char buf[]);
#ifndef _WIN32
static void format_group(int id, const void *data, size_t buf_len, char buf[]);
static void format_mode(int id, const void *data, size_t buf_len, char buf[]);
static void format_owner(int id, const void *data, size_t buf_len, char buf[]);
static void format_perms(int id, const void *data, size_t buf_len, char buf[]);
static void format_nlinks(int id, const void *data, size_t buf_len, char buf[]);
#endif
static void format_id(int id, const void *data, size_t buf_len, char buf[]);
static size_t calculate_column_width(FileView *view);
static size_t get_max_filename_width(const FileView *view);
static size_t get_filename_width(const FileView *view, int i);
static size_t get_filetype_decoration_width(const dir_entry_t *entry);
static int move_curr_line(FileView *view);
static void reset_view_columns(FileView *view);

void
fview_init(void)
{
	static const struct {
		SortingKey key;
		column_func func;
	} sort_to_func[] = {
		{ SK_BY_NAME,   &format_name },
		{ SK_BY_INAME,  &format_name },
		{ SK_BY_SIZE,   &format_size },
		{ SK_BY_NITEMS, &format_nitems },
		{ SK_BY_GROUPS, &format_primary_group },
		{ SK_BY_TYPE,   &format_type },
		{ SK_BY_TARGET, &format_target },

		{ SK_BY_EXTENSION,     &format_ext },
		{ SK_BY_FILEEXT,       &format_fileext },
		{ SK_BY_TIME_ACCESSED, &format_time },
		{ SK_BY_TIME_CHANGED,  &format_time },
		{ SK_BY_TIME_MODIFIED, &format_time },
		{ SK_BY_DIR,           &format_dir },

#ifndef _WIN32
		{ SK_BY_GROUP_ID,   &format_group },
		{ SK_BY_GROUP_NAME, &format_group },
		{ SK_BY_OWNER_ID,   &format_owner },
		{ SK_BY_OWNER_NAME, &format_owner },

		{ SK_BY_MODE, &format_mode },

		{ SK_BY_PERMISSIONS, &format_perms },

		{ SK_BY_NLINKS, &format_nlinks },
#endif
	};
	ARRAY_GUARD(sort_to_func, SK_COUNT);

	size_t i;

	columns_set_line_print_func(&column_line_print);
	for(i = 0U; i < ARRAY_LEN(sort_to_func); ++i)
	{
		columns_add_column_desc(sort_to_func[i].key, sort_to_func[i].func);
	}
	columns_add_column_desc(SK_BY_ID, &format_id);
}

void
fview_view_init(FileView *view)
{
	view->curr_line = 0;
	view->top_line = 0;

	view->local_cs = 0;

	view->columns = columns_create();
	view->view_columns = strdup("");
	view->view_columns_g = strdup("");

	view->sort_groups = strdup("");
	view->sort_groups_g = strdup("");
	(void)regcomp(&view->primary_group, view->sort_groups,
			REG_EXTENDED | REG_ICASE);
}

void
fview_view_reset(FileView *view)
{
	view->ls_view_g = view->ls_view = 0;
	/* Invalidate maximum file name widths cache. */
	view->max_filename_width = 0;;
	view->column_count = 1;

	view->num_type_g = view->num_type = NT_NONE;
	view->num_width_g = view->num_width = 4;
	view->real_num_width = 0;

	pthread_mutex_lock(view->timestamps_mutex);
	view->postponed_redraw = 0;
	view->postponed_reload = 0;
	pthread_mutex_unlock(view->timestamps_mutex);
}

void
fview_view_cs_reset(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].hi_num = -1;
	}
}

void
draw_dir_list(FileView *view)
{
	draw_dir_list_only(view);

	if(view != curr_view)
	{
		put_inactive_mark(view);
	}
}

void
draw_dir_list_only(FileView *view)
{
	int x;
	size_t cell;
	size_t col_width;
	size_t col_count;
	int coll_pad;
	int top = view->top_line;

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);

	if(top + view->window_rows > view->list_rows)
	{
		top = view->list_rows - view->window_rows;
	}
	if(top < 0)
	{
		top = 0;
	}

	ui_view_title_update(view);

	/* This is needed for reloading a list that has had files deleted. */
	while(view->list_rows - view->list_pos <= 0)
	{
		--view->list_pos;
		--view->curr_line;
	}

	top = calculate_top_position(view, top);

	ui_view_erase(view);

	cell = 0U;
	coll_pad = (!ui_view_displays_columns(view) && cfg.extra_padding) ? 1 : 0;
	for(x = top; x < view->list_rows; ++x)
	{
		size_t prefix_len = 0U;
		const column_data_t cdt = {
			.view = view,
			.line_pos = x,
			.line_hi_group = get_line_color(view, x),
			.is_current = (view == curr_view) ? x == view->list_pos : 0,
			.current_line = cell/col_count,
			.column_offset = (cell%col_count)*col_width,
			.prefix_len = &prefix_len,
		};

		const size_t print_width = calculate_print_width(view, x, col_width);

		draw_cell(view, &cdt, col_width - coll_pad, print_width);

		++cell;
		if(cell >= view->window_cells)
		{
			break;
		}
	}

	view->top_line = top;
	view->curr_line = view->list_pos - view->top_line;

	if(view == curr_view)
	{
		consider_scroll_bind(view);
	}

	ui_view_win_changed(view);
}

/* Calculates number of columns and maximum width of column in a view. */
static void
calculate_table_conf(FileView *view, size_t *count, size_t *width)
{
	calculate_number_width(view);

	if(ui_view_displays_columns(view))
	{
		*count = 1;
		*width = MAX(0, ui_view_available_width(view) - view->real_num_width);
	}
	else
	{
		*count = calculate_columns_count(view);
		*width = calculate_column_width(view);
	}

	view->column_count = *count;
	view->window_cells = *count*(view->window_rows + 1);
}

/* Calculates real number of characters that should be allocated in view for
 * numbers column. */
static void
calculate_number_width(FileView *view)
{
	if(ui_view_displays_numbers(view))
	{
		const int digit_count = count_digits(view->list_rows);
		const int min = view->num_width;
		const int max = ui_view_available_width(view);
		view->real_num_width = MIN(MAX(1 + digit_count, min), max);
	}
	else
	{
		view->real_num_width = 0;
	}
}

/* Counts number of digits in a number assuming that zero takes on digit.
 * Returns the count. */
static int
count_digits(int num)
{
	int count = 0;
	do
	{
		count++;
		num /= 10;
	}
	while(num != 0);
	return count;
}

/* Calculates top position basing on window and list size and trying to show as
 * much of the directory as possible.  Can modify view->curr_line.  Returns
 * new top. */
static int
calculate_top_position(FileView *view, int top)
{
	int result = MIN(MAX(top, 0), view->list_rows - 1);
	result = ROUND_DOWN(result, view->column_count);
	if((int)view->window_cells >= view->list_rows)
	{
		result = 0;
	}
	else if(view->list_rows - top < (int)view->window_cells)
	{
		if(view->window_cells - (view->list_rows - top) >= view->column_count)
		{
			result = view->list_rows - view->window_cells + (view->column_count - 1);
			result = ROUND_DOWN(result, view->column_count);
			view->curr_line++;
		}
	}
	return result;
}

/* Calculates highlight group for the line specified by its position.  Returns
 * highlight group number. */
static int
get_line_color(const FileView *view, int pos)
{
	switch(view->dir_entry[pos].type)
	{
		case FT_DIR:
			return DIRECTORY_COLOR;
		case FT_FIFO:
			return FIFO_COLOR;
		case FT_LINK:
			if(view->on_slow_fs)
			{
				return LINK_COLOR;
			}
			else
			{
				char full[PATH_MAX];
				get_full_path_at(view, pos, sizeof(full), full);
				if(get_link_target_abs(full, view->dir_entry[pos].origin, full,
							sizeof(full)) != 0)
				{
					return BROKEN_LINK_COLOR;
				}

				/* Assume that targets on slow file system are not broken as actual
				 * check might take long time. */
				if(is_on_slow_fs(full, cfg.slow_fs_list))
				{
					return LINK_COLOR;
				}

				return path_exists(full, DEREF) ? LINK_COLOR : BROKEN_LINK_COLOR;
			}
#ifndef _WIN32
		case FT_SOCK:
			return SOCKET_COLOR;
#endif
		case FT_CHAR_DEV:
		case FT_BLOCK_DEV:
			return DEVICE_COLOR;
		case FT_EXEC:
			return EXECUTABLE_COLOR;

		default:
			return WIN_COLOR;
	}
}

/* Calculates width of the column using entry and maximum width. */
static size_t
calculate_print_width(const FileView *view, int i, size_t max_width)
{
	if(!ui_view_displays_columns(view))
	{
		const size_t raw_name_width = get_filename_width(view, i);
		return MIN(max_width - 1, raw_name_width);
	}

	return max_width;
}

/* Draws a full cell of the file list.  print_width <= col_width. */
static void
draw_cell(const FileView *view, const column_data_t *cdt, size_t col_width,
		size_t print_width)
{
	if(cfg.extra_padding)
	{
		column_line_print(cdt, FILL_COLUMN_ID, " ", -1, AT_LEFT, " ");
	}

	columns_format_line(get_view_columns(view), cdt, col_width);

	if(cfg.extra_padding)
	{
		column_line_print(cdt, FILL_COLUMN_ID, " ", print_width, AT_LEFT, " ");
	}
}

/* Retrieves active view columns handle of the view considering 'lsview' option
 * status.  Returns the handle. */
static columns_t *
get_view_columns(const FileView *view)
{
	/* Note that columns_t performs some caching, so we might want to keep one
	 * handle per view rather than sharing one. */

	static const column_info_t name_column = {
		.column_id = SK_BY_NAME, .full_width = 0UL,    .text_width = 0UL,
		.align = AT_LEFT,        .sizing = ST_AUTO,    .cropping = CT_ELLIPSIS,
	};
	static const column_info_t id_column = {
		.column_id = SK_BY_ID,  .full_width = 7UL,     .text_width = 7UL,
		.align = AT_LEFT,       .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS,
	};

	static columns_t *comparison_columns;
	static columns_t *ls_columns;

	if(!ui_view_displays_columns(view))
	{
		if(ls_columns == NULL)
		{
			ls_columns = columns_create();
			columns_add_column(ls_columns, name_column);
		}
		return ls_columns;
	}

	if(cv_compare(view->custom.type))
	{
		if(comparison_columns == NULL)
		{
			comparison_columns = columns_create();
			columns_add_column(comparison_columns, name_column);
			columns_add_column(comparison_columns, id_column);
		}
		return comparison_columns;
	}

	return view->columns;
}

/* Corrects top of the other view to synchronize it with the current view if
 * 'scrollbind' option is set or view is in the compare mode. */
static void
consider_scroll_bind(FileView *view)
{
	if(cfg.scroll_bind || view->custom.type == CV_DIFF)
	{
		FileView *const other = (view == &lwin) ? &rwin : &lwin;
		const int bind_off = cfg.scroll_bind ? curr_stats.scroll_bind_off : 0;
		other->top_line = view->top_line/view->column_count;
		if(view == &lwin)
		{
			other->top_line += bind_off;
		}
		else
		{
			other->top_line -= bind_off;
		}
		other->top_line *= other->column_count;
		other->top_line = calculate_top_position(other, other->top_line);

		if(can_scroll_up(other))
		{
			(void)correct_list_pos_on_scroll_down(other, 0);
		}
		if(can_scroll_down(other))
		{
			(void)correct_list_pos_on_scroll_up(other, 0);
		}

		other->curr_line = other->list_pos - other->top_line;

		if(window_shows_dirlist(other))
		{
			draw_dir_list(other);
			refresh_view_win(other);
		}
	}
}

void
redraw_view(FileView *view)
{
	if(curr_stats.need_update == UT_NONE && !curr_stats.restart_in_progress)
	{
		redraw_view_imm(view);
	}
}

void
redraw_view_imm(FileView *view)
{
	if(window_shows_dirlist(view))
	{
		draw_dir_list(view);
		if(view == curr_view)
		{
			fview_cursor_redraw(view);
		}
		else
		{
			put_inactive_mark(view);
		}
	}
}

void
redraw_current_view(void)
{
	redraw_view(curr_view);
}

void
erase_current_line_bar(FileView *view)
{
	if(view == other_view)
	{
		put_inactive_mark(view);
	}
	else
	{
		clear_current_line_bar(view, 0);
	}
}

void
fview_cursor_redraw(FileView *view)
{
	if(view == curr_view)
	{
		/* Call file list function, which will also ensure that current position in
		 * list is correct. */
		flist_set_pos(view, view->list_pos);
	}
	else
	{
		if(move_curr_line(view))
		{
			draw_dir_list(view);
		}
		put_inactive_mark(view);
	}
}

/* Adds inactive cursor mark to the view. */
static void
put_inactive_mark(FileView *view)
{
	size_t col_width;
	size_t col_count;
	int line_attrs;
	int line, column;

	clear_current_line_bar(view, 1);

	if(!cfg.extra_padding)
	{
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);

	line_attrs = prepare_inactive_color(view, get_current_entry(view),
			get_line_color(view, view->list_pos));

	line = view->curr_line/col_count;
	column = view->real_num_width + (view->curr_line%col_count)*col_width;
	checked_wmove(view->win, line, column);

	wprinta(view->win, INACTIVE_CURSOR_MARK, line_attrs);
	ui_view_win_changed(view);
}

int
all_files_visible(const FileView *view)
{
	return view->list_rows <= (int)view->window_cells;
}

size_t
get_last_visible_cell(const FileView *view)
{
	return view->top_line + view->window_cells - 1;
}

size_t
get_window_top_pos(const FileView *view)
{
	if(view->top_line == 0)
	{
		return 0;
	}

	return view->top_line + get_effective_scroll_offset(view);
}

size_t
get_window_middle_pos(const FileView *view)
{
	const int list_middle = DIV_ROUND_UP(view->list_rows, (2*view->column_count));
	const int window_middle = DIV_ROUND_UP(view->window_rows, 2);
	return view->top_line
	     + MAX(0, MIN(list_middle, window_middle) - 1)*view->column_count;
}

size_t
get_window_bottom_pos(const FileView *view)
{
	if(view->list_rows - 1 <= (int)get_last_visible_cell(view))
	{
		const size_t last = view->list_rows - 1;
		return last - last%view->column_count;
	}
	else
	{
		const size_t off = get_effective_scroll_offset(view);
		const size_t column_correction = view->column_count - 1;
		return get_last_visible_cell(view) - off - column_correction;
	}
}

/* Calculate color attributes for cursor line of inactive pane.  Returns
 * attributes that can be used for drawing on a window. */
static int
prepare_inactive_color(FileView *view, dir_entry_t *entry, int line_color)
{
	const col_scheme_t *cs = ui_view_get_cs(view);
	col_attr_t col = cs->color[WIN_COLOR];

	mix_in_file_hi(view, entry, line_color, &col);

	if(entry->selected)
	{
		cs_mix_colors(&col, &cs->color[SELECTED_COLOR]);
	}

	if(cs_is_color_set(&cs->color[OTHER_LINE_COLOR]))
	{
		cs_mix_colors(&col, &cs->color[OTHER_LINE_COLOR]);
	}

	return COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr;
}

/* Redraws directory list without any extra actions that are performed in
 * erase_current_line_bar().  is_current defines whether element under the
 * cursor is being erased. */
static void
clear_current_line_bar(FileView *view, int is_current)
{
	const int old_cursor = view->curr_line;
	const int old_pos = view->top_line + old_cursor;
	size_t col_width;
	size_t col_count;
	size_t print_width;

	size_t prefix_len = 0U;
	column_data_t cdt = {
		.view = view,
		.line_pos = old_pos,
		.is_current = is_current,
		.prefix_len = &prefix_len,
	};

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(old_cursor < 0)
	{
		return;
	}

	if(old_pos < 0 || old_pos >= view->list_rows)
	{
		/* The entire list is going to be redrawn so just return. */
		return;
	}

	cdt.line_hi_group = get_line_color(view, old_pos),

	calculate_table_conf(view, &col_count, &col_width);

	cdt.current_line = old_cursor/col_count;
	cdt.column_offset = (old_cursor%col_count)*col_width;

	print_width = calculate_print_width(view, old_pos, col_width);

	if(!ui_view_displays_columns(view))
	{
		if(is_current)
		{
			/* When this function is used to draw cursor position in inactive ls-like
			 * view, only name width should be updated. */
			col_width = print_width;
		}
		else if(cfg.extra_padding)
		{
			/* Padding in ls-like view adds additional empty single character between
			 * columns, on which we shouldn't draw anything here. */
			--col_width;
		}
	}

	draw_cell(view, &cdt, col_width, print_width);
}

int
can_scroll_up(const FileView *view)
{
	return view->top_line > 0;
}

int
can_scroll_down(const FileView *view)
{
	return (int)get_last_visible_cell(view) < view->list_rows - 1;
}

void
scroll_up(FileView *view, size_t by)
{
	/* Round it up, so 1 will cause one line scrolling. */
	view->top_line -= view->column_count*DIV_ROUND_UP(by, view->column_count);
	if(view->top_line < 0)
	{
		view->top_line = 0;
	}
	view->top_line = calculate_top_position(view, view->top_line);

	view->curr_line = view->list_pos - view->top_line;
}

void
scroll_down(FileView *view, size_t by)
{
	/* Round it up, so 1 will cause one line scrolling. */
	view->top_line += view->column_count*DIV_ROUND_UP(by, view->column_count);
	view->top_line = calculate_top_position(view, view->top_line);

	view->curr_line = view->list_pos - view->top_line;
}

int
get_corrected_list_pos_down(const FileView *view, size_t pos_delta)
{
	const int scroll_offset = get_effective_scroll_offset(view);
	if(view->list_pos <=
			view->top_line + scroll_offset + (MAX((int)pos_delta, 1) - 1))
	{
		const size_t column_correction = view->list_pos%view->column_count;
		const size_t offset = scroll_offset + pos_delta + column_correction;
		return view->top_line + offset;
	}
	return view->list_pos;
}

int
get_corrected_list_pos_up(const FileView *view, size_t pos_delta)
{
	const int scroll_offset = get_effective_scroll_offset(view);
	const int last = get_last_visible_cell(view);
	if(view->list_pos >= last - scroll_offset - (MAX((int)pos_delta, 1) - 1))
	{
		const size_t column_correction = (view->column_count - 1) -
				view->list_pos%view->column_count;
		const size_t offset = scroll_offset + pos_delta + column_correction;
		return last - offset;
	}
	return view->list_pos;
}

int
consider_scroll_offset(FileView *view)
{
	int need_redraw = 0;
	int pos = view->list_pos;
	if(cfg.scroll_off > 0)
	{
		const int s = (int)get_effective_scroll_offset(view);
		/* Check scroll offset at the top. */
		if(can_scroll_up(view) && pos - view->top_line < s)
		{
			scroll_up(view, s - (pos - view->top_line));
			need_redraw = 1;
		}
		/* Check scroll offset at the bottom. */
		if(can_scroll_down(view))
		{
			const int last = (int)get_last_visible_cell(view);
			if(pos > last - s)
			{
				scroll_down(view, s + (pos - last));
				need_redraw = 1;
			}
		}
	}
	return need_redraw;
}

/* Returns scroll offset value for the view taking view height into account. */
static size_t
get_effective_scroll_offset(const FileView *view)
{
	int val = MIN(DIV_ROUND_UP(view->window_rows, 2), MAX(cfg.scroll_off, 0));
	return val*view->column_count;
}

void
scroll_by_files(FileView *view, ssize_t by)
{
	if(by > 0)
	{
		scroll_down(view, by);
	}
	else if(by < 0)
	{
		scroll_up(view, -by);
	}
}

void
update_scroll_bind_offset(void)
{
	const int rwin_pos = rwin.top_line/rwin.column_count;
	const int lwin_pos = lwin.top_line/lwin.column_count;
	curr_stats.scroll_bind_off = rwin_pos - lwin_pos;
}

/* Print callback for column_view unit. */
static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[])
{
	char print_buf[strlen(buf) + 1];
	size_t prefix_len, final_offset;
	size_t width_left, trim_pos;
	int reserved_width;

	const column_data_t *const cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line_pos];

	const int numbers_visible = (offset == 0 && ui_view_displays_numbers(view));
	const int padding = (cfg.extra_padding != 0);

	const int primary = (column_id == SK_BY_NAME || column_id == SK_BY_INAME);
	const int line_attrs = prepare_col_color(view, entry, primary,
			cdt->line_hi_group, cdt->is_current);

	size_t extra_prefix = primary ? *cdt->prefix_len : 0U;

	if(extra_prefix != 0U && align == AT_RIGHT)
	{
		/* Prefix length requires correction if left hand side of file name is
		 * trimmed. */
		size_t width = utf8_strsw(buf);
		if(utf8_strsw(full_column) > width)
		{
			/* As left side is trimmed and might contain ellipsis calculate offsets
			 * according to the right side. */
			width -= utf8_strsw(full_column + extra_prefix);
			extra_prefix = utf8_strsnlen(buf, width);
		}
	}

	prefix_len = padding + view->real_num_width + extra_prefix;
	final_offset = prefix_len + cdt->column_offset + offset;

	if(numbers_visible)
	{
		const int column = final_offset - extra_prefix - view->real_num_width;
		draw_line_number(cdt, column);
	}

	if(extra_prefix != 0U)
	{
		/* Copy prefix part into working buffer. */
		strncpy(print_buf, buf, extra_prefix);
		print_buf[extra_prefix] = '\0';
		buf += extra_prefix;
		full_column += extra_prefix;

		checked_wmove(view->win, cdt->current_line, final_offset - extra_prefix);
		wprinta(view->win, print_buf,
				prepare_col_color(view, entry, 0, cdt->line_hi_group, cdt->is_current));
	}

	checked_wmove(view->win, cdt->current_line, final_offset);

	if(fentry_is_fake(entry))
	{
		memset(print_buf, '.', sizeof(print_buf) - 1U);
		print_buf[sizeof(print_buf) - 1U] = '\0';
	}
	else
	{
		strcpy(print_buf, buf);
	}
	reserved_width = cfg.extra_padding ? (column_id != FILL_COLUMN_ID) : 0;
	width_left = padding + ui_view_available_width(view)
	           - reserved_width - offset;
	trim_pos = utf8_nstrsnlen(buf, width_left);
	if(trim_pos < sizeof(print_buf))
	{
		print_buf[trim_pos] = '\0';
	}
	wprinta(view->win, print_buf, line_attrs);

	if(primary && view->matches != 0 && entry->search_match)
	{
		highlight_search(view, entry, full_column, print_buf, trim_pos, align,
				cdt->current_line, final_offset, line_attrs);
	}
}

/* Draws current line number at specified column. */
static void
draw_line_number(const column_data_t *cdt, int column)
{
	FileView *const view = cdt->view;
	const size_t i = cdt->line_pos;
	dir_entry_t *const entry = &view->dir_entry[i];

	const int mixed = (cdt->is_current && view->num_type == NT_MIX);
	const char *const format = mixed ? "%-*d " : "%*d ";
	const int num = (view->num_type & NT_REL) && !mixed
	              ? abs((int)i - view->list_pos)
	              : (int)i + 1;

	char num_str[view->real_num_width + 1];
	snprintf(num_str, sizeof(num_str), format, view->real_num_width - 1, num);

	checked_wmove(view->win, cdt->current_line, column);
	wprinta(view->win, num_str,
			prepare_col_color(view, entry, 0, cdt->line_hi_group, cdt->is_current));
}

/* Highlights search match for the entry (assumed to be a search hit).  Modifies
 * the buf argument in process. */
static void
highlight_search(FileView *view, dir_entry_t *entry, const char full_column[],
		char buf[], size_t buf_len, AlignType align, int line, int col,
		int line_attrs)
{
	size_t name_offset, lo, ro;
	const char *fname;

	const size_t width = utf8_strsw(buf);

	const char *prefix, *suffix;
	ui_get_decors(entry, &prefix, &suffix);

	fname = get_last_path_component(full_column) + strlen(prefix);
	name_offset = fname - full_column;

	lo = name_offset + entry->match_left;
	ro = name_offset + entry->match_right;

	if((size_t)entry->match_right >= strlen(fname) - strlen(suffix))
	{
		/* Don't highlight anything past the end of file name (like trailing
		 * slash). */
		ro -= entry->match_right - (strlen(fname) - strlen(suffix));
	}

	if(align == AT_LEFT && buf_len < ro)
	{
		/* Right end of the match isn't visible. */

		char mark[4];
		const size_t mark_len = MIN(sizeof(mark) - 1, width);
		const int offset = width - mark_len;
		copy_str(mark, mark_len + 1, ">>>");

		checked_wmove(view->win, line, col + offset);
		wprinta(view->win, mark, line_attrs ^ A_REVERSE);
	}
	else if(align == AT_RIGHT && lo < (short int)strlen(full_column) - buf_len)
	{
		/* Left end of the match isn't visible. */

		char mark[4];
		const size_t mark_len = MIN(sizeof(mark) - 1, width);
		copy_str(mark, mark_len + 1, "<<<");

		checked_wmove(view->win, line, col);
		wprinta(view->win, mark, line_attrs ^ A_REVERSE);
	}
	else
	{
		/* Match is completely visible (although some chars might be concealed with
		 * ellipsis). */

		size_t match_start;
		char c;

		/* Match offsets require correction if left hand side of file name is
		 * trimmed. */
		if(align == AT_RIGHT && utf8_strsw(full_column) > width)
		{
			/* As left side is trimmed and might contain ellipsis calculate offsets
			 * according to the right side. */
			lo = utf8_strsnlen(buf, width - utf8_strsw(full_column + lo));
			ro = utf8_strsnlen(buf, width - utf8_strsw(full_column + ro));
		}

		/* Calculate number of screen characters before the match. */
		c = buf[lo];
		buf[lo] = '\0';
		match_start = utf8_strsw(buf);
		buf[lo] = c;

		checked_wmove(view->win, line, col + match_start);
		buf[ro] = '\0';
		wprinta(view->win, buf + lo, line_attrs ^ (A_REVERSE | A_UNDERLINE));
	}
}

/* Calculate color attributes for a view column.  Returns attributes that can be
 * used for drawing on a window. */
static int
prepare_col_color(const FileView *view, dir_entry_t *entry, int primary,
		int line_color, int current)
{
	FileView *const other = (view == &lwin) ? &rwin : &lwin;
	const col_scheme_t *const cs = ui_view_get_cs(view);
	col_attr_t col = cs->color[WIN_COLOR];

	/* File-specific highlight affects only primary field for non-current lines
	 * and whole line for the current line. */
	if(primary || current)
	{
		mix_in_file_hi(view, entry, line_color, &col);
	}

	/* If two files on the same line in side-by-side comparison have different
	 * ids, that's a mismatch. */
	if(view->custom.type == CV_DIFF &&
			other->dir_entry[entry_to_pos(view, entry)].id != entry->id)
	{
		cs_mix_colors(&col, &cs->color[MISMATCH_COLOR]);
	}

	if(entry->selected)
	{
		cs_mix_colors(&col, &cs->color[SELECTED_COLOR]);
	}

	if(current)
	{
		if(view == curr_view)
		{
			cs_mix_colors(&col, &cs->color[CURR_LINE_COLOR]);
		}
		else if(cs_is_color_set(&cs->color[OTHER_LINE_COLOR]))
		{
			cs_mix_colors(&col, &cs->color[OTHER_LINE_COLOR]);
		}
	}

	return COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr;
}

/* Applies file name and file type specific highlights for the entry. */
static void
mix_in_file_hi(const FileView *view, dir_entry_t *entry, int type_hi,
		col_attr_t *col)
{
	if(fentry_is_fake(entry))
	{
		return;
	}

	/* Apply file name specific highlights. */
	mix_in_file_name_hi(view, entry, col);

	/* Apply file type specific highlights for non-regular files (regular files
	 * are colored the same way window is). */
	if(type_hi != WIN_COLOR)
	{
		const col_scheme_t *cs = ui_view_get_cs(view);
		cs_mix_colors(col, &cs->color[type_hi]);
	}
}

/* Applies file name specific highlight for the entry. */
static void
mix_in_file_name_hi(const FileView *view, dir_entry_t *entry, col_attr_t *col)
{
	const col_scheme_t *const cs = ui_view_get_cs(view);
	char *const typed_fname = get_typed_entry_fpath(entry);
	const col_attr_t *color = cs_get_file_hi(cs, typed_fname, &entry->hi_num);
	free(typed_fname);
	if(color != NULL)
	{
		cs_mix_colors(col, color);
	}
}

/* File name format callback for column_view unit. */
TSTATIC void
format_name(int id, const void *data, size_t buf_len, char buf[])
{
	size_t len, i;
	dir_entry_t *child, *parent;

	const column_data_t *cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *const entry = &view->dir_entry[cdt->line_pos];

	if(!flist_custom_active(view))
	{
		/* Just file name. */
		format_entry_name(entry, buf_len + 1, buf);
		return;
	}

	if(!ui_view_displays_columns(view) || view->custom.type != CV_TREE)
	{
		/* File name possibly with path prefix. */
		get_short_path_of(view, entry, 1, 0, buf_len + 1U, buf);
		return;
	}

	/* File name possibly with path and tree prefixes. */
	len = 0U;
	child = entry;
	parent = child - child->child_pos;
	while(parent != child)
	{
		const char *prefix;
		/* To avoid prepending, strings are reversed here and whole tree prefix is
		 * reversed below to compensate for it. */
		if(parent->child_count == child->child_pos + child->child_count)
		{
			prefix = (child == entry ? " --`" : "    ");
		}
		else
		{
			prefix = (child == entry ? " --|" : "   |");
		}
		(void)sstrappend(buf, &len, buf_len + 1U, prefix);

		child = parent;
		parent -= parent->child_pos;
	}

	for(i = 0U; i < len/2U; ++i)
	{
		const char t = buf[i];
		buf[i] = buf[len - 1U - i];
		buf[len - 1U - i] = t;
	}

	get_short_path_of(view, entry, 1, 1, buf_len + 1U - len, buf + len);
	*cdt->prefix_len = len;
}

/* Primary name group format (first value of 'sortgroups' option) callback for
 * column_view unit. */
static void
format_primary_group(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	const FileView *view = cdt->view;
	const dir_entry_t *const entry = &view->dir_entry[cdt->line_pos];
	regmatch_t match = get_group_match(&view->primary_group, entry->name);

	copy_str(buf, MIN(buf_len + 1U, match.rm_eo - match.rm_so + 1U),
			entry->name + match.rm_so);
}

/* File size format callback for column_view unit. */
static void
format_size(int id, const void *data, size_t buf_len, char buf[])
{
	char str[24];
	const column_data_t *cdt = data;
	const FileView *view = cdt->view;
	const dir_entry_t *const entry = &view->dir_entry[cdt->line_pos];
	uint64_t size = DCACHE_UNKNOWN;

	if(fentry_is_dir(entry))
	{
		uint64_t nitems;
		dcache_get_of(entry, &size, &nitems);

		if(size == DCACHE_UNKNOWN && cfg.view_dir_size == VDS_NITEMS &&
				!view->on_slow_fs)
		{
			if(nitems == DCACHE_UNKNOWN)
			{
				nitems = entry_calc_nitems(entry);
			}

			snprintf(buf, buf_len + 1, " %d", (int)nitems);
			return;
		}
	}

	if(size == DCACHE_UNKNOWN)
	{
		size = entry->size;
	}

	str[0] = '\0';
	friendly_size_notation(size, sizeof(str), str);
	snprintf(buf, buf_len + 1, " %s", str);
}

/* Item number format callback for column_view unit. */
static void
format_nitems(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	const FileView *view = cdt->view;
	const dir_entry_t *const entry = &view->dir_entry[cdt->line_pos];
	uint64_t nitems;

	if(!fentry_is_dir(entry))
	{
		copy_str(buf, buf_len + 1, " 0");
		return;
	}

	if(view->on_slow_fs)
	{
		copy_str(buf, buf_len + 1, " ?");
		return;
	}

	dcache_get_of(entry, NULL, &nitems);

	if(nitems == DCACHE_UNKNOWN)
	{
		nitems = entry_calc_nitems(entry);
	}

	snprintf(buf, buf_len + 1, " %d", (int)nitems);
}

/* File type (dir/reg/exe/link/...) format callback for column_view unit. */
static void
format_type(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	snprintf(buf, buf_len, " %s", get_type_str(entry->type));
}

/* Symbolic link target format callback for column_view unit. */
static void
format_target(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	char full_path[PATH_MAX];

	buf[0] = '\0';

	if(entry->type != FT_LINK || !symlinks_available())
	{
		return;
	}

	get_full_path_of(entry, sizeof(full_path), full_path);
	(void)get_link_target(full_path, buf, buf_len);
}

/* File or directory extension format callback for column_view unit. */
static void
format_ext(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	const char *ext;

	ext = get_ext(entry->name);
	copy_str(buf, buf_len + 1, ext);
}

/* File-only extension format callback for column_view unit. */
static void
format_fileext(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];

	if(!fentry_is_dir(entry))
	{
		format_ext(id, data, buf_len, buf);
	}
	else
	{
		*buf = '\0';
	}
}

/* File modification/access/change date format callback for column_view unit. */
static void
format_time(int id, const void *data, size_t buf_len, char buf[])
{
	struct tm *tm_ptr;
	const column_data_t *cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line_pos];

	switch(id)
	{
		case SK_BY_TIME_MODIFIED:
			tm_ptr = localtime(&entry->mtime);
			break;
		case SK_BY_TIME_ACCESSED:
			tm_ptr = localtime(&entry->atime);
			break;
		case SK_BY_TIME_CHANGED:
			tm_ptr = localtime(&entry->ctime);
			break;

		default:
			assert(0 && "Unknown sort by time type");
			tm_ptr = NULL;
			break;
	}

	if(tm_ptr != NULL)
	{
		strftime(buf, buf_len + 1, cfg.time_format, tm_ptr);
	}
	else
	{
		buf[0] = '\0';
	}
}

/* Directory vs. file type format callback for column_view unit. */
static void
format_dir(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	const char *type = fentry_is_dir(entry) ? "dir" : "file";
	snprintf(buf, buf_len, " %s", type);
}

#ifndef _WIN32

/* File group id/name format callback for column_view unit. */
static void
format_group(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];

	buf[0] = ' ';
	get_gid_string(entry, id == SK_BY_GROUP_ID, buf_len - 1, buf + 1);
}

/* File owner id/name format callback for column_view unit. */
static void
format_owner(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];

	buf[0] = ' ';
	get_uid_string(entry, id == SK_BY_OWNER_ID, buf_len - 1, buf + 1);
}

/* File mode format callback for column_view unit. */
static void
format_mode(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	snprintf(buf, buf_len, " %o", entry->mode);
}

/* File permissions mask format callback for column_view unit. */
static void
format_perms(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line_pos];
	get_perm_string(buf, buf_len, entry->mode);
}

/* Hard link count format callback for column_view unit. */
static void
format_nlinks(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	snprintf(buf, buf_len, "%lu", (unsigned long)entry->nlinks);
}

#endif

/* File identifier on comparisons format callback for column_view unit. */
static void
format_id(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	snprintf(buf, buf_len, "#%d", entry->id);
}

void
fview_set_lsview(FileView *view, int enabled)
{
	if(view->ls_view != enabled)
	{
		view->ls_view = enabled;
		ui_view_schedule_redraw(view);
	}
}

size_t
calculate_columns_count(FileView *view)
{
	if(!ui_view_displays_columns(view))
	{
		const size_t column_width = calculate_column_width(view);
		return ui_view_available_width(view)/column_width;
	}
	return 1U;
}

/* Returns width of one column in the view. */
static size_t
calculate_column_width(FileView *view)
{
	const int column_gap = (cfg.extra_padding ? 2 : 1);
	if(view->max_filename_width == 0)
	{
		view->max_filename_width = get_max_filename_width(view);
	}
	return MIN(view->max_filename_width + column_gap,
	           (size_t)ui_view_available_width(view));
}

void
fview_dir_updated(FileView *view)
{
	view->local_cs = cs_load_local(view == &lwin, view->curr_dir);
}

void
fview_list_updated(FileView *view)
{
	/* Invalidate maximum file name widths cache. */
	view->max_filename_width = 0;
}

/* Finds maximum filename width (length in character positions on the screen)
 * among all entries of the view.  Returns the width. */
static size_t
get_max_filename_width(const FileView *view)
{
	size_t max_len = 0UL;
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		const size_t name_len = get_filename_width(view, i);
		if(name_len > max_len)
		{
			max_len = name_len;
		}
	}
	return max_len;
}

/* Gets filename width (length in character positions on the screen) of ith
 * entry of the view.  Returns the width. */
static size_t
get_filename_width(const FileView *view, int i)
{
	const dir_entry_t *const entry = &view->dir_entry[i];
	size_t name_len;
	if(flist_custom_active(view))
	{
		char name[NAME_MAX];
		/* XXX: should this be formatted name?. */
		get_short_path_of(view, entry, 0, 0, sizeof(name), name);
		name_len = utf8_strsw(name);
	}
	else
	{
		name_len = utf8_strsw(entry->name);
	}
	return name_len + get_filetype_decoration_width(entry);
}

/* Retrieves additional number of characters which are needed to display names
 * of the entry.  Returns the number. */
static size_t
get_filetype_decoration_width(const dir_entry_t *entry)
{
	const char *prefix, *suffix;
	ui_get_decors(entry, &prefix, &suffix);
	return strlen(prefix) + strlen(suffix);
}

void
fview_position_updated(FileView *view)
{
	int redraw = 0;
	size_t col_width;
	size_t col_count;
	size_t print_width;
	size_t prefix_len = 0U;
	column_data_t cdt = {
		.view = view,
		.is_current = 1,
		.prefix_len = &prefix_len,
	};

	if(view->curr_line > view->list_rows - 1)
	{
		view->curr_line = view->list_rows - 1;
	}

	if(curr_stats.load_stage < 1 || !window_shows_dirlist(view))
	{
		return;
	}

	if(view == other_view)
	{
		clear_current_line_bar(view, 0);
		if(move_curr_line(view))
		{
			draw_dir_list(view);
		}
		else
		{
			put_inactive_mark(view);
		}
		return;
	}

	erase_current_line_bar(view);

	redraw = move_curr_line(view);

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(redraw)
	{
		draw_dir_list(view);
		clear_current_line_bar(view, 0);
	}

	calculate_table_conf(view, &col_count, &col_width);
	print_width = calculate_print_width(view, view->list_pos, col_width);

	cdt.line_pos = view->list_pos;
	cdt.line_hi_group = get_line_color(view, view->list_pos);
	cdt.current_line = view->curr_line/col_count;
	cdt.column_offset = (view->curr_line%col_count)*col_width;

	draw_cell(view, &cdt, print_width, print_width);

	refresh_view_win(view);
	update_stat_window(view, 0);

	if(view == curr_view)
	{
		/* We're updating view non-lazily above, so doing the same with the
		 * ruler. */
		ui_ruler_update(view, 0);

		checked_wmove(view->win, cdt.current_line,
				cdt.column_offset + prefix_len + (cfg.extra_padding != 0));

		if(curr_stats.view)
		{
			qv_draw(view);
		}
	}
}

/* Returns non-zero if redraw is needed. */
static int
move_curr_line(FileView *view)
{
	int redraw = 0;
	int pos = view->list_pos;
	int last;

	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(pos == -1)
		return 0;

	view->list_pos = pos;

	if(view->curr_line > view->list_rows - 1)
		view->curr_line = view->list_rows - 1;

	view->top_line = calculate_top_position(view, view->top_line);

	last = (int)get_last_visible_cell(view);
	if(view->top_line <= pos && pos <= last)
	{
		view->curr_line = pos - view->top_line;
	}
	else if(pos > last)
	{
		scroll_down(view, pos - last);
		redraw++;
	}
	else if(pos < view->top_line)
	{
		scroll_up(view, view->top_line - pos);
		redraw++;
	}

	if(consider_scroll_offset(view))
	{
		redraw++;
	}

	return redraw != 0 || (view->num_type & NT_REL);
}

void
fview_sorting_updated(FileView *view)
{
	reset_view_columns(view);
}

/* Reinitializes view columns. */
static void
reset_view_columns(FileView *view)
{
	if(!ui_view_displays_columns(view) ||
			(curr_stats.restart_in_progress && flist_custom_active(view) &&
			 ui_view_unsorted(view)))
	{
		return;
	}

	if(view->view_columns[0] == '\0')
	{
		column_info_t column_info = {
			.column_id = SK_BY_NAME, .full_width = 0UL, .text_width = 0UL,
			.align = AT_LEFT,        .sizing = ST_AUTO, .cropping = CT_NONE,
		};

		columns_clear(view->columns);
		columns_add_column(view->columns, column_info);

		column_info.column_id = get_secondary_key((SortingKey)abs(view->sort[0]));
		column_info.align = AT_RIGHT;
		columns_add_column(view->columns, column_info);
	}
	else if(strstr(view->view_columns, "{}") != NULL)
	{
		load_view_columns_option(view, view->view_columns);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
