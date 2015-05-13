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

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <string.h> /* strcpy() strlen() */

#include "cfg/config.h"
#include "ui/statusline.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "column_view.h"
#include "filelist.h"
#include "opt_handlers.h"
#include "quickview.h"
#include "sort.h"

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
static void consider_scroll_bind(FileView *view);
static int prepare_inactive_color(FileView *view, dir_entry_t *entry,
		int line_color);
static int clear_current_line_bar(FileView *view, int is_current);
static size_t get_effective_scroll_offset(const FileView *view);
static void column_line_print(const void *data, int column_id, const char *buf,
		size_t offset, AlignType align, const char full_column[]);
static void highlight_search(FileView *view, dir_entry_t *entry,
		const char full_column[], char buf[], size_t buf_len, AlignType align,
		int line, int col, int line_attrs);
static int prepare_col_color(const FileView *view, dir_entry_t *entry,
		int primary, int line_color, int current);
static void mix_in_file_hi(const FileView *view, dir_entry_t *entry,
		int type_hi, col_attr_t *col);
static void mix_in_file_name_hi(const FileView *view, dir_entry_t *entry,
		col_attr_t *col);
static void format_name(int id, const void *data, size_t buf_len, char buf[]);
static void format_size(int id, const void *data, size_t buf_len, char buf[]);
static void format_type(int id, const void *data, size_t buf_len, char buf[]);
static void format_ext(int id, const void *data, size_t buf_len, char buf[]);
static void format_time(int id, const void *data, size_t buf_len, char buf[]);
static void format_dir(int id, const void *data, size_t buf_len, char buf[]);
#ifndef _WIN32
static void format_group(int id, const void *data, size_t buf_len, char buf[]);
static void format_mode(int id, const void *data, size_t buf_len, char buf[]);
static void format_owner(int id, const void *data, size_t buf_len, char buf[]);
static void format_perms(int id, const void *data, size_t buf_len, char buf[]);
#endif
static size_t calculate_column_width(FileView *view);
static size_t get_max_filename_width(const FileView *view);
static size_t get_filename_width(const FileView *view, int i);
static size_t get_filetype_decoration_width(FileType type);
static int move_curr_line(FileView *view);
static void reset_view_columns(FileView *view);

void
fview_init(void)
{
	static const struct {
		SortingKey key;
		column_func func;
	} sort_to_func[] = {
		{ SK_BY_NAME,  &format_name },
		{ SK_BY_INAME, &format_name },
		{ SK_BY_SIZE,  &format_size },
		{ SK_BY_TYPE,  &format_type },

		{ SK_BY_EXTENSION,     &format_ext },
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
#endif
	};
	ARRAY_GUARD(sort_to_func, SK_COUNT);

	size_t i;

	columns_set_line_print_func(&column_line_print);
	for(i = 0U; i < ARRAY_LEN(sort_to_func); ++i)
	{
		columns_add_column_desc(sort_to_func[i].key, sort_to_func[i].func);
	}
}

void
fview_view_init(FileView *view)
{
	view->curr_line = 0;
	view->top_line = 0;

	view->local_cs = 0;

	view->columns = columns_create();
	view->view_columns = strdup("");
}

void
fview_view_reset(FileView *view)
{
	view->ls_view = 0;
	view->max_filename_width = get_max_filename_width(view);
	view->column_count = 1;

	view->num_type = NT_NONE;
	view->num_width = 4;
	view->real_num_width = 0;

	view->postponed_redraw = 0;
	view->postponed_reload = 0;
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
	coll_pad = (view->ls_view && cfg.filelist_col_padding) ? 1 : 0;
	for(x = top; x < view->list_rows; ++x)
	{
		const column_data_t cdt = {
			.view = view,
			.line_pos = x,
			.line_hi_group = get_line_color(view, x),
			.is_current = (view == curr_view) ? x == view->list_pos : 0,
			.current_line = cell/col_count,
			.column_offset = (cell%col_count)*col_width,
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

	if(view->ls_view)
	{
		*count = calculate_columns_count(view);
		*width = calculate_column_width(view);
	}
	else
	{
		*count = 1;
		*width = MAX(0, ui_view_available_width(view) - view->real_num_width);
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
				if(is_on_slow_fs(full))
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
	if(view->ls_view)
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
	if(cfg.filelist_col_padding)
	{
		column_line_print(cdt, FILL_COLUMN_ID, " ", -1, AT_LEFT, " ");
	}

	columns_format_line(view->columns, cdt, col_width);

	if(cfg.filelist_col_padding)
	{
		column_line_print(cdt, FILL_COLUMN_ID, " ", print_width, AT_LEFT, " ");
	}
}

/* Corrects top of the other view to synchronize it with the current view if
 * 'scrollbind' option is set. */
static void
consider_scroll_bind(FileView *view)
{
	if(cfg.scroll_bind)
	{
		FileView *other = (view == &lwin) ? &rwin : &lwin;
		other->top_line = view->top_line/view->column_count;
		if(view == &lwin)
		{
			other->top_line += curr_stats.scroll_bind_off;
		}
		else
		{
			other->top_line -= curr_stats.scroll_bind_off;
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

		draw_dir_list(other);
		refresh_view_win(other);
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
	if(clear_current_line_bar(view, 0) && view == other_view)
	{
		put_inactive_mark(view);
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

void
put_inactive_mark(FileView *view)
{
	size_t col_width;
	size_t col_count;
	int line_attrs;
	int line, column;

	(void)clear_current_line_bar(view, 1);

	if(!cfg.filelist_col_padding)
	{
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);

	line_attrs = prepare_inactive_color(view, &view->dir_entry[view->list_pos],
			get_line_color(view, view->list_pos));

	line = view->curr_line/col_count;
	column = view->real_num_width + (view->curr_line%col_count)*col_width;
	checked_wmove(view->win, line, column);

	wprinta(view->win, INACTIVE_CURSOR_MARK, line_attrs);
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
	const int list_middle = view->list_rows/(2*view->column_count);
	const int window_middle = view->window_rows/2;
	return view->top_line + MIN(list_middle, window_middle)*view->column_count;
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
		mix_colors(&col, &cs->color[SELECTED_COLOR]);
	}

	if(is_color_set(&cs->color[OTHER_LINE_COLOR]))
	{
		mix_colors(&col, &cs->color[OTHER_LINE_COLOR]);
	}

	return COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr;
}

/* Redraws directory list without any extra actions that are performed in
 * erase_current_line_bar().  is_current defines whether element under the
 * cursor is being erased.  Returns non-zero if something was actually redrawn,
 * otherwise zero is returned. */
static int
clear_current_line_bar(FileView *view, int is_current)
{
	int old_cursor = view->curr_line;
	int old_pos = view->top_line + old_cursor;
	size_t col_width;
	size_t col_count;
	size_t print_width;

	column_data_t cdt = {
		.view = view,
		.line_pos = old_pos,
		.line_hi_group = get_line_color(view, old_pos),
		.is_current = is_current,
	};

	if(old_cursor < 0)
	{
		return 0;
	}

	if(curr_stats.load_stage < 2)
	{
		return 0;
	}

	if(old_pos < 0 || old_pos >= view->list_rows)
	{
		/* The entire list is going to be redrawn so just return. */
		return 0;
	}

	calculate_table_conf(view, &col_count, &col_width);

	cdt.current_line = old_cursor/col_count;
	cdt.column_offset = (old_cursor%col_count)*col_width;

	print_width = calculate_print_width(view, old_pos, col_width);

	if(is_current)
	{
		/* When this function is used to draw cursor position in inactive view, only
		 * name width should be updated. */
		col_width = print_width;
	}
	else if(view->ls_view && cfg.filelist_col_padding)
	{
		/* Padding in ls-like view adds additional empty single character between
		 * columns, on which we shouldn't draw anything here. */
		--col_width;
	}

	draw_cell(view, &cdt, col_width, print_width);

	return 1;
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
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset, AlignType align, const char full_column[])
{
	const int padding = (cfg.filelist_col_padding != 0);

	int primary;
	int line_attrs;
	char print_buf[strlen(buf) + 1];
	size_t width_left;
	size_t trim_pos;
	int reserved_width;

	const column_data_t *const cdt = data;
	const size_t i = cdt->line_pos;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[i];

	const int displays_numbers = (offset == 0 && ui_view_displays_numbers(view));

	const size_t prefix_len = padding + view->real_num_width;
	const size_t final_offset = prefix_len + cdt->column_offset + offset;

	primary = (column_id == SK_BY_NAME || column_id == SK_BY_INAME);
	line_attrs = prepare_col_color(view, entry, primary, cdt->line_hi_group,
			cdt->is_current);

	if(displays_numbers)
	{
		char number[view->real_num_width + 1];
		int mixed;
		const char *format;
		int line_number;

		const int line_attrs = prepare_col_color(view, entry, 0, cdt->line_hi_group,
				cdt->is_current);

		mixed = cdt->is_current && view->num_type == NT_MIX;
		format = mixed ? "%-*d " : "%*d ";
		line_number = ((view->num_type & NT_REL) && !mixed)
		            ? abs(i - view->list_pos)
		            : (int)(i + 1);

		snprintf(number, sizeof(number), format, view->real_num_width - 1,
				line_number);

		checked_wmove(view->win, cdt->current_line,
				final_offset - view->real_num_width);
		wprinta(view->win, number, line_attrs);
	}

	checked_wmove(view->win, cdt->current_line, final_offset);

	strcpy(print_buf, buf);
	reserved_width = cfg.filelist_col_padding ? (column_id != FILL_COLUMN_ID) : 0;
	width_left = padding + ui_view_available_width(view)
	           - reserved_width - offset;
	trim_pos = get_normal_utf8_string_widthn(buf, width_left);
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

/* Highlights search match for the entry (assumed to be a search hit).  Modifies
 * the buf argument in process. */
static void
highlight_search(FileView *view, dir_entry_t *entry, const char full_column[],
		char buf[], size_t buf_len, AlignType align, int line, int col,
		int line_attrs)
{
	const size_t width = get_screen_string_length(buf);

	const FileType type = ui_view_entry_target_type(view,
			entry_to_pos(view, entry));
	const size_t prefix_len = cfg.decorations[type][DECORATION_PREFIX] != '\0';

	const char *const fname = get_last_path_component(full_column) + prefix_len;
	const size_t name_offset = fname - full_column;

	size_t lo = name_offset + entry->match_left;
	size_t ro = name_offset + entry->match_right;

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

		if(align == AT_RIGHT)
		{
			/* Match offsets require correction if left hand side of the file is
			 * trimmed. */

			const size_t orig_width = get_screen_string_length(full_column);
			if(orig_width > width)
			{
				const int offset = orig_width - width;
				lo -= offset;
				ro -= offset;
			}
		}

		/* Calculate number of screen characters before the match. */
		c = buf[lo];
		buf[lo] = '\0';
		match_start = get_screen_string_length(buf);
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
	const col_scheme_t *const cs = ui_view_get_cs(view);
	col_attr_t col = cs->color[WIN_COLOR];

	/* File-specific highlight affects only primary field for non-current lines
	 * and whole line for the current line. */
	if(primary || current)
	{
		mix_in_file_hi(view, entry, line_color, &col);
	}

	if(entry->selected)
	{
		mix_colors(&col, &cs->color[SELECTED_COLOR]);
	}

	if(current)
	{
		if(view == curr_view)
		{
			mix_colors(&col, &cs->color[CURR_LINE_COLOR]);
		}
		else if(is_color_set(&cs->color[OTHER_LINE_COLOR]))
		{
			mix_colors(&col, &cs->color[OTHER_LINE_COLOR]);
		}
	}

	return COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr;
}

/* Applies file name and file type specific highlights for the entry. */
static void
mix_in_file_hi(const FileView *view, dir_entry_t *entry, int type_hi,
		col_attr_t *col)
{
	/* Apply file name specific highlights. */
	mix_in_file_name_hi(view, entry, col);

	/* Apply file type specific highlights for non-regular files (regular files
	 * are colored the same way window is). */
	if(type_hi != WIN_COLOR)
	{
		const col_scheme_t *cs = ui_view_get_cs(view);
		mix_colors(col, &cs->color[type_hi]);
	}
}

/* Applies file name specific highlight for the entry. */
static void
mix_in_file_name_hi(const FileView *view, dir_entry_t *entry, col_attr_t *col)
{
	const col_scheme_t *const cs = ui_view_get_cs(view);
	const col_attr_t *color = get_file_hi(cs, entry->name, &entry->hi_num);
	if(color != NULL)
	{
		mix_colors(col, color);
	}
}

/* File name format callback for column_view unit. */
static void
format_name(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	const FileView *view = cdt->view;
	if(flist_custom_active(view))
	{
		get_short_path_of(view, &view->dir_entry[cdt->line_pos], 1, buf_len + 1,
				buf);
	}
	else
	{
		format_entry_name(view, cdt->line_pos, buf_len + 1, buf);
	}
}

/* File size format callback for column_view unit. */
static void
format_size(int id, const void *data, size_t buf_len, char buf[])
{
	char str[24];
	const column_data_t *cdt = data;
	uint64_t size = get_file_size_by_entry(cdt->view, cdt->line_pos);

	str[0] = '\0';
	friendly_size_notation(size, sizeof(str), str);
	snprintf(buf, buf_len + 1, " %s", str);
}

/* File type (dir/reg/exe/link/...) format callback for column_view unit. */
static void
format_type(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	snprintf(buf, buf_len, " %s", get_type_str(entry->type));
}

/* File extension format callback for column_view unit. */
static void
format_ext(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line_pos];
	const char *ext;

	ext = get_ext(entry->name);
	copy_str(buf, buf_len + 1, ext);
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
	const char *type = is_directory_entry(entry) ? "dir" : "file";
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

#endif

void
fview_set_lsview(FileView *view, int enabled)
{
	if(view->ls_view == enabled)
	{
		return;
	}

	view->ls_view = enabled;

	if(view->ls_view)
	{
		column_info_t column_info = {
			.column_id = SK_BY_NAME, .full_width = 0UL, .text_width = 0UL,
			.align = AT_LEFT,        .sizing = ST_AUTO, .cropping = CT_ELLIPSIS,
		};

		columns_clear(view->columns);
		columns_add_column(view->columns, column_info);
		ui_view_schedule_redraw(view);
	}
	else
	{
		load_view_columns_option(view, view->view_columns);
	}
}

size_t
calculate_columns_count(FileView *view)
{
	if(view->ls_view)
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
	const int column_gap = (cfg.filelist_col_padding ? 2 : 1);
	return MIN(view->max_filename_width + column_gap,
	           (size_t)ui_view_available_width(view));
}

void
fview_dir_updated(FileView *view)
{
	view->local_cs = check_directory_for_color_scheme(view == &lwin,
			view->curr_dir);
}

void
fview_list_updated(FileView *view)
{
	view->max_filename_width = get_max_filename_width(view);
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
	const FileType target_type = ui_view_entry_target_type(view, i);
	size_t name_len;
	if(flist_custom_active(view))
	{
		char name[NAME_MAX];
		get_short_path_of(view, &view->dir_entry[i], 0, sizeof(name), name);
		name_len = get_screen_string_length(name);
	}
	else
	{
		name_len = get_screen_string_length(view->dir_entry[i].name);
	}
	return name_len + get_filetype_decoration_width(target_type);
}

/* Returns additional number of characters which are needed to display names of
 * files of specific type. */
static size_t
get_filetype_decoration_width(FileType type)
{
	const size_t prefix_len = cfg.decorations[type][DECORATION_PREFIX] != '\0';
	const size_t suffix_len = cfg.decorations[type][DECORATION_SUFFIX] != '\0';
	return prefix_len + suffix_len;
}

void
fview_position_updated(FileView *view)
{
	int redraw = 0;
	size_t col_width;
	size_t col_count;
	size_t print_width;
	column_data_t cdt = {
		.view = view,
		.is_current = 1,
	};

	if(view->curr_line > view->list_rows - 1)
	{
		view->curr_line = view->list_rows - 1;
	}

	if(curr_stats.load_stage < 1)
	{
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
	update_stat_window(view);

	if(curr_stats.view)
	{
		quick_view_file(view);
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
	if(view->ls_view)
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
/* vim: set cinoptions+=t0 : */
