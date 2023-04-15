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

#include <regex.h> /* regmatch_t regexec() */

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* abs() malloc() */
#include <string.h> /* memset() strcpy() strlen() */

#include "../cfg/config.h"
#include "../compat/pthread.h"
#include "../lua/vlua.h"
#include "../utils/fs.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/regexp.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../flist_pos.h"
#include "../opt_handlers.h"
#include "../sort.h"
#include "../vifm.h"
#include "color_scheme.h"
#include "column_view.h"
#include "quickview.h"
#include "statusline.h"

/* Mark for a cursor position of inactive pane. */
#define INACTIVE_CURSOR_MARK "*"

static void draw_left_column(view_t *view);
static void draw_right_column(view_t *view);
static void print_side_column(view_t *view, entries_t entries,
		const char current[], const char path[], int width, int offset,
		int number_width);
static void fill_column(view_t *view, int start_line, int top, int width,
		int offset);
static void calculate_table_conf(view_t *view, size_t *count, size_t *width);
static int calculate_number_width(const view_t *view, int list_length,
		int width);
static int count_digits(int num);
static int calculate_top_position(view_t *view, int top);
static int get_line_color(const view_t *view, const dir_entry_t *entry);
static void draw_cell(columns_t *columns, column_data_t *cdt, size_t col_width,
		int truncated);
static columns_t * get_view_columns(const view_t *view, int truncated);
static columns_t * get_name_column(int truncated);
static void consider_scroll_bind(view_t *view);
static cchar_t prepare_inactive_color(view_t *view, dir_entry_t *entry,
		int line_color);
static void redraw_cell(view_t *view, int top, int cursor, int is_current);
static void compute_and_draw_cell(column_data_t *cdt, int cell,
		size_t col_count, size_t col_width);
static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);
static void draw_line_number(const column_data_t *cdt, int column);
static void get_match_range(dir_entry_t *entry, const char full_column[],
		int *match_from, int *match_to);
static void highlight_search(view_t *view, const char full_column[], char buf[],
		size_t buf_len, AlignType align, int line, int col,
		const cchar_t *line_attrs, int match_from, int match_to);
static cchar_t prepare_col_color(const view_t *view, int primary, int line_nr,
		const column_data_t *cdt);
static void mix_in_common_colors(col_attr_t *col, const view_t *view,
		dir_entry_t *entry, int line_color);
static void mix_in_file_hi(const view_t *view, dir_entry_t *entry, int type_hi,
		col_attr_t *col);
static void mix_in_file_name_hi(const view_t *view, dir_entry_t *entry,
		col_attr_t *col);
TSTATIC void format_name(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_size(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_nitems(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_primary_group(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_type(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_target(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_ext(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_fileext(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_time(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_dir(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
#ifndef _WIN32
static void format_group(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_mode(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_owner(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_perms(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_nlinks(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void format_inode(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
#endif
static void format_id(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static size_t calculate_column_width(view_t *view);
static size_t calculate_columns_count(view_t *view);
static int has_extra_tls_col(const view_t *view, int col_width);
static preview_area_t get_miller_preview_area(view_t *view);
static size_t get_max_filename_width(const view_t *view);
static size_t get_filename_width(const view_t *view, int i);
static size_t get_filetype_decoration_width(const dir_entry_t *entry);
static int cache_cursor_pos(view_t *view);
static void invalidate_cursor_pos_cache(view_t *view);
static void position_hardware_cursor(view_t *view);
static int move_curr_line(view_t *view);
static void reset_view_columns(view_t *view);

void
fview_setup(void)
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

		{ SK_BY_INODE, &format_inode },
#endif
	};
	ARRAY_GUARD(sort_to_func, SK_COUNT);

	size_t i;

	columns_set_line_print_func(&column_line_print);
	for(i = 0U; i < ARRAY_LEN(sort_to_func); ++i)
	{
		columns_add_column_desc(sort_to_func[i].key, sort_to_func[i].func, NULL);
	}
	columns_add_column_desc(SK_BY_ID, &format_id, NULL);
	columns_add_column_desc(SK_BY_ROOT, &format_name, NULL);
	columns_add_column_desc(SK_BY_FILEROOT, &format_name, NULL);
}

void
fview_init(view_t *view)
{
	view->id = ui_next_view_id++;
	assert(ui_next_view_id != 0 && "Made full circle for view ids.");

	view->curr_line = 0;
	view->top_line = 0;

	view->local_cs = 0;

	view->columns = columns_create();
	view->view_columns = strdup("");
	view->view_columns_g = strdup("");

	view->sort_groups = strdup("");
	view->sort_groups_g = strdup("");
	(void)regexp_compile(&view->primary_group, view->sort_groups,
			REG_EXTENDED | REG_ICASE);

	view->preview_prg = strdup("");
	view->preview_prg_g = strdup("");

	view->timestamps_mutex = malloc(sizeof(*view->timestamps_mutex));
	pthread_mutex_init(view->timestamps_mutex, NULL);
}

void
fview_reset(view_t *view)
{
	view->ls_view_g = view->ls_view = 0;
	view->ls_transposed_g = view->ls_transposed = 0;
	view->ls_cols_g = view->ls_cols = 0;
	/* Invalidate maximum file name widths cache. */
	view->max_filename_width = 0;;
	view->column_count = 1;
	view->run_size = 1;

	view->miller_view_g = view->miller_view = 0;
	view->miller_ratios_g[0] = view->miller_ratios[0] = 1;
	view->miller_ratios_g[1] = view->miller_ratios[1] = 1;
	view->miller_ratios_g[2] = view->miller_ratios[2] = 1;
	view->miller_preview_g = view->miller_preview = MP_DIRS;

	view->num_type_g = view->num_type = NT_NONE;
	view->num_width_g = view->num_width = 4;
	view->real_num_width = 0;

	(void)ui_view_query_scheduled_event(view);
}

void
fview_reset_cs(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].hi_num = -1;
	}
}

void
draw_dir_list(view_t *view)
{
	draw_dir_list_only(view);

	if(view != curr_view)
	{
		fview_draw_inactive_cursor(view);
	}
}

void
draw_dir_list_only(view_t *view)
{
	int x, cell;
	size_t col_width, col_count;
	int visible_cells;

	if(curr_stats.load_stage < 2 || vifm_testing())
	{
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);

	ui_view_title_update(view);

	/* This is needed for reloading a list that has had files deleted. */
	while(view->list_rows - view->list_pos <= 0)
	{
		--view->list_pos;
		--view->curr_line;
	}

	view->top_line = calculate_top_position(view, view->top_line);

	ui_view_erase(view, 0);

	draw_left_column(view);

	visible_cells = view->window_cells;
	if(has_extra_tls_col(view, col_width))
	{
		visible_cells += view->window_rows;
	}

	for(x = view->top_line, cell = 0;
			x < view->list_rows && cell < visible_cells;
			++x, ++cell)
	{
		column_data_t cdt = {
			.view = view,
			.entry = &view->dir_entry[x],
			.line_pos = x,
			.current_pos = view->list_pos,
		};

		compute_and_draw_cell(&cdt, cell, col_count, col_width);
	}

	draw_right_column(view);

	view->curr_line = view->list_pos - view->top_line;

	if(view == curr_view)
	{
		consider_scroll_bind(view);
		position_hardware_cursor(view);
	}

	ui_view_win_changed(view);

	ui_view_redrawn(view);
}

/* Draws a column to the left of the main part of the view. */
static void
draw_left_column(view_t *view)
{
	char path[PATH_MAX + 1];
	const char *const dir = flist_get_dir(view);

	int number_width = 0;
	int lcol_width = ui_view_left_reserved(view) - 1;
	if(lcol_width <= 0)
	{
		flist_free_cache(&view->left_column);
		return;
	}

	copy_str(path, sizeof(path), dir);
	remove_last_path_component(path);
	(void)flist_update_cache(view, &view->left_column, path);

	number_width = calculate_number_width(view,
			view->left_column.entries.nentries,
			lcol_width - (cfg.extra_padding ? 2 : 0));
	lcol_width -= number_width;

	if(view->left_column.entries.nentries >= 0)
	{
		print_side_column(view, view->left_column.entries, dir, path, lcol_width, 0,
				number_width);
	}
}

/* Draws a column to the right of the main part of the view. */
static void
draw_right_column(view_t *view)
{
	const int displayed_graphics = view->displays_graphics;
	view->displays_graphics = 0;

	const int offset = ui_view_left_reserved(view)
	                 + ui_view_main_padded(view)
	                 + 1;

	const int rcol_width = ui_view_right_reserved(view) - 1;
	if(rcol_width <= 0)
	{
		flist_free_cache(&view->right_column);
		return;
	}

	const preview_area_t parea = get_miller_preview_area(view);

	dir_entry_t *const entry = get_current_entry(view);
	if(view->miller_preview != MP_DIRS && !fentry_is_dir(entry))
	{
		const char *clear_cmd = qv_draw_on(entry, &parea);
		update_string(&view->file_preview_clear_cmd, clear_cmd);
		return;
	}

	if(view->miller_preview == MP_FILES && fentry_is_dir(entry))
	{
		return;
	}

	if(displayed_graphics)
	{
		/* Do this even if there is no clear command. */
		qv_cleanup_area(&parea, view->file_preview_clear_cmd);
		update_string(&view->file_preview_clear_cmd, NULL);
	}

	char path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(path), path);
	(void)flist_update_cache(view, &view->right_column, path);

	if(view->right_column.entries.nentries >= 0)
	{
		print_side_column(view, view->right_column.entries, NULL, path, rcol_width,
				offset, 0);
	}
}

/* Prints column full of entry names.  Current is a hint that tells which column
 * has to be selected (otherwise position from history record is used). */
static void
print_side_column(view_t *view, entries_t entries, const char current[],
		const char path[], int width, int offset, int number_width)
{
	columns_t *const columns = get_name_column(0);
	const int scroll_offset = fpos_get_offset(view);
	int top, pos;
	int i;

	sort_entries(view, entries);

	pos = flist_hist_find(view, entries, path, &top);

	/* Use hint if provided. */
	if(current != NULL)
	{
		dir_entry_t *entry;
		entry = entry_from_path(view, entries.entries, entries.nentries, current);
		if(entry != NULL)
		{
			pos = entry - entries.entries;
		}
	}

	/* Make sure that current element is visible on the screen. */
	if(pos < top + scroll_offset)
	{
		top = pos - scroll_offset;
	}
	else if(pos >= top + view->window_rows - scroll_offset)
	{
		top = pos - (view->window_rows - scroll_offset - 1);
	}
	/* Ensure that top position is correct and we fill all lines for which we have
	 * files. */
	if(top < 0)
	{
		top = 0;
	}
	if(entries.nentries - top < view->window_rows)
	{
		top = MAX(0, entries.nentries - view->window_rows);
	}

	if(current != NULL)
	{
		/* We will display cursor on this entry and want history to be aware of
		 * it. */
		flist_hist_update(view, path, get_last_path_component(current), pos - top);
	}

	int padding = (cfg.extra_padding ? 1 : 0);

	for(i = top; i < entries.nentries && i - top < view->window_rows; ++i)
	{
		size_t prefix_len = 0U;
		column_data_t cdt = {
			.view = view,
			.entry = &entries.entries[i],
			.line_pos = i,
			.line_hi_group = get_line_color(view, &entries.entries[i]),
			.current_pos = pos,
			.total_width = number_width + width,
			.number_width = number_width,
			.current_line = i - top,
			.column_offset = offset,
			.prefix_len = &prefix_len,
		};

		draw_cell(columns, &cdt, width - 2*padding, /*truncated=*/0);
	}

	fill_column(view, i, top, number_width + width, offset - padding);
}

/* Fills column to the bottom to clear it from previous content. */
static void
fill_column(view_t *view, int start_line, int top, int width, int offset)
{
	char filler[width + 1];
	memset(filler, ' ', sizeof(filler) - 1U);
	filler[sizeof(filler) - 1U] = '\0';

	char a_space[] = " ";
	dir_entry_t non_entry = {
		.name = a_space,
		.origin = a_space,
		.type = FT_UNK,
	};

	int i;
	for(i = start_line; i - top < view->window_rows; ++i)
	{
		size_t prefix_len = 0U;
		column_data_t cdt = {
			.view = view,
			.entry = &non_entry,
			.line_pos = -1,
			.total_width = width,
			.current_pos = -1,
			.current_line = i - top,
			.column_offset = offset,
			.prefix_len = &prefix_len,
		};

		const format_info_t info = {
			.data = &cdt,
			.id = FILL_COLUMN_ID
		};

		column_line_print(filler, 0, AT_LEFT, filler, &info);
	}
}

/* Calculates number of columns and maximum width of column in a view. */
static void
calculate_table_conf(view_t *view, size_t *count, size_t *width)
{
	view->real_num_width = calculate_number_width(view, view->list_rows,
			ui_view_main_area(view));

	if(ui_view_displays_columns(view))
	{
		*count = 1;
		*width = MAX(0, ui_view_main_area(view) - view->real_num_width);
	}
	else
	{
		*count = calculate_columns_count(view);
		*width = calculate_column_width(view);
	}

	view->column_count = *count;
	view->run_size = fview_is_transposed(view) ? view->window_rows
	                                           : view->column_count;
	view->window_cells = *count*view->window_rows;
}

/* Calculates real number of characters that should be allocated in view for
 * numbers column.  Returns the number. */
static int
calculate_number_width(const view_t *view, int list_length, int width)
{
	if(ui_view_displays_numbers(view))
	{
		const int digit_count = count_digits(list_length);
		const int min = view->num_width;
		return MIN(MAX(1 + digit_count, min), width);
	}

	return 0;
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
calculate_top_position(view_t *view, int top)
{
	int result = MIN(MAX(top, 0), view->list_rows - 1);
	result = ROUND_DOWN(result, view->run_size);
	if(view->window_cells >= view->list_rows)
	{
		result = 0;
	}
	else if(view->list_rows - top < view->window_cells)
	{
		if(view->window_cells - (view->list_rows - top) >= view->run_size)
		{
			result = view->list_rows - view->window_cells + (view->run_size - 1);
			result = ROUND_DOWN(result, view->run_size);
			view->curr_line++;
		}
	}
	return result;
}

/* Calculates highlight group for the entry.  Returns highlight group number. */
static int
get_line_color(const view_t *view, const dir_entry_t *entry)
{
	switch(entry->type)
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
				char full[PATH_MAX + 1];
				get_full_path_of(entry, sizeof(full), full);
				if(get_link_target_abs(full, entry->origin, full, sizeof(full)) != 0)
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
			return (entry->nlinks > 1 ? HARD_LINK_COLOR : WIN_COLOR);
	}
}

/* Draws a full cell of the file list.  col_width doesn't include extra padding!
 * The total printed widths will be that plus two empty cells (one before and
 * one after). */
static void
draw_cell(columns_t *columns, column_data_t *cdt, size_t col_width,
		int truncated)
{
	int drop_right_padding = 0;

	/* When rightmost column of a transposed ls-like view is partially visible,
	 * make sure it's truncated by the window border rather than by the padding
	 * because the padding makes it look like the column was displayed in full. */
	if(truncated && cfg.extra_padding)
	{
		drop_right_padding = 1;
	}

	size_t width_left;
	if(cdt->view->ls_view)
	{
		width_left = cdt->view->window_cols
		           - cdt->column_offset
		           - ui_view_right_reserved(cdt->view)
		           - (cfg.extra_padding ? 2 : 0);

		if(drop_right_padding)
		{
			width_left += 1;
		}
	}
	else
	{
		width_left = cdt->is_main
		           ? ui_view_main_area(cdt->view) -
		             (cdt->column_offset - ui_view_left_reserved(cdt->view))
		           : col_width + 1U;
	}

	const format_info_t info = {
		.data = cdt,
		.id = FILL_COLUMN_ID
	};

	if(cfg.extra_padding)
	{
		column_line_print(" ", -1, AT_LEFT, " ", &info);
	}

	columns_format_line(columns, cdt, MIN(col_width, width_left));

	if(cfg.extra_padding && !drop_right_padding)
	{
		column_line_print(" ", col_width, AT_LEFT, " ", &info);
	}
}

/* Retrieves active view columns handle of the view considering 'lsview' option
 * status.  Returns the handle. */
static columns_t *
get_view_columns(const view_t *view, int truncated)
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

	if(!ui_view_displays_columns(view))
	{
		return get_name_column(truncated);
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

/* Retrieves columns view handle consisting of a single name column.  Returns
 * the handle. */
static columns_t *
get_name_column(int truncated)
{
	static const column_info_t name_column_ell = {
		.column_id = SK_BY_NAME, .full_width = 0UL, .text_width = 0UL,
		.align = AT_LEFT,        .sizing = ST_AUTO, .cropping = CT_ELLIPSIS,
	};
	static columns_t *columns_ell;

	if(truncated)
	{
		static columns_t *columns_trunc;
		if(columns_trunc == NULL)
		{
			column_info_t name_column_trunc = name_column_ell;
			name_column_trunc.cropping = CT_TRUNCATE;

			columns_trunc = columns_create();
			columns_add_column(columns_trunc, name_column_trunc);
		}
		return columns_trunc;
	}

	if(columns_ell == NULL)
	{
		columns_ell = columns_create();
		columns_add_column(columns_ell, name_column_ell);
	}
	return columns_ell;
}

/* Corrects top of the other view to synchronize it with the current view if
 * 'scrollbind' option is set or view is in the compare mode. */
static void
consider_scroll_bind(view_t *view)
{
	if(cfg.scroll_bind || view->custom.type == CV_DIFF)
	{
		view_t *const other = (view == &lwin) ? &rwin : &lwin;
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

		if(fpos_can_scroll_back(other))
		{
			(void)fpos_scroll_down(other, 0);
		}
		if(fpos_can_scroll_fwd(other))
		{
			(void)fpos_scroll_up(other, 0);
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
redraw_view(view_t *view)
{
	if(!stats_redraw_planned() && !curr_stats.restart_in_progress &&
			window_shows_dirlist(view))
	{
		/* Make sure cursor is visible and relevant part of the view is
		 * displayed. */
		(void)move_curr_line(view);
		/* Update cursor position cache as it might have been moved outside this
		 * unit. */
		(void)cache_cursor_pos(view);
		/* And then redraw the view unconditionally as requested. */
		draw_dir_list(view);
	}
}

void
redraw_current_view(void)
{
	redraw_view(curr_view);
}

void
fview_cursor_redraw(view_t *view)
{
	/* fview_cursor_redraw() is also called in situations when file list has
	 * changed, let fview_position_updated() deal with it.  With a cache of last
	 * position, it should be fine. */
	fview_position_updated(view);

	/* Always redrawing the cell won't hurt and will account for the case when
	 * selection state of item under the cursor has changed. */
	if(view == other_view)
	{
		fview_draw_inactive_cursor(view);
	}
	else
	{
		if(!ui_view_displays_columns(view))
		{
			/* Inactive cell in ls-like view usually takes less space than an active
			 * one.  Need to clear the cell before drawing over it. */
			redraw_cell(view, view->top_line, view->curr_line, 0);
		}
		redraw_cell(view, view->top_line, view->curr_line, 1);

		/* redraw_cell() naturally moves hardware cursor after current entry (to the
		 * next line when not in ls-like view).  Fix it up. */
		position_hardware_cursor(view);
	}
}

void
fview_clear_miller_preview(view_t *view)
{
	if(!view->miller_view || view->miller_preview == MP_DIRS)
	{
		return;
	}

	const int padding = (cfg.extra_padding ? 1 : 0);
	const int rcol_width = ui_view_right_reserved(view) - padding - 1;
	if(rcol_width > 0)
	{
		const preview_area_t parea = get_miller_preview_area(view);
		qv_cleanup_area(&parea, view->file_preview_clear_cmd);
	}
}

void
fview_draw_inactive_cursor(view_t *view)
{
	size_t col_width, col_count;
	int line, column;

	/* Reset last seen position on drawing inactive cursor or an active one won't
	 * be drawn next time. */
	invalidate_cursor_pos_cache(view);

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(!ui_view_displays_columns(view))
	{
		/* Inactive cell in ls-like view usually takes less space than an active
		 * one.  Need to clear the cell before drawing over it. */
		redraw_cell(view, view->top_line, view->curr_line, 0);
	}
	redraw_cell(view, view->top_line, view->curr_line, 1);

	if(!cfg.extra_padding)
	{
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);

	cchar_t line_attrs = prepare_inactive_color(view, get_current_entry(view),
			get_line_color(view, get_current_entry(view)));

	line = fpos_get_line(view, view->curr_line);
	column = view->real_num_width + ui_view_left_reserved(view)
	       + fpos_get_col(view, view->curr_line)*col_width;
	checked_wmove(view->win, line, column);

	wprinta(view->win, INACTIVE_CURSOR_MARK, &line_attrs, 0);
	ui_view_win_changed(view);
}

/* Calculate color attributes for cursor line of inactive pane.  Returns
 * attributes that can be used for drawing on a window. */
static cchar_t
prepare_inactive_color(view_t *view, dir_entry_t *entry, int line_color)
{
	const col_scheme_t *cs = ui_view_get_cs(view);
	col_attr_t col = ui_get_win_color(view, cs);

	mix_in_common_colors(&col, view, entry, line_color);
	cs_mix_colors(&col, &cs->color[OTHER_LINE_COLOR]);

	return cs_color_to_cchar(&col, -1);
}

/* Redraws single directory list entry.  is_current defines whether element
 * is under the cursor and should be highlighted as such.  The function depends
 * on passed in positions instead of view state to be independent from changes
 * in the view. */
static void
redraw_cell(view_t *view, int top, int cursor, int is_current)
{
	const int pos = top + cursor;
	if(pos < 0 || pos >= view->list_rows)
	{
		/* The entire list is going to be redrawn so just return. */
		return;
	}

	if(curr_stats.load_stage < 2 || cursor < 0)
	{
		return;
	}

	size_t col_width, col_count;
	calculate_table_conf(view, &col_count, &col_width);

	column_data_t cdt = {
		.view = view,
		.entry = &view->dir_entry[pos],
		.line_pos = pos,
		.current_pos = is_current ? view->list_pos : -1,
	};
	compute_and_draw_cell(&cdt, cursor, col_count, col_width);
}

/* Fills in fields of cdt based on passed in arguments and
 * view/entry/line_pos/current_pos fields of cdt.  Then draws the cell. */
static void
compute_and_draw_cell(column_data_t *cdt, int cell, size_t col_count,
		size_t col_width)
{
	size_t prefix_len = 0U;

	const int col = fpos_get_col(cdt->view, cell);

	cdt->current_line = fpos_get_line(cdt->view, cell);
	cdt->column_offset = ui_view_left_reserved(cdt->view) + col*col_width;
	cdt->line_hi_group = get_line_color(cdt->view, cdt->entry);
	cdt->number_width = cdt->view->real_num_width;
	cdt->total_width = ui_view_main_padded(cdt->view);
	cdt->prefix_len = &prefix_len;
	cdt->is_main = 1;

	if(cfg.extra_padding && !ui_view_displays_columns(cdt->view))
	{
		if(cdt->view->ls_cols != 0 && col_count > 1 && col == (int)col_count - 1)
		{
			/* Reserve one character column after the last ls column. */
			col_width -= 1;
		}
		else
		{
			/* Reserve two character columns between two ls columns to draw padding or
			 * before and after single column if it's the only one. */
			col_width -= 2;
		}
	}

	int truncated = (cell >= cdt->view->window_cells);
	columns_t *columns = get_view_columns(cdt->view, truncated);
	draw_cell(columns, cdt, col_width, truncated);

	cdt->prefix_len = NULL;
}

void
fview_scroll_back_by(view_t *view, int by)
{
	/* Round it up, so 1 will cause one line scrolling. */
	view->top_line -= view->run_size*DIV_ROUND_UP(by, view->run_size);
	if(view->top_line < 0)
	{
		view->top_line = 0;
	}
	view->top_line = calculate_top_position(view, view->top_line);

	view->curr_line = view->list_pos - view->top_line;
}

void
fview_scroll_fwd_by(view_t *view, int by)
{
	/* Round it up, so 1 will cause one line scrolling. */
	view->top_line += view->run_size*DIV_ROUND_UP(by, view->run_size);
	view->top_line = calculate_top_position(view, view->top_line);

	view->curr_line = view->list_pos - view->top_line;
}

void
fview_scroll_by(view_t *view, int by)
{
	if(by > 0)
	{
		fview_scroll_fwd_by(view, by);
	}
	else if(by < 0)
	{
		fview_scroll_back_by(view, -by);
	}
}

int
fview_enforce_scroll_offset(view_t *view)
{
	int need_redraw = 0;
	int pos = view->list_pos;
	if(cfg.scroll_off > 0)
	{
		const int s = fpos_get_offset(view);
		/* Check scroll offset at the top. */
		if(fpos_can_scroll_back(view) && pos - view->top_line < s)
		{
			fview_scroll_back_by(view, s - (pos - view->top_line));
			need_redraw = 1;
		}
		/* Check scroll offset at the bottom. */
		if(fpos_can_scroll_fwd(view))
		{
			const int last = fpos_get_last_visible_cell(view);
			if(pos > last - s)
			{
				fview_scroll_fwd_by(view, s + (pos - last));
				need_redraw = 1;
			}
		}
	}
	return need_redraw;
}

void
fview_scroll_page_up(view_t *view)
{
	if(fpos_can_scroll_back(view))
	{
		fpos_scroll_page(view, fpos_get_last_visible_cell(view), -1);
	}
}

void
fview_scroll_page_down(view_t *view)
{
	if(fpos_can_scroll_fwd(view))
	{
		fpos_scroll_page(view, view->top_line, 1);
	}
}

/* Print callback for column_view unit. */
static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	char print_buf[strlen(buf) + 1];
	size_t prefix_len, final_offset;
	size_t width_left, trim_pos;
	int reserved_width;

	const column_data_t *const cdt = info->data;
	view_t *view = cdt->view;
	dir_entry_t *entry = cdt->entry;

	const int numbers_visible = (offset == 0 && cdt->number_width > 0);
	const int padding = (cfg.extra_padding != 0);

	const int primary = info->id == SK_BY_NAME
	                 || info->id == SK_BY_INAME
	                 || info->id == SK_BY_ROOT
	                 || info->id == SK_BY_FILEROOT
	                 || info->id == SK_BY_EXTENSION
	                 || info->id == SK_BY_FILEEXT
	                 || vlua_viewcolumn_is_primary(curr_stats.vlua, info->id);
	const cchar_t line_attrs = prepare_col_color(view, primary, 0, cdt);

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

	prefix_len = padding + cdt->number_width + extra_prefix;
	final_offset = prefix_len + cdt->column_offset + offset;

	if(numbers_visible)
	{
		int column = final_offset - extra_prefix - cdt->number_width - padding;
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
		cchar_t cch = prepare_col_color(view, 0, 0, cdt);
		wprinta(view->win, print_buf, &cch, 0);
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
	reserved_width = (cfg.extra_padding && info->id != FILL_COLUMN_ID ? 1 : 0);
	width_left = cdt->total_width - reserved_width - offset;
	trim_pos = utf8_nstrsnlen(buf, width_left);
	if(trim_pos < sizeof(print_buf))
	{
		print_buf[trim_pos] = '\0';
	}
	wprinta(view->win, print_buf, &line_attrs, 0);

	if(primary && view->matches != 0 && entry->search_match)
	{
		int match_from = cdt->match_from;
		int match_to = cdt->match_to;

		if(!cdt->custom_match)
		{
			get_match_range(entry, full_column, &match_from, &match_to);
		}

		if(match_from != match_to)
		{
			highlight_search(view, full_column, print_buf, trim_pos, align,
					cdt->current_line, final_offset, &line_attrs, match_from, match_to);
		}
	}
}

/* Draws current line number at specified column. */
static void
draw_line_number(const column_data_t *cdt, int column)
{
	view_t *const view = cdt->view;

	const int mixed = cdt->line_pos == cdt->current_pos
	               && view->num_type == NT_MIX;
	const char *const format = mixed ? "%-*d " : "%*d ";
	const int num = (view->num_type & NT_REL) && !mixed
	              ? abs(cdt->line_pos - cdt->current_pos)
	              : cdt->line_pos + 1;
	const int padding = (cfg.extra_padding != 0);

	char num_str[cdt->number_width + 1];
	snprintf(num_str, sizeof(num_str), format, padding + cdt->number_width - 1,
			num);

	checked_wmove(view->win, cdt->current_line, column);
	cchar_t cch = prepare_col_color(view, 0, 1, cdt);
	wprinta(view->win, num_str, &cch, 0);
}

/* Adjusts search match offsets for the entry (assumed to be a search hit) to
 * account for decorations and full path.  Sets *match_from and *match_to. */
static void
get_match_range(dir_entry_t *entry, const char full_column[], int *match_from,
		int *match_to)
{
	const char *prefix, *suffix;
	ui_get_decors(entry, &prefix, &suffix);

	const char *fname = get_last_path_component(full_column) + strlen(prefix);
	size_t name_offset = fname - full_column;

	*match_from = name_offset + entry->match_left;
	*match_to = name_offset + entry->match_right;

	if((size_t)entry->match_right > strlen(fname) - strlen(suffix))
	{
		/* Don't highlight anything past the end of file name except for single
		 * trailing slash. */
		*match_to -= entry->match_right - (strlen(fname) - strlen(suffix));
		if(suffix[0] == '/')
		{
			++*match_to;
		}
	}
}

/* Highlights search match for the entry (assumed to be a search hit).  Modifies
 * the buf argument in process. */
static void
highlight_search(view_t *view, const char full_column[], char buf[],
		size_t buf_len, AlignType align, int line, int col,
		const cchar_t *line_attrs, int match_from, int match_to)
{
	size_t lo = match_from;
	size_t ro = match_to;

	const size_t width = utf8_strsw(buf);

	if(align == AT_LEFT && buf_len < ro)
	{
		/* Right end of the match isn't visible. */

		char mark[4];
		const size_t mark_len = MIN(sizeof(mark) - 1, width);
		const int offset = width - mark_len;
		copy_str(mark, mark_len + 1, ">>>");

		checked_wmove(view->win, line, col + offset);
		wprinta(view->win, mark, line_attrs, A_REVERSE);
	}
	else if(align == AT_RIGHT && lo < (short int)strlen(full_column) - buf_len)
	{
		/* Left end of the match isn't visible. */

		char mark[4];
		const size_t mark_len = MIN(sizeof(mark) - 1, width);
		copy_str(mark, mark_len + 1, "<<<");

		checked_wmove(view->win, line, col);
		wprinta(view->win, mark, line_attrs, A_REVERSE);
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
		wprinta(view->win, buf + lo, line_attrs, (A_REVERSE | A_UNDERLINE));
	}
}

/* Calculate color attributes for a view column.  Returns attributes that can be
 * used for drawing on a window. */
static cchar_t
prepare_col_color(const view_t *view, int primary, int line_nr,
		const column_data_t *cdt)
{
	const col_scheme_t *const cs = ui_view_get_cs(view);
	col_attr_t col = ui_get_win_color(view, cs);

	if(!cdt->is_main)
	{
		cs_mix_colors(&col, &cs->color[AUX_WIN_COLOR]);
	}

	if(cdt->current_line%2 == 1)
	{
		cs_mix_colors(&col, &cs->color[ODD_LINE_COLOR]);
	}

	if(cdt->line_pos != -1)
	{
		const int is_current = (cdt->line_pos == cdt->current_pos);

		/* File-specific highlight affects only primary field for non-current lines
		 * and whole line for the current line. */
		const int with_line_hi = (primary || is_current);
		const int line_color = with_line_hi ? cdt->line_hi_group : -1;
		mix_in_common_colors(&col, view, cdt->entry, line_color);

		if(is_current)
		{
			int color = (view == curr_view || !cdt->is_main) ? CURR_LINE_COLOR
			                                                 : OTHER_LINE_COLOR;
			/* Avoid combining attributes for non-primary column. */
			if(!primary)
			{
				cs_overlap_colors(&col, &cs->color[color]);
			}
			else
			{
				cs_mix_colors(&col, &cs->color[color]);
			}
		}
		else if(line_nr)
		{
			/* Line number of current line is not affected by this highlight group. */
			cs_mix_colors(&col, &cs->color[LINE_NUM_COLOR]);
		}
	}

	return cs_color_to_cchar(&col, -1);
}

/* Mixes in colors of current entry, mismatch and selection. */
static void
mix_in_common_colors(col_attr_t *col, const view_t *view, dir_entry_t *entry,
		int line_color)
{
	if(line_color >= 0)
	{
		mix_in_file_hi(view, entry, line_color, col);
	}

	const col_scheme_t *const cs = ui_view_get_cs(view);
	view_t *const other = (view == &lwin) ? &rwin : &lwin;

	if(view->custom.type == CV_DIFF)
	{
		const dir_entry_t *oentry = &other->dir_entry[entry_to_pos(view, entry)];

		/* If two files on the same line in side-by-side comparison have different
		 * ids, that's a mismatch. */
		if(oentry->id != entry->id)
		{
			cs_mix_colors(col, &cs->color[MISMATCH_COLOR]);
		}
		else if(fentry_is_fake(entry))
		{
			cs_mix_colors(col, &cs->color[BLANK_COLOR]);
		}
		else if(fentry_is_fake(oentry))
		{
			cs_mix_colors(col, &cs->color[UNMATCHED_COLOR]);
		}
	}

	if(entry->selected)
	{
		cs_mix_colors(col, &cs->color[SELECTED_COLOR]);
	}
}

/* Applies file name and file type specific highlights for the entry. */
static void
mix_in_file_hi(const view_t *view, dir_entry_t *entry, int type_hi,
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
mix_in_file_name_hi(const view_t *view, dir_entry_t *entry, col_attr_t *col)
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
format_name(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	dir_entry_t *child, *parent;

	const column_data_t *cdt = info->data;
	view_t *view = cdt->view;

	NameFormat fmt = NF_FULL;
	if(info->id == SK_BY_ROOT ||
			(info->id == SK_BY_FILEROOT && !fentry_is_dir(cdt->entry)))
	{
		fmt = NF_ROOT;
	}

	if(!flist_custom_active(view))
	{
		/* Just file name. */
		format_entry_name(cdt->entry, fmt, buf_len + 1, buf);
		return;
	}

	if(!ui_view_displays_columns(view) || !cv_tree(view->custom.type))
	{
		/* File name possibly with path prefix. */
		get_short_path_of(view, cdt->entry, fmt, 0, buf_len + 1U, buf);
		return;
	}

	/* File name possibly with path and tree prefixes. */
	size_t prefix_len = 0U;
	child = cdt->entry;
	parent = child - child->child_pos;
	while(parent != child)
	{
		const int folded = child->folded;

		const char *prefix;
		/* To avoid prepending, strings are reversed here and whole tree prefix is
		 * reversed below to compensate for it. */
		if(parent->child_count == child->child_pos + child->child_count)
		{
			prefix = (child == cdt->entry ? (folded ? " ++`" : " --`") : "    ");
		}
		else
		{
			prefix = (child == cdt->entry ? (folded ? " ++|" : " --|") : "   |");
		}
		(void)sstrappend(buf, &prefix_len, buf_len + 1U, prefix);

		child = parent;
		parent -= parent->child_pos;
	}

	if(cdt->entry->child_pos == 0 && cdt->entry->folded)
	{
		/* Mark root node as folded. */
		(void)sstrappend(buf, &prefix_len, buf_len + 1U, " ++");
	}

	size_t i;
	for(i = 0U; i < prefix_len/2U; ++i)
	{
		const char t = buf[i];
		buf[i] = buf[prefix_len - 1U - i];
		buf[prefix_len - 1U - i] = t;
	}

	get_short_path_of(view, cdt->entry, fmt, 1, buf_len + 1U - prefix_len,
			buf + prefix_len);
	*cdt->prefix_len = prefix_len;
}

/* Primary name group format (first value of 'sortgroups' option) callback for
 * column_view unit. */
static void
format_primary_group(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	const view_t *view = cdt->view;
	regmatch_t match = get_group_match(&view->primary_group, cdt->entry->name);

	copy_str(buf, MIN(buf_len + 1U, (size_t)match.rm_eo - match.rm_so + 1U),
			cdt->entry->name + match.rm_so);
}

/* File size format callback for column_view unit. */
static void
format_size(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	char str[64];
	const column_data_t *cdt = info->data;
	const view_t *view = cdt->view;
	uint64_t size = DCACHE_UNKNOWN;

	if(fentry_is_dir(cdt->entry))
	{
		uint64_t nitems;
		uint64_t *nitems_ptr = (cfg.view_dir_size == VDS_NITEMS ? &nitems : NULL);
		fentry_get_dir_info(view, cdt->entry, &size, nitems_ptr);

		if(size == DCACHE_UNKNOWN && nitems_ptr != NULL)
		{
			snprintf(buf, buf_len + 1, " %d", (int)nitems);
			return;
		}
	}

	if(size == DCACHE_UNKNOWN)
	{
		size = cdt->entry->size;
	}

	str[0] = '\0';
	friendly_size_notation(size, sizeof(str), str);
	snprintf(buf, buf_len + 1, " %s", str);
}

/* Item number format callback for column_view unit. */
static void
format_nitems(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	uint64_t nitems;

	if(!fentry_is_dir(cdt->entry))
	{
		copy_str(buf, buf_len + 1, " 0");
		return;
	}

	if(cdt->view->on_slow_fs)
	{
		copy_str(buf, buf_len + 1, " ?");
		return;
	}

	nitems = fentry_get_nitems(cdt->view, cdt->entry);
	snprintf(buf, buf_len + 1, " %d", (int)nitems);
}

/* File type (dir/reg/exe/link/...) format callback for column_view unit. */
static void
format_type(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	snprintf(buf, buf_len, " %s", get_type_str(cdt->entry->type));
}

/* Symbolic link target format callback for column_view unit. */
static void
format_target(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	char full_path[PATH_MAX + 1];

	buf[0] = '\0';

	if(cdt->entry->type != FT_LINK || !symlinks_available())
	{
		return;
	}

	get_full_path_of(cdt->entry, sizeof(full_path), full_path);
	(void)get_link_target(full_path, buf, buf_len);
}

/* File or directory extension format callback for column_view unit. */
static void
format_ext(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	const char *const ext = get_ext(cdt->entry->name);
	copy_str(buf, buf_len + 1, ext);
}

/* File-only extension format callback for column_view unit. */
static void
format_fileext(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	const column_data_t *cdt = info->data;

	if(!fentry_is_dir(cdt->entry))
	{
		format_ext(data, buf_len, buf, info);
	}
	else
	{
		*buf = '\0';
	}
}

/* File modification/access/change date format callback for column_view unit. */
static void
format_time(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	struct tm *tm_ptr;
	const column_data_t *cdt = info->data;

	switch(info->id)
	{
		case SK_BY_TIME_MODIFIED:
			tm_ptr = localtime(&cdt->entry->mtime);
			break;
		case SK_BY_TIME_ACCESSED:
			tm_ptr = localtime(&cdt->entry->atime);
			break;
		case SK_BY_TIME_CHANGED:
			tm_ptr = localtime(&cdt->entry->ctime);
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
format_dir(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	const char *type = fentry_is_dir(cdt->entry) ? "dir" : "file";
	snprintf(buf, buf_len, " %s", type);
}

#ifndef _WIN32

/* File group id/name format callback for column_view unit. */
static void
format_group(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;

	buf[0] = ' ';
	get_gid_string(cdt->entry, info->id == SK_BY_GROUP_ID, buf_len - 1, buf + 1);
}

/* File owner id/name format callback for column_view unit. */
static void
format_owner(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;

	buf[0] = ' ';
	get_uid_string(cdt->entry, info->id == SK_BY_OWNER_ID, buf_len - 1, buf + 1);
}

/* File mode format callback for column_view unit. */
static void
format_mode(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	snprintf(buf, buf_len, " %o", cdt->entry->mode);
}

/* File permissions mask format callback for column_view unit. */
static void
format_perms(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	get_perm_string(buf, buf_len, cdt->entry->mode);
}

/* Hard link count format callback for column_view unit. */
static void
format_nlinks(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	snprintf(buf, buf_len, "%lu", (unsigned long)cdt->entry->nlinks);
}

/* Inode number format callback for column_view unit. */
static void
format_inode(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	snprintf(buf, buf_len, "%lu", (unsigned long)cdt->entry->inode);
}

#endif

/* File identifier on comparisons format callback for column_view unit. */
static void
format_id(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	const column_data_t *cdt = info->data;
	snprintf(buf, buf_len, "#%d", cdt->entry->id);
}

void
fview_set_lsview(view_t *view, int enabled)
{
	if(view->ls_view != enabled)
	{
		view->ls_view = enabled;
		ui_view_schedule_redraw(view);
	}
}

int
fview_is_transposed(const view_t *view)
{
	return (!ui_view_displays_columns(view) && view->ls_transposed);
}

int
fview_previews(view_t *view, const char path[])
{
	if(!view->miller_view || view->miller_preview == MP_DIRS)
	{
		return 0;
	}

	dir_entry_t *entry = get_current_entry(view);
	if(fentry_is_dir(entry))
	{
		return 0;
	}

	return fentry_points_to(entry, path);
}

void
fview_set_millerview(view_t *view, int enabled)
{
	if(view->miller_view != enabled)
	{
		view->miller_view = enabled;
		ui_view_schedule_redraw(view);
	}
}

/* Returns width of one column in the view. */
static size_t
calculate_column_width(view_t *view)
{
	size_t max_width = ui_view_main_padded(view);

	size_t column_width;
	if(view->ls_cols == 0)
	{
		if(view->max_filename_width == 0)
		{
			view->max_filename_width = get_max_filename_width(view);
		}

		const int column_gap = (cfg.extra_padding ? 2 : 1);
		column_width = view->max_filename_width + column_gap;
	}
	else
	{
		column_width = max_width/view->ls_cols;
	}

	return MIN(column_width, max_width);
}

int
fview_map_coordinates(view_t *view, int x, int y)
{
	if(view->miller_view)
	{
		const int lcol_end = ui_view_left_reserved(view);
		const int rcol_start = lcol_end + ui_view_main_padded(view);

		if(x < lcol_end)
		{
			return FVM_LEAVE;
		}
		if(x >= rcol_start)
		{
			return FVM_OPEN;
		}
	}

	int pos;
	if(ui_view_displays_columns(view))
	{
	  pos = view->top_line + y;
	}
	else
	{
		size_t col_count, col_width;
		calculate_table_conf(view, &col_count, &col_width);

		if(has_extra_tls_col(view, col_width))
		{
			++col_count;
		}

		size_t x_offset = x/col_width;
		if(x_offset >= col_count)
		{
			return FVM_NONE;
		}

		pos = fview_is_transposed(view)
		    ? view->top_line + view->run_size*x_offset + y
		    : view->top_line + view->run_size*y + x_offset;
	}

	return (pos < view->list_rows ? pos : FVM_NONE);
}

/* Whether there is an extra visual column to transposed ls-like view to display
 * more context (when available and unless user requested fixed number of
 * columns).  Returns non-zero if so. */
static int
has_extra_tls_col(const view_t *view, int col_width)
{
	return fview_is_transposed(view)
			&& view->ls_cols == 0
			&& view->column_count*col_width < ui_view_main_area(view);
}

void
fview_update_geometry(view_t *view)
{
	view->column_count = calculate_columns_count(view);
	view->run_size = fview_is_transposed(view) ? view->window_rows
	                                           : view->column_count;
	view->window_cells = view->column_count*view->window_rows;
}

void
fview_dir_updated(view_t *view)
{
	view->local_cs = cs_load_local(view == &lwin, view->curr_dir);
	fview_clear_miller_preview(view);
}

/* Computes area description for miller preview.  Returns the area. */
static preview_area_t
get_miller_preview_area(view_t *view)
{
	const col_scheme_t *const cs = ui_view_get_cs(view);
	col_attr_t def_col = ui_get_win_color(view, cs);
	cs_mix_colors(&def_col, &cs->color[AUX_WIN_COLOR]);

	const int offset = ui_view_left_reserved(view)
	                 + ui_view_main_padded(view)
	                 + 1;

	const preview_area_t parea = {
		.source = view,
		.view = view,
		.def_col = def_col,
		.x = offset,
		.y = 0,
		/* Lack of `- padding` here is intentional, the preview should fill the view
		 * until the right border. */
		.w = ui_view_right_reserved(view) - 1,
		.h = view->window_rows,
	};
	return parea;
}

void
fview_list_updated(view_t *view)
{
	/* Invalidate maximum file name widths cache. */
	view->max_filename_width = 0;
	/* Even if position will remain the same, we might need to redraw it. */
	invalidate_cursor_pos_cache(view);
}

void
fview_decors_updated(view_t *view)
{
	/* Invalidate maximum file name widths cache. */
	view->max_filename_width = 0;
}

/* Evaluates number of columns in the view.  Returns the number. */
static size_t
calculate_columns_count(view_t *view)
{
	if(!ui_view_displays_columns(view))
	{
		const size_t column_width = calculate_column_width(view);
		size_t max_width = ui_view_main_padded(view);
		return max_width/column_width;
	}
	return 1U;
}

/* Finds maximum filename width (length in character positions on the screen)
 * among all entries of the view.  Returns the width. */
static size_t
get_max_filename_width(const view_t *view)
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
get_filename_width(const view_t *view, int i)
{
	const dir_entry_t *const entry = &view->dir_entry[i];
	size_t name_len;
	if(flist_custom_active(view))
	{
		char name[NAME_MAX + 1];
		/* XXX: should this be formatted name?. */
		get_short_path_of(view, entry, NF_NONE, 0, sizeof(name), name);
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
	return utf8_strsw(prefix) + utf8_strsw(suffix);
}

void
fview_position_updated(view_t *view)
{
	const int old_top = view->top_line;
	const int old_curr = view->curr_line;

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
		invalidate_cursor_pos_cache(view);
		if(move_curr_line(view))
		{
			draw_dir_list(view);
		}
		else
		{
			redraw_cell(view, old_top, old_curr, 0);
			fview_draw_inactive_cursor(view);
		}
		return;
	}

	if(curr_stats.load_stage < 2)
	{
		return;
	}

	int redraw = move_curr_line(view);
	int up_to_date = cache_cursor_pos(view);
	if(!redraw && up_to_date)
	{
		return;
	}

	if(redraw)
	{
		draw_dir_list(view);
	}
	else
	{
		redraw_cell(view, old_top, old_curr, 0);
		redraw_cell(view, view->top_line, view->curr_line, 1);
		draw_right_column(view);
	}

	refresh_view_win(view);
	ui_stat_update(view, 0);

	if(view == curr_view)
	{
		/* We're updating view non-lazily above, so doing the same with the
		 * ruler. */
		ui_ruler_update(view, 0);

		position_hardware_cursor(view);

		if(curr_stats.preview.on)
		{
			qv_draw(view);
		}
	}
}

/* Compares current cursor position against previously cached one and updates
 * the cache if necessary.  Returns non-zero if cache is up to date, otherwise
 * zero is returned. */
static int
cache_cursor_pos(view_t *view)
{
	char path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(path), path);

	if(view->list_pos == view->last_seen_pos &&
		 view->curr_line == view->last_curr_line &&
		 view->last_curr_file != NULL &&
		 strcmp(view->last_curr_file, path) == 0)
	{
		return 1;
	}

	view->last_seen_pos = view->list_pos;
	view->last_curr_line = view->curr_line;
	replace_string(&view->last_curr_file, path);
	return 0;
}

/* Invalidates cache of current cursor position for the specified view. */
static void
invalidate_cursor_pos_cache(view_t *view)
{
	view->last_seen_pos = -1;
}

/* Moves hardware cursor to the beginning of the name of current entry. */
static void
position_hardware_cursor(view_t *view)
{
	size_t col_width, col_count;
	int current_line, column_offset;
	char buf[view->window_cols + 1];

	size_t prefix_len = 0U;
	column_data_t cdt = {
		.view = view,
		.entry = get_current_entry(view),
		.prefix_len = &prefix_len,
	};
	const format_info_t info = {
		.data = &cdt,
		.id = SK_BY_NAME
	};

	if(cdt.entry == NULL)
	{
		/* Happens in tests. */
		return;
	}

	int cell = view->list_pos - view->top_line;

	calculate_table_conf(view, &col_count, &col_width);
	current_line = fpos_get_line(view, cell);
	column_offset = ui_view_left_reserved(view)
	              + fpos_get_col(view, cell)*col_width;
	format_name(NULL, sizeof(buf) - 1U, buf, &info);

	checked_wmove(view->win, current_line,
			(cfg.extra_padding != 0) + column_offset + prefix_len);
}

/* Returns non-zero if redraw is needed. */
static int
move_curr_line(view_t *view)
{
	int redraw = 0;
	int pos = view->list_pos;
	int last;
	size_t col_width, col_count;
	columns_t *columns;

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

	last = fpos_get_last_visible_cell(view);
	if(view->top_line <= pos && pos <= last)
	{
		view->curr_line = pos - view->top_line;
	}
	else if(pos > last)
	{
		fview_scroll_fwd_by(view, pos - last);
		redraw++;
	}
	else if(pos < view->top_line)
	{
		fview_scroll_back_by(view, view->top_line - pos);
		redraw++;
	}

	if(fview_enforce_scroll_offset(view))
	{
		redraw++;
	}

	calculate_table_conf(view, &col_count, &col_width);
	/* Columns might be NULL in tests. */
	columns = get_view_columns(view, 0);
	if(columns != NULL && !columns_matches_width(columns, col_width))
	{
		redraw++;
	}

	return redraw != 0 || (view->num_type & NT_REL);
}

void
fview_sorting_updated(view_t *view)
{
	reset_view_columns(view);
}

/* Reinitializes view columns. */
static void
reset_view_columns(view_t *view)
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
