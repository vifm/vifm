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

#include "filelist.h"

#ifdef _WIN32
#include <fcntl.h>
#include <lm.h>
#include <ntdef.h>
#include <windows.h>
#include <winioctl.h>
#endif

#include <curses.h>

#include <dirent.h> /* DIR */
#include <sys/stat.h> /* stat */
#include <sys/time.h> /* localtime */
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS */
#include <pwd.h>
#include <grp.h>
#endif

#include <assert.h> /* assert() */
#include <errno.h>
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() malloc() */
#include <string.h> /* memset() strcat() strcmp() strcpy() strlen() */
#include <time.h>

#include "cfg/config.h"
#include "menus/menus.h"
#include "modes/file_info.h"
#include "modes/modes.h"
#include "utils/env.h"
#include "utils/filter.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/tree.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "background.h"
#include "color_scheme.h"
#include "column_view.h"
#include "fileops.h"
#include "fileops.h"
#include "filetype.h"
#include "fuse.h"
#include "macros.h"
#include "opt_handlers.h"
#include "quickview.h"
#include "running.h"
#include "sort.h"
#include "status.h"
#include "term_title.h"
#include "types.h"
#include "ui.h"

#ifdef _WIN32
#define CASE_SENSATIVE_FILTER 0
#else
#define CASE_SENSATIVE_FILTER 1
#endif

/* Packet set of parameters to pass as user data for processing columns. */
typedef struct
{
	FileView *view;
	size_t line;
	int current;
	size_t current_line;
	size_t column_offset;
}
column_data_t;

static void column_line_print(const void *data, int column_id, const char *buf,
		size_t offset);
static void format_name(int id, const void *data, size_t buf_len, char *buf);
static void format_size(int id, const void *data, size_t buf_len, char *buf);
static void format_ext(int id, const void *data, size_t buf_len, char *buf);
#ifndef _WIN32
static void format_group(int id, const void *data, size_t buf_len, char *buf);
static void format_mode(int id, const void *data, size_t buf_len, char *buf);
static void format_owner(int id, const void *data, size_t buf_len, char *buf);
#endif
static void format_time(int id, const void *data, size_t buf_len, char *buf);
#ifndef _WIN32
static void format_perms(int id, const void *data, size_t buf_len, char buf[]);
#endif
static void init_view(FileView *view);
static void reset_view(FileView *view);
static void reset_filter(filter_t *filter);
static void init_view_history(FileView *view);
static int get_line_color(FileView* view, int pos);
static char * get_viewer_command(const char *viewer);
static void consider_scroll_bind(FileView *view);
static void correct_list_pos_down(FileView *view, size_t pos_delta);
static void correct_list_pos_up(FileView *view, size_t pos_delta);
static int calculate_top_position(FileView *view, int top);
static size_t calculate_print_width(const FileView *view, int i,
		size_t max_width);
static void calculate_table_conf(FileView *view, size_t *count, size_t *width);
size_t calculate_columns_count(FileView *view);
static size_t calculate_column_width(FileView *view);
static size_t get_effective_scroll_offset(const FileView *view);
static void save_selection(FileView *view);
static void free_saved_selection(FileView *view);
TSTATIC int file_is_visible(FileView *view, const char filename[], int is_dir);
static size_t get_filetype_decoration_width(FileType type);
static int populate_dir_list_internal(FileView *view, int reload);
static int is_dir_big(const char path[]);
static void sort_dir_list(int msg, FileView *view);
static void rescue_from_empty_filelist(FileView * view);
static void add_parent_dir(FileView *view);
static void local_filter_finish(FileView *view);
static void update_filtering_lists(FileView *view, int add, int clear);
static int load_unfiltered_list(FileView *const view);
static int get_unfiltered_pos(const FileView *const view, int pos);
static void store_local_filter_position(FileView *const view, int pos);
static int extract_previously_selected_pos(FileView *const view);
static void clear_local_filter_hist_after(FileView *const view, int pos);
static int find_nearest_neighour(const FileView *const view);
static int add_dir_entry(dir_entry_t **list, size_t *list_size,
		const dir_entry_t *entry);
static int file_can_be_displayed(const char directory[], const char filename[]);
static int parent_dir_is_visible(int in_root);

const size_t COLUMN_GAP = 2;

void
init_filelists(void)
{
	columns_set_line_print_func(column_line_print);
	columns_add_column_desc(SORT_BY_NAME, format_name);
	columns_add_column_desc(SORT_BY_INAME, format_name);
	columns_add_column_desc(SORT_BY_SIZE, format_size);

	columns_add_column_desc(SORT_BY_EXTENSION, format_ext);
#ifndef _WIN32
	columns_add_column_desc(SORT_BY_GROUP_ID, format_group);
	columns_add_column_desc(SORT_BY_GROUP_NAME, format_group);
	columns_add_column_desc(SORT_BY_MODE, format_mode);
	columns_add_column_desc(SORT_BY_OWNER_ID, format_owner);
	columns_add_column_desc(SORT_BY_OWNER_NAME, format_owner);
#endif
	columns_add_column_desc(SORT_BY_TIME_ACCESSED, format_time);
	columns_add_column_desc(SORT_BY_TIME_CHANGED, format_time);
	columns_add_column_desc(SORT_BY_TIME_MODIFIED, format_time);
#ifndef _WIN32
	columns_add_column_desc(SORT_BY_PERMISSIONS, format_perms);
#endif

	init_view(&rwin);
	init_view(&lwin);

	curr_view = &lwin;
	other_view = &rwin;
}

/* Print callback for column_view unit. */
static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	int line_color;
	col_attr_t col;
	char print_buf[strlen(buf) + 1];
	size_t width_left;
	size_t trim_pos;

	const column_data_t *cdt = data;
	size_t i = cdt->line;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line];
	checked_wmove(view->win, cdt->current_line, 1 + cdt->column_offset + offset);

	col = view->cs.color[WIN_COLOR];
	if(column_id == SORT_BY_NAME || column_id == SORT_BY_INAME)
	{
		line_color = get_line_color(view, i);
		mix_colors(&col, &view->cs.color[line_color]);
		if(entry->selected)
			mix_colors(&col, &view->cs.color[SELECTED_COLOR]);
		if(cdt->current)
		{
			mix_colors(&col, &view->cs.color[CURR_LINE_COLOR]);
			line_color = CURRENT_COLOR;
		}
		else if(entry->selected)
		{
			line_color = SELECTED_COLOR;
		}
		init_pair(view->color_scheme + line_color, col.fg, col.bg);
	}
	else
	{
		line_color = WIN_COLOR;

		if(entry->selected)
		{
			mix_colors(&col, &view->cs.color[SELECTED_COLOR]);
			line_color = SELECTED_COLOR;
		}

		if(cdt->current)
		{
			mix_colors(&col, &view->cs.color[CURR_LINE_COLOR]);
			line_color = CURRENT_COLOR;
		}
		else
		{
			init_pair(view->color_scheme + line_color, col.fg, col.bg);
		}
	}
	wattron(view->win, COLOR_PAIR(view->color_scheme + line_color) | col.attr);

	strcpy(print_buf, buf);
	width_left = view->window_width - (column_id != FILL_COLUMN_ID) - offset;
	trim_pos = get_normal_utf8_string_widthn(buf, width_left);
	print_buf[trim_pos] = '\0';
	wprint(view->win, print_buf);

	wattroff(view->win, COLOR_PAIR(view->color_scheme + line_color) | col.attr);
}

/* File name format callback for column_view unit. */
static void
format_name(int id, const void *data, size_t buf_len, char *buf)
{
	const column_data_t *cdt = data;
	format_entry_name(cdt->view, cdt->line, buf_len + 1, buf);
}

/* File size format callback for column_view unit. */
static void
format_size(int id, const void *data, size_t buf_len, char *buf)
{
	char str[24] = "";
	const column_data_t *cdt = data;
	uint64_t size = get_file_size_by_entry(cdt->view, cdt->line);
	friendly_size_notation(size, sizeof(str), str);
	snprintf(buf, buf_len + 1, " %s", str);
}

/* File extension format callback for column_view unit. */
static void
format_ext(int id, const void *data, size_t buf_len, char *buf)
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line];
	const char *dot = strrchr(entry->name,  '.');
	snprintf(buf, buf_len + 1, "%s", (dot == NULL) ? "" : (dot + 1));
	chosp(buf);
}

#ifndef _WIN32

/* File group id/name format callback for column_view unit. */
static void
format_group(int id, const void *data, size_t buf_len, char *buf)
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line];
	if(id == SORT_BY_GROUP_NAME)
	{
		struct group *grp_buf;
		if((grp_buf = getgrgid(entry->gid)) != NULL)
		{
			snprintf(buf, buf_len, " %s", grp_buf->gr_name);
			return;
		}
	}

	snprintf(buf, buf_len, " %d", (int)entry->gid);
}

/* File owner id/name format callback for column_view unit. */
static void
format_owner(int id, const void *data, size_t buf_len, char *buf)
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line];
	if(id == SORT_BY_OWNER_NAME)
	{
		struct passwd *pwd_buf;
		if((pwd_buf = getpwuid(entry->uid)) != NULL)
		{
			snprintf(buf, buf_len, " %s", pwd_buf->pw_name);
			return;
		}
	}

	snprintf(buf, buf_len, " %d", (int)entry->uid);
}

/* File mode format callback for column_view unit. */
static void
format_mode(int id, const void *data, size_t buf_len, char *buf)
{
	const column_data_t *cdt = data;
	dir_entry_t *entry = &cdt->view->dir_entry[cdt->line];
	snprintf(buf, buf_len, " %s", get_mode_str(entry->mode));
}

#endif

/* File modification/access/change date format callback for column_view unit. */
static void
format_time(int id, const void *data, size_t buf_len, char *buf)
{
	struct tm *tm_ptr;
	const column_data_t *cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line];

	switch(id)
	{
		case SORT_BY_TIME_MODIFIED:
			tm_ptr = localtime(&entry->mtime);
			break;
		case SORT_BY_TIME_ACCESSED:
			tm_ptr = localtime(&entry->atime);
			break;
		case SORT_BY_TIME_CHANGED:
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

#ifndef _WIN32
/* File permissions mask format callback for column_view unit. */
static void
format_perms(int id, const void *data, size_t buf_len, char buf[])
{
	const column_data_t *cdt = data;
	FileView *view = cdt->view;
	dir_entry_t *entry = &view->dir_entry[cdt->line];
	get_perm_string(buf, buf_len, entry->mode);
}
#endif

/* Loads initial display values into view structure. */
static void
init_view(FileView *view)
{
	view->curr_line = 0;
	view->top_line = 0;
	view->list_rows = 0;
	view->list_pos = 0;
	view->selected_filelist = NULL;
	view->history_num = 0;
	view->history_pos = 0;
	view->color_scheme = 1;

	view->hide_dot = 1;
	view->matches = 0;
	view->columns = columns_create();
	view->view_columns = strdup("");

	reset_view(view);

	init_view_history(view);
	reset_view_sort(view);
}

void
reset_views(void)
{
	reset_view(&lwin);
	reset_view(&rwin);
}

/* Loads some of view parameters that should be restored on configuration
 * reloading (e.g. on :restart command). */
static void
reset_view(FileView *view)
{
	strncpy(view->regexp, "", sizeof(view->regexp));
	view->invert = cfg.filter_inverted_by_default ? 1 : 0;
	view->prev_invert = view->invert;
	view->ls_view = 0;
	view->max_filename_len = 0;
	view->column_count = 1;

	(void)replace_string(&view->prev_name_filter, "");
	reset_filter(&view->name_filter);
	(void)replace_string(&view->prev_auto_filter, "");
	reset_filter(&view->auto_filter);

	(void)replace_string(&view->local_filter.prev, "");
	reset_filter(&view->local_filter.filter);
	view->local_filter.in_progress = 0;
	view->local_filter.saved = NULL;
	view->local_filter.poshist = NULL;
	view->local_filter.poshist_len = 0U;

	view->sort[0] = DEFAULT_SORT_KEY;
	memset(&view->sort[1], NO_SORT_OPTION, sizeof(view->sort) - 1);
}

/* Resets filter to empty state (either initializes or clears it). */
static void
reset_filter(filter_t *filter)
{
	if(filter->raw == NULL)
	{
		filter_init(filter, CASE_SENSATIVE_FILTER);
	}
	else
	{
		filter_clear(filter);
	}
}

/* Allocates memory for view history smartly (handles huge values). */
static void
init_view_history(FileView *view)
{
	if(cfg.history_len == 0)
		return;

	view->history = calloc(cfg.history_len, sizeof(history_t));
	while(view->history == NULL)
	{
		cfg.history_len /= 2;
		view->history = calloc(cfg.history_len, sizeof(history_t));
	}
}

void
load_initial_directory(FileView *view, const char *dir)
{
	if(view->curr_dir[0] == '\0')
		snprintf(view->curr_dir, sizeof(view->curr_dir), "%s", dir);
	else
		dir = view->curr_dir;

	view->dir_entry = calloc(1, sizeof(dir_entry_t));

	view->dir_entry[0].name = strdup("");
	view->dir_entry[0].type = DIRECTORY;

	view->list_rows = 1;
	if(!is_root_dir(view->curr_dir))
		chosp(view->curr_dir);
	(void)change_directory(view, dir);
}

static int
get_line_color(FileView* view, int pos)
{
	switch(view->dir_entry[pos].type)
	{
		case DIRECTORY:
			return DIRECTORY_COLOR;
		case FIFO:
			return FIFO_COLOR;
		case LINK:
			if(is_on_slow_fs(view->curr_dir))
			{
				return LINK_COLOR;
			}
			else
			{
				char full[PATH_MAX];
				snprintf(full, sizeof(full), "%s/%s", view->curr_dir,
						view->dir_entry[pos].name);
				if(get_link_target_abs(full, view->curr_dir, full, sizeof(full)) != 0)
				{
					return BROKEN_LINK_COLOR;
				}
				return path_exists(full) ? LINK_COLOR : BROKEN_LINK_COLOR;
			}
#ifndef _WIN32
		case SOCKET:
			return SOCKET_COLOR;
#endif
		case CHARACTER_DEVICE:
		case BLOCK_DEVICE:
			return DEVICE_COLOR;
		case EXECUTABLE:
			return EXECUTABLE_COLOR;
		default:
			return WIN_COLOR;
	}
}

#ifndef _WIN32
FILE *
use_info_prog(const char *viewer)
{
	pid_t pid;
	int error_pipe[2];
	char *cmd;

	cmd = get_viewer_command(viewer);

	if(pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		free(cmd);
		return NULL;
	}

	if((pid = fork()) == -1)
	{
		show_error_msg("Fork error", "Error forking process");
		free(cmd);
		return NULL;
	}

	if(pid == 0)
	{
		run_from_fork(error_pipe, 0, cmd);
		free(cmd);
		return NULL;
	}
	else
	{
		FILE * f;
		close(error_pipe[1]); /* Close write end of pipe. */
		free(cmd);
		f = fdopen(error_pipe[0], "r");
		if(f == NULL)
			close(error_pipe[0]);
		return f;
	}
}
#else
static FILE *
use_info_prog_internal(const char *viewer, int out_pipe[2])
{
	char *cmd;
	char *args[4];
	int retcode;

	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
		return NULL;
	if(_dup2(out_pipe[1], _fileno(stderr)) != 0)
		return NULL;

	cmd = get_viewer_command(viewer);

	args[0] = "cmd";
	args[1] = "/C";
	args[2] = cmd;
	args[3] = NULL;

	retcode = _spawnvp(P_NOWAIT, args[0], (const char **)args);
	free(cmd);

	return (retcode == 0) ? NULL : _fdopen(out_pipe[0], "r");
}

FILE *
use_info_prog(const char *viewer)
{
	int out_fd, err_fd;
	int out_pipe[2];
	FILE *result;

	if(_pipe(out_pipe, 512, O_NOINHERIT) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		return NULL;
	}

	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	result = use_info_prog_internal(viewer, out_pipe);

	_dup2(out_fd, _fileno(stdout));
	_dup2(err_fd, _fileno(stderr));

	if(result == NULL)
		close(out_pipe[0]);
	close(out_pipe[1]);

	return result;
}
#endif

/* Returns a pointer to newly allocated memory, which should be released by the
 * caller. */
static char *
get_viewer_command(const char *viewer)
{
	char *result;
	if(strchr(viewer, '%') == NULL)
	{
		char *const escaped = escape_filename(get_current_file_name(curr_view), 0);
		result = format_str("%s %s", viewer, escaped);
		free(escaped);
	}
	else
	{
		result = expand_macros(viewer, NULL, NULL, 1);
	}
	return result;
}

char *
get_current_file_name(FileView *view)
{
	if(view->list_pos == -1)
		return "";
	return view->dir_entry[view->list_pos].name;
}

void
free_selected_file_array(FileView *view)
{
	if(view->selected_filelist != NULL)
	{
		free_string_array(view->selected_filelist, view->selected_files);
		view->selected_filelist = NULL;
	}
}

/* If you use this function using the free_selected_file_array()
 * will clean up the allocated memory
 */
void
get_all_selected_files(FileView *view)
{
	int x;
	int y;

	count_selected(view);

	/* No selected files so just use the current file */
	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
		view->user_selection = 0;
	}

	if(view->selected_filelist != NULL)
	{
		free_selected_file_array(view);
		/* setting this because free_selected_file_array doesn't do it */
		view->selected_files = 0;
	}
	count_selected(view);
	view->selected_filelist = calloc(view->selected_files, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < view->list_rows; x++)
	{
		if(!view->dir_entry[x].selected)
			continue;
		if(is_parent_dir(view->dir_entry[x].name))
		{
			view->dir_entry[x].selected = 0;
			continue;
		}

		view->selected_filelist[y] = strdup(view->dir_entry[x].name);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		y++;
	}
	view->selected_files = y;
}

/* If you use this function using the free_selected_file_array()
 * will clean up the allocated memory
 */
void
get_selected_files(FileView *view, int count, const int *indexes)
{
	int x;
	int y = 0;

	if(view->selected_filelist != NULL)
		free_selected_file_array(view);
	view->selected_filelist = (char **)calloc(count, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < count; x++)
	{
		if(is_parent_dir(view->dir_entry[indexes[x]].name))
			continue;

		view->selected_filelist[y] = strdup(view->dir_entry[indexes[x]].name);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		y++;
	}

	view->selected_files = y;
}

void
count_selected(FileView *view)
{
	int i;
	view->selected_files = 0;
	for(i = 0; i < view->list_rows; i++)
		view->selected_files += (view->dir_entry[i].selected != 0);
}

int
find_file_pos_in_list(const FileView *const view, const char file[])
{
	int i;
	for(i = 0; i < view->list_rows; i++)
	{
		if(stroscmp(view->dir_entry[i].name, file) == 0)
		{
			return i;
		}
	}
	return -1;
}

void
update_view_title(FileView *view)
{
	char *buf;
	size_t len;
	int gen_view = get_mode() == VIEW_MODE && !curr_view->explore_mode;
	FileView *selected = gen_view ? other_view : curr_view;

	if(gen_view && view == other_view)
		return;

	if(curr_stats.load_stage < 2)
		return;

	if(view == selected)
	{
		col_attr_t col;

		col = cfg.cs.color[TOP_LINE_COLOR];
		mix_colors(&col, &cfg.cs.color[TOP_LINE_SEL_COLOR]);
		init_pair(DCOLOR_BASE + TOP_LINE_SEL_COLOR, col.fg, col.bg);

		wbkgdset(view->title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_SEL_COLOR) |
				(col.attr & A_REVERSE));
		wattrset(view->title, col.attr & ~A_REVERSE);
	}
	else
	{
		wbkgdset(view->title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
				(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
		wattrset(view->title, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
		wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
				(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
		wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
		werase(top_line);
	}
	werase(view->title);

	if(curr_stats.load_stage < 2)
		return;

	buf = replace_home_part(view->curr_dir);
	if(view == selected)
	{
		set_term_title(replace_home_part(view->curr_dir));
	}

	if(view->explore_mode)
	{
		if(!is_root_dir(buf))
			strcat(buf, "/");
		strcat(buf, get_current_file_name(view));
	}

	len = get_screen_string_length(buf);
	if(len > view->window_width + 1 && view == selected)
	{ /* Truncate long directory names */
		const char *ptr;

		ptr = buf;
		while(len > view->window_width - 2)
		{
			len--;
			ptr += get_char_width(ptr);
		}

		wprintw(view->title, "...");
		wprint(view->title, ptr);
	}
	else if(len > view->window_width + 1 && view != selected)
	{
		size_t len = get_normal_utf8_string_widthn(buf, view->window_width - 3 + 1);
		buf[len] = '\0';
		wprint(view->title, buf);
		wprintw(view->title, "...");
	}
	else
	{
		wprint(view->title, buf);
	}

	wnoutrefresh(view->title);
}

void
reset_view_sort(FileView *view)
{
	if(view->view_columns[0] == '\0')
	{
		column_info_t column_info =
		{
			.column_id = SORT_BY_NAME, .full_width = 0UL, .text_width = 0UL,
			.align = AT_LEFT,          .sizing = ST_AUTO, .cropping = CT_NONE,
		};

		columns_clear(view->columns);
		columns_add_column(view->columns, column_info);

		column_info.column_id = get_secondary_key(abs(view->sort[0]));
		column_info.align = AT_RIGHT;
		columns_add_column(view->columns, column_info);
	}
	else if(strstr(view->view_columns, "{}") != NULL)
	{
		load_view_columns_option(view, view->view_columns);
	}
}

void
invert_sorting_order(FileView *view)
{
	view->sort[0] = -view->sort[0];
}

void
draw_dir_list(FileView *view)
{
	int attr;
	int x;
	int y;
	size_t col_width;
	size_t col_count;
	int top = view->top_line;

	if(curr_stats.load_stage < 2)
		return;

	calculate_table_conf(view, &col_count, &col_width);

	if(top + view->window_rows > view->list_rows)
		top = view->list_rows - view->window_rows;
	if(top < 0)
		top = 0;

	update_view_title(view);

	/* This is needed for reloading a list that has had files deleted */
	while((view->list_rows - view->list_pos) <= 0)
	{
		view->list_pos--;
		view->curr_line--;
	}

	top = calculate_top_position(view, top);

	/* Colorize the files */

	if(view->color_scheme == DCOLOR_BASE)
		attr = cfg.cs.color[WIN_COLOR].attr;
	else
		attr = view->cs.color[WIN_COLOR].attr;
	wbkgdset(view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme) | attr);
	werase(view->win);

	y = 0;
	for(x = top; x < view->list_rows; x++)
	{
		checked_wmove(view->win, y, 1);
		wclrtoeol(view->win);
		column_data_t cdt = {view, x, 0, y/col_count, (y%col_count)*col_width};
		columns_format_line(view->columns, &cdt, col_width);
		y++;
		if(y >= view->window_cells)
			break;
	}

	view->top_line = top;
	view->curr_line = view->list_pos - view->top_line;

	if(view != curr_view)
	{
		put_inactive_mark(view);
	}

	if(view == curr_view)
	{
		consider_scroll_bind(view);
	}

	ui_view_win_changed(view);
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
update_scroll_bind_offset(void)
{
	int rwin_pos = rwin.top_line/rwin.column_count;
	int lwin_pos = lwin.top_line/lwin.column_count;
	curr_stats.scroll_bind_off = rwin_pos - lwin_pos;
}

void
correct_list_pos(FileView *view, ssize_t pos_delta)
{
	if(pos_delta > 0)
	{
		correct_list_pos_down(view, pos_delta);
	}
	else if(pos_delta < 0)
	{
		correct_list_pos_up(view, -pos_delta);
	}
}

int
correct_list_pos_on_scroll_down(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_down(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

/* Tries to move cursor forwards by pos_delta positions. */
static void
correct_list_pos_down(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_down(view, pos_delta);
}

int
get_corrected_list_pos_down(const FileView *view, size_t pos_delta)
{
	int scroll_offset = get_effective_scroll_offset(view);
	if(view->list_pos <= view->top_line + scroll_offset + (MAX(pos_delta, 1) - 1))
	{
		size_t column_correction = view->list_pos%view->column_count;
		size_t offset = scroll_offset + pos_delta + column_correction;
		return view->top_line + offset;
	}
	return view->list_pos;
}

int
correct_list_pos_on_scroll_up(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_up(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

/* Tries to move cursor backwards by pos_delta positions. */
static void
correct_list_pos_up(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_up(view, pos_delta);
}

int
get_corrected_list_pos_up(const FileView *view, size_t pos_delta)
{
	int scroll_offset = get_effective_scroll_offset(view);
	int last = get_last_visible_file(view);
	if(view->list_pos >= last - scroll_offset - (MAX((int)pos_delta, 1) - 1))
	{
		size_t column_correction = (view->column_count - 1) -
				view->list_pos%view->column_count;
		size_t offset = scroll_offset + pos_delta + column_correction;
		return last - offset;
	}
	return view->list_pos;
}

int
all_files_visible(const FileView *view)
{
	return view->list_rows <= view->window_cells;
}

void
erase_current_line_bar(FileView *view)
{
	int old_cursor = view->curr_line;
	int old_pos = view->top_line + old_cursor;
	size_t col_width;
	size_t col_count;
	size_t print_width;
	column_data_t cdt = {view, old_pos, 0};

	if(old_cursor < 0)
		return;

	if(curr_stats.load_stage < 2)
		return;

	if(old_pos < 0 || old_pos >= view->list_rows)
	{
		/* The entire list is going to be redrawn so just return. */
		return;
	}

	calculate_table_conf(view, &col_count, &col_width);
	print_width = calculate_print_width(view, old_pos, col_width);

	cdt.current_line = old_cursor/col_count;
	cdt.column_offset = (old_cursor%col_count)*col_width;

	column_line_print(&cdt, FILL_COLUMN_ID, " ", -1);
	columns_format_line(view->columns, &cdt, col_width);
	column_line_print(&cdt, FILL_COLUMN_ID, " ", print_width);

	if(view == other_view)
	{
		put_inactive_mark(view);
	}
}

int
move_curr_line(FileView *view)
{
	int redraw = 0;
	int pos = view->list_pos;
	size_t last;

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

	last = get_last_visible_file(view);
	if(view->top_line <= pos && pos <= last)
	{
		view->curr_line = pos - view->top_line;
	}
	else if(pos > last)
	{
		scroll_down(view, pos - last);
		redraw = 1;
	}
	else if(pos < view->top_line)
	{
		scroll_up(view, view->top_line - pos);
		redraw = 1;
	}

	redraw = consider_scroll_offset(view) ? 1 : redraw;

	return redraw;
}

/* Calculates top position basing on window and list size and trying to show as
 * much of the directory as possible.  Can modify view->curr_line.  Returns
 * new top. */
static int
calculate_top_position(FileView *view, int top)
{
	int result = MIN(MAX(top, 0), view->list_rows - 1);
	result = ROUND_DOWN(result, view->column_count);
	if(view->window_cells >= view->list_rows)
	{
		result = 0;
	}
	else if(view->list_rows - top < view->window_cells)
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

void
move_to_list_pos(FileView *view, int pos)
{
	int redraw = 0;
	size_t col_width;
	size_t col_count;
	size_t print_width;
	column_data_t cdt = {view, 0, 1, 0};

	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(pos == -1)
		return;

	if(view->curr_line > view->list_rows - 1)
		view->curr_line = view->list_rows - 1;

	view->list_pos = pos;

	erase_current_line_bar(view);

	redraw = move_curr_line(view);

	if(curr_stats.load_stage < 2)
		return;

	if(redraw)
		draw_dir_list(view);

	calculate_table_conf(view, &col_count, &col_width);
	print_width = calculate_print_width(view, view->list_pos, col_width);

	cdt.line = pos;
	cdt.current_line = view->curr_line/col_count;
	cdt.column_offset = (view->curr_line%col_count)*col_width;

	column_line_print(&cdt, FILL_COLUMN_ID, " ", -1);
	columns_format_line(view->columns, &cdt, print_width);
	column_line_print(&cdt, FILL_COLUMN_ID, " ", print_width);

	refresh_view_win(view);
	update_stat_window(view);

	if(curr_stats.view)
		quick_view_file(view);
}

/* Calculates width of the column using entry and maximum width. */
static size_t
calculate_print_width(const FileView *view, int i, size_t max_width)
{
	if(view->ls_view)
	{
		const dir_entry_t *old_entry = &view->dir_entry[i];
		size_t old_name_width = strlen(old_entry->name);
		old_name_width += get_filetype_decoration_width(old_entry->type);
		/* FIXME: remove this hack for directories. */
		if(old_entry->type == DIRECTORY)
		{
			old_name_width--;
		}
		return MIN(max_width - 1, old_name_width);
	}
	else
	{
		return max_width;
	}
}

void
put_inactive_mark(FileView *view)
{
	size_t col_width;
	size_t col_count;

	calculate_table_conf(view, &col_count, &col_width);

	mvwaddstr(view->win, view->curr_line/col_count,
			(view->curr_line%col_count)*col_width, "*");
}

/* Calculates number of columns and maximum width of column in a view. */
static void
calculate_table_conf(FileView *view, size_t *count, size_t *width)
{
	*width = view->window_width - 1;
	*count = 1;

	if(view->ls_view)
	{
		*count = calculate_columns_count(view);
		*width = calculate_column_width(view);
	}
	view->column_count = *count;
	view->window_cells = *count*(view->window_rows + 1);
}

size_t
calculate_columns_count(FileView *view)
{
	if(view->ls_view)
	{
		size_t column_width = calculate_column_width(view);
		return (view->window_width - 1)/column_width;
	}
	else
	{
		return 1;
	}
}

/* Returns width of one column in the view. */
static size_t
calculate_column_width(FileView *view)
{
	return MIN(view->max_filename_len + COLUMN_GAP, view->window_width - 1);
}

void
goto_history_pos(FileView *view, int pos)
{
	curr_stats.skip_history = 1;
	if(change_directory(view, view->history[pos].dir) < 0)
	{
		curr_stats.skip_history = 0;
		return;
	}
	curr_stats.skip_history = 0;

	load_dir_list(view, 1);
	move_to_list_pos(view, find_file_pos_in_list(view, view->history[pos].file));

	view->history_pos = pos;
}

void
clean_positions_in_history(FileView *view)
{
	int i;
	for(i = 0; i <= view->history_pos; i++)
		view->history[i].file[0] = '\0';
}

void
save_view_history(FileView *view, const char *path, const char *file, int pos)
{
	int x;

	/* this could happen on FUSE error */
	if(view->list_rows <= 0)
		return;

	if(cfg.history_len <= 0)
		return;

	if(path == NULL)
		path = view->curr_dir;
	if(file == NULL)
		file = view->dir_entry[view->list_pos].name;
	if(pos < 0)
		pos = view->list_pos;

	if(view->history_num > 0 &&
			stroscmp(view->history[view->history_pos].dir, path) == 0)
	{
		if(curr_stats.load_stage < 2 || file[0] == '\0')
			return;
		x = view->history_pos;
		(void)replace_string(&view->history[x].file, file);
		view->history[x].rel_pos = pos - view->top_line;
		return;
	}

	if(curr_stats.skip_history)
		return;

	if(view->history_num > 0 && view->history_pos != view->history_num - 1)
	{
		x = view->history_num - 1;
		while(x > view->history_pos)
			view->history[x--].dir[0] = '\0';
		view->history_num = view->history_pos + 1;
	}
	x = view->history_num;

	if(x == cfg.history_len)
	{
		free_history_items(view->history, 1);
		memmove(view->history, view->history + 1,
				sizeof(history_t)*(cfg.history_len - 1));

		x--;
		view->history_num = x;
	}
	view->history[x].dir = strdup(path);
	view->history[x].file = strdup(file);
	view->history[x].rel_pos = pos - view->top_line;
	view->history_num++;
	view->history_pos = view->history_num - 1;
}

int
is_in_view_history(FileView *view, const char *path)
{
	int i;
	if(view->history == NULL)
		return 0;
	for(i = view->history_pos; i >= 0; i--)
	{
		if(strlen(view->history[i].dir) < 1)
			break;
		if(stroscmp(view->history[i].dir, path) == 0)
			return 1;
	}
	return 0;
}

static void
check_view_dir_history(FileView *view)
{
	int pos = 0;
	int rel_pos = -1;

	if(cfg.history_len > 0 && view->history_num > 0 && curr_stats.ch_pos)
	{
		int x;
		int found = 0;
		x = view->history_pos;
		if(stroscmp(view->history[x].dir, view->curr_dir) == 0 &&
				view->history[x].file[0] == '\0')
			x--;
		for(; x >= 0; x--)
		{
			if(strlen(view->history[x].dir) < 1)
				break;
			if(stroscmp(view->history[x].dir, view->curr_dir) == 0)
			{
				found = 1;
				break;
			}
		}
		if(found)
		{
			pos = find_file_pos_in_list(view, view->history[x].file);
			rel_pos = view->history[x].rel_pos;
		}
		else if(path_starts_with(view->last_dir, view->curr_dir) &&
				stroscmp(view->last_dir, view->curr_dir) != 0 &&
				strchr(view->last_dir + strlen(view->curr_dir) + 1, '/') == NULL)
		{
			char buf[NAME_MAX];
			snprintf(buf, sizeof(buf), "%s/",
					view->last_dir + strlen(view->curr_dir));

			pos = find_file_pos_in_list(view, buf);
			rel_pos = -1;
		}
		else
		{
			view->list_pos = 0;
			view->curr_line = 0;
			view->top_line = 0;
			return;
		}
	}

	if(pos < 0)
		pos = 0;
	view->list_pos = pos;
	if(rel_pos >= 0)
	{
		view->top_line = pos - MIN(view->window_cells - 1, rel_pos);
		if(view->top_line < 0)
			view->top_line = 0;
		view->curr_line = pos - view->top_line;
	}
	else
	{
		size_t last = get_last_visible_file(view);
		if(view->list_pos < view->window_cells)
		{
			scroll_up(view, view->top_line);
		}
		else if(view->list_pos > last)
		{
			scroll_down(view, view->list_pos - last);
		}
	}
	(void)consider_scroll_offset(view);
}

int
consider_scroll_offset(FileView *view)
{
	int need_redraw = 0;
	int pos = view->list_pos;
	if(cfg.scroll_off > 0)
	{
		size_t s = get_effective_scroll_offset(view);
		/* Check scroll offset at the top. */
		if(can_scroll_up(view) && pos - view->top_line < s)
		{
			scroll_up(view, s - (pos - view->top_line));
			need_redraw = 1;
		}
		/* Check scroll offset at the bottom. */
		if(can_scroll_down(view))
		{
			size_t last = get_last_visible_file(view);
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

int
can_scroll_up(const FileView *view)
{
	return view->top_line > 0;
}

int
can_scroll_down(const FileView *view)
{
	return get_last_visible_file(view) < view->list_rows - 1;
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
at_first_line(const FileView *view)
{
	return view->list_pos/view->column_count == 0;
}

int
at_last_line(const FileView *view)
{
	size_t col_count = view->column_count;
	return view->list_pos/col_count == (view->list_rows - 1)/col_count;
}

int
at_first_column(const FileView *view)
{
	return view->list_pos%view->column_count == 0;
}

int
at_last_column(const FileView *view)
{
	return view->list_pos%view->column_count == view->column_count - 1;
}

size_t
get_window_top_pos(const FileView *view)
{
	if(view->top_line == 0)
	{
		return 0;
	}
	else
	{
		return view->top_line + get_effective_scroll_offset(view);
	}
}

size_t
get_window_middle_pos(const FileView *view)
{
	int list_middle = view->list_rows/(2*view->column_count);
	int window_middle = view->window_rows/2;
	return view->top_line + MIN(list_middle, window_middle)*view->column_count;
}

size_t
get_window_bottom_pos(const FileView *view)
{
	if(all_files_visible(view))
	{
		size_t last = view->list_rows - 1;
		return last - last%view->column_count;
	}
	else
	{
		size_t off = get_effective_scroll_offset(view);
		size_t column_correction = view->column_count - 1;
		return get_last_visible_file(view) - off - column_correction;
	}
}

size_t
get_last_visible_file(const FileView *view)
{
	return view->top_line + view->window_cells - 1;
}

void
go_to_start_of_line(FileView *view)
{
	view->list_pos = get_start_of_line(view);
}

int
get_start_of_line(const FileView *view)
{
	int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	return ROUND_DOWN(pos, view->column_count);
}

int
get_end_of_line(const FileView *view)
{
	int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	pos += (view->column_count - 1) - pos%view->column_count;
	return MIN(pos, view->list_rows - 1);
}

void
clean_selected_files(FileView *view)
{
	save_selection(view);
	erase_selection(view);
}

/* Saves list of selected files if any. */
static void
save_selection(FileView *view)
{
	if(view->selected_files != 0)
	{
		char **save_selected_filelist;

		free_string_array(view->saved_selection, view->nsaved_selection);

		save_selected_filelist = view->selected_filelist;
		view->selected_filelist = NULL;

		get_all_selected_files(view);
		view->nsaved_selection = view->selected_files;
		view->saved_selection = view->selected_filelist;

		view->selected_filelist = save_selected_filelist;
	}
}

void
erase_selection(FileView *view)
{
	int x;

	/* This is needed, since otherwise we loose number of items in the array,
	 * which can cause access violation of memory leaks. */
	free_selected_file_array(view);

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;
	view->selected_files = 0;
}

void
invert_selection(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; i++)
	{
		dir_entry_t *const e = &view->dir_entry[i];
		if(!is_parent_dir(e->name))
		{
			e->selected = !e->selected;
		}
	}
}

void
leave_invalid_dir(FileView *view)
{
	char *const path = view->curr_dir;

	if(try_updir_from_fuse_mount(path, view))
	{
		return;
	}

	while(!directory_accessible(path) && is_path_well_formed(path))
	{
		if(try_updir_from_fuse_mount(path, view))
		{
			break;
		}

		remove_last_path_component(path);
	}

	ensure_path_well_formed(path);
}

#ifdef _WIN32
static int
check_dir_changed(FileView *view)
{
	char buf[PATH_MAX];
	HANDLE hfile;
	FILETIME ft;
	int r;

	if(stroscmp(view->watched_dir, view->curr_dir) != 0)
	{
		FindCloseChangeNotification(view->dir_watcher);
		strcpy(view->watched_dir, view->curr_dir);
		view->dir_watcher = FindFirstChangeNotificationA(view->curr_dir, 1,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY);
		if(view->dir_watcher == NULL || view->dir_watcher == INVALID_HANDLE_VALUE)
			log_msg("ha%s", "d");
	}
 
	if(WaitForSingleObject(view->dir_watcher, 0) == WAIT_OBJECT_0)
	{
		FindNextChangeNotification(view->dir_watcher);
		return 1;
	}

	snprintf(buf, sizeof(buf), "%s/.", view->curr_dir);

	hfile = CreateFileA(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return -1;

	if(!GetFileTime(hfile, NULL, NULL, &ft))
	{
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);

	r = CompareFileTime(&view->dir_mtime, &ft);
	view->dir_mtime = ft;

	return r != 0;
}
#endif

static int
update_dir_mtime(FileView *view)
{
#ifndef _WIN32
	struct stat s;

	if(stat(view->curr_dir, &s) != 0)
		return -1;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	view->dir_mtime = s.st_mtim;
#else
	view->dir_mtime = s.st_mtime;
#endif
	return 0;
#else
	char buf[PATH_MAX];
	HANDLE hfile;

	snprintf(buf, sizeof(buf), "%s/.", view->curr_dir);

	hfile = CreateFileA(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return -1;

	if(!GetFileTime(hfile, NULL, NULL, &view->dir_mtime))
	{
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);

	while(check_dir_changed(view) > 0);

	return 0;
#endif
}

#ifdef _WIN32
static const char *
handle_mount_points(const char *path)
{
	static char buf[PATH_MAX];

	DWORD attr;
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	REPARSE_DATA_BUFFER *rdbp;

	attr = GetFileAttributes(path);
	if(attr == INVALID_FILE_ATTRIBUTES)
		return path;

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return path;

	snprintf(buf, sizeof(buf), "%s", path);
	chosp(buf);
	hfind = FindFirstFileA(buf, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
		return path;

	FindClose(hfind);

	if(ffd.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT)
		return path;

	hfile = CreateFileA(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return path;

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb,
				sizeof(rdb), &attr, NULL))
	{
		CloseHandle(hfile);
		return path;
	}
	CloseHandle(hfile);
	
	rdbp = (REPARSE_DATA_BUFFER *)rdb;
	t = to_multibyte(rdbp->MountPointReparseBuffer.PathBuffer);
	if(strncmp(t, "\\??\\", 4) == 0)
		strcpy(buf, t + 4);
	else
		strcpy(buf, t);
	free(t);
	return buf;
}
#endif

void
navigate_to(FileView *view, const char path[])
{
	if(change_directory(view, path) >= 0)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, view->list_pos);
	}
}

/*
 * The directory can either be relative to the current
 * directory - ../
 * or an absolute path - /usr/local/share
 * The *directory passed to change_directory() cannot be modified.
 * Symlink directories require an absolute path
 *
 * Return value:
 *  -1  if there were errors.
 *   0  if directory successfully changed and we didn't leave FUSE mount
 *      directory.
 *   1  if directory successfully changed and we left FUSE mount directory.
 */
int
change_directory(FileView *view, const char *directory)
{
	char newdir[PATH_MAX];
	char dir_dup[PATH_MAX];

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, NULL, -1);
	}

#ifdef _WIN32
	directory = handle_mount_points(directory);
#endif

	if(is_path_absolute(directory))
	{
		canonicalize_path(directory, dir_dup, sizeof(dir_dup));
	}
	else
	{
#ifdef _WIN32
		if(directory[0] == '/')
			snprintf(newdir, sizeof(newdir), "%c:%s", view->curr_dir[0], directory);
		else
#endif
			snprintf(newdir, sizeof(newdir), "%s/%s", view->curr_dir, directory);
		canonicalize_path(newdir, dir_dup, sizeof(dir_dup));
	}

#ifdef _WIN32
	to_forward_slash(dir_dup);
#endif

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	if(!is_valid_dir(dir_dup))
	{
		show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);
		snprintf(view->curr_dir, sizeof(view->curr_dir), "%s", dir_dup);
		leave_invalid_dir(view);
		snprintf(dir_dup, sizeof(dir_dup), "%s", view->curr_dir);
	}

	snprintf(view->last_dir, sizeof(view->last_dir), "%s", view->curr_dir);

	/* Check if we're exiting from a FUSE mounted top level directory and the
	 * other pane isn't in it or any of it subdirectories.
	 * If so, unmount & let FUSE serialize */
	if(is_parent_dir(directory) && in_mounted_dir(view->curr_dir))
	{
		FileView *other = (view == curr_view) ? other_view : curr_view;
		if(!path_starts_with(other->curr_dir, view->curr_dir))
		{
			int r = try_unmount_fuse(view);
			if(r != 0)
				return r;
		}
		else if(try_updir_from_fuse_mount(view->curr_dir, view))
		{
			return 1;
		}
	}

	/* Clean up any excess separators */
	if(!is_root_dir(view->curr_dir))
		chosp(view->curr_dir);
	if(!is_root_dir(view->last_dir))
		chosp(view->last_dir);

#ifndef _WIN32
	if(!path_exists(dir_dup))
#else
	if(!is_valid_dir(dir_dup))
#endif
	{
		LOG_SERROR_MSG(errno, "Can't access \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);

		clean_selected_files(view);
		return -1;
	}

	if(access(dir_dup, X_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Directory Access Error",
				"You do not have execute access on %s", dir_dup);

		clean_selected_files(view);
		return -1;
	}

	if(access(dir_dup, R_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, R_OK) \"%s\"", dir_dup);
		log_cwd();

		if(stroscmp(view->curr_dir, dir_dup) != 0)
		{
			show_error_msgf("Directory Access Error",
					"You do not have read access on %s", dir_dup);
		}
	}

	if(my_chdir(dir_dup) == -1 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Change Directory Error", "Couldn't open %s", dir_dup);
		return -1;
	}

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	if(stroscmp(dir_dup, view->curr_dir) != 0)
	{
		filter_clear(&view->local_filter.filter);
		free_saved_selection(view);
	}
	else
	{
		save_selection(view);
	}
	erase_selection(view);

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", dir_dup);

	snprintf(view->curr_dir, sizeof(view->curr_dir), "%s", dir_dup);

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, "", -1);
	}
	return 0;
}

int
is_dir_list_loaded(FileView *view)
{
	dir_entry_t *entry = &view->dir_entry[0];
	return entry != NULL && entry->name[0] != '\0';
}

/* Frees list of previously selected files. */
static void
free_saved_selection(FileView *view)
{
	free_string_array(view->saved_selection, view->nsaved_selection);
	view->nsaved_selection = 0;
	view->saved_selection = NULL;
}

static void
reset_selected_files(FileView *view, int need_free)
{
	int x;
	for(x = 0; x < view->selected_files; x++)
	{
		if(view->selected_filelist[x] != NULL)
		{
			int pos = find_file_pos_in_list(view, view->selected_filelist[x]);
			if(pos >= 0 && pos < view->list_rows)
				view->dir_entry[pos].selected = 1;
		}
	}

	if(need_free)
	{
		x = view->selected_files;
		free_selected_file_array(view);
		view->selected_files = x;
	}
}

#ifdef _WIN32
static void
fill_with_shared(FileView *view)
{
	NET_API_STATUS res;
	wchar_t *wserver;
	
	wserver = to_wide(view->curr_dir + 2);
	if(wserver == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	view->list_rows = 0;
	do
	{
		PSHARE_INFO_0 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;

		res = NetShareEnum(wserver, 0, (LPBYTE *)&buf_ptr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			PSHARE_INFO_0 p;
			DWORD i;

			p = buf_ptr;
			for(i = 1; i <= er; i++)
			{
				char buf[512];
				WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)p->shi0_netname, -1, buf,
						sizeof(buf), NULL, NULL);


					dir_entry_t *dir_entry;

					view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
							(view->list_rows + 1)*sizeof(dir_entry_t));
					if(view->dir_entry == NULL)
					{
						show_error_msg("Memory Error", "Unable to allocate enough memory");
						p++;
						continue;
					}

					dir_entry = view->dir_entry + view->list_rows;

					/* Allocate extra for adding / to directories. */
					dir_entry->name = malloc(strlen(buf) + 1 + 1);
					if(dir_entry->name == NULL)
					{
						show_error_msg("Memory Error", "Unable to allocate enough memory");
						p++;
						continue;
					}

					strcpy(dir_entry->name, buf);

					/* All files start as unselected and unmatched */
					dir_entry->selected = 0;
					dir_entry->search_match = 0;

					dir_entry->size = 0;
					dir_entry->attrs = 0;
					dir_entry->mtime = 0;
					dir_entry->atime = 0;
					dir_entry->ctime = 0;

					strcat(dir_entry->name, "/");
					dir_entry->type = DIRECTORY;
					view->list_rows++;

				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);

	free(wserver);
}

static int
is_win_symlink(DWORD attr, DWORD tag)
{
	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return 0;

	return (tag == IO_REPARSE_TAG_SYMLINK);
}

static time_t
win_to_unix_time(FILETIME ft)
{
	const uint64_t WINDOWS_TICK = 10000000;
	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;
	uint64_t win_time;

	win_time = ft.dwHighDateTime;
	win_time = (win_time << 32) | ft.dwLowDateTime;

	return win_time/WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}
#endif

static int
fill_dir_list(FileView *view)
{
	int with_parent_dir = 0;
	const int is_root = is_root_dir(view->curr_dir);

	view->matches = 0;
	view->max_filename_len = 0;

#ifndef _WIN32
	DIR *dir;
	struct dirent *d;

	if((dir = opendir(view->curr_dir)) == NULL)
		return -1;

	for(view->list_rows = 0; (d = readdir(dir)); view->list_rows++)
	{
		dir_entry_t *dir_entry;
		size_t name_len;
		struct stat s;

		/* Ignore the "." directory. */
		if(stroscmp(d->d_name, ".") == 0)
		{
			view->list_rows--;
			continue;
		}
		if(stroscmp(d->d_name, "..") == 0)
		{
			if(!parent_dir_is_visible(is_root))
			{
				view->list_rows--;
				continue;
			}
			with_parent_dir = 1;
		}
		else if(!file_is_visible(view, d->d_name, d->d_type == DT_DIR))
		{
			view->filtered++;
			view->list_rows--;
			continue;
		}
		else if(view->hide_dot && d->d_name[0] == '.')
		{
			view->filtered++;
			view->list_rows--;
			continue;
		}

		view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
				(view->list_rows + 1) * sizeof(dir_entry_t));
		if(view->dir_entry == NULL)
		{
			closedir(dir);
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			return -1;
		}

		dir_entry = view->dir_entry + view->list_rows;

		name_len = strlen(d->d_name);
		/* Allocate extra for adding / to directories. */
		dir_entry->name = malloc(name_len + 1 + 1);
		if(dir_entry->name == NULL)
		{
			closedir(dir);
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			return -1;
		}

		strcpy(dir_entry->name, d->d_name);

		/* All files start as unselected and unmatched */
		dir_entry->selected = 0;
		dir_entry->search_match = 0;

		/* Load the inode info */
		if(lstat(dir_entry->name, &s) != 0)
		{
			LOG_SERROR_MSG(errno, "Can't lstat() \"%s/%s\"", view->curr_dir,
					dir_entry->name);
			log_cwd();

			dir_entry->type = type_from_dir_entry(d);
			if(dir_entry->type == DIRECTORY)
			{
				strcat(dir_entry->name, "/");
				name_len++;
			}
			dir_entry->size = 0;
			dir_entry->mode = 0;
			dir_entry->uid = -1;
			dir_entry->gid = -1;
			dir_entry->mtime = 0;
			dir_entry->atime = 0;
			dir_entry->ctime = 0;

			name_len += get_filetype_decoration_width(dir_entry->type);
			view->max_filename_len = MAX(view->max_filename_len, name_len);
			continue;
		}

		dir_entry->size = (uintmax_t)s.st_size;
		dir_entry->mode = s.st_mode;
		dir_entry->uid = s.st_uid;
		dir_entry->gid = s.st_gid;
		dir_entry->mtime = s.st_mtime;
		dir_entry->atime = s.st_atime;
		dir_entry->ctime = s.st_ctime;

		if(s.st_ino)
		{
			dir_entry->type = get_type_from_mode(s.st_mode);
			if(dir_entry->type == LINK)
			{
				struct stat st;
				if(check_link_is_dir(dir_entry->name))
					strcat(dir_entry->name, "/");
				if(stat(dir_entry->name, &st) == 0)
					dir_entry->mode = st.st_mode;
			}
			else if(dir_entry->type == DIRECTORY)
			{
					strcat(dir_entry->name, "/");
					name_len++;
			}
			name_len += get_filetype_decoration_width(dir_entry->type);
			view->max_filename_len = MAX(view->max_filename_len, name_len);
		}
	}
	closedir(dir);
#else
	char buf[PATH_MAX];
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;

	if(is_unc_root(view->curr_dir))
	{
		fill_with_shared(view);
		return 0;
	}

	snprintf(buf, sizeof(buf), "%s/*", view->curr_dir);

	view->list_rows = 0;

	hfind = FindFirstFileA(buf, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
		return -1;

	do
	{
		dir_entry_t *dir_entry;
		size_t name_len;

		/* Ignore the "." directory. */
		if(stroscmp(ffd.cFileName, ".") == 0)
			continue;
		/* Always include the ../ directory unless it is the root directory. */
		if(stroscmp(ffd.cFileName, "..") == 0)
		{
			if(!parent_dir_is_visible(is_root))
			{
				continue;
			}
			with_parent_dir = 1;
		}
		else if(!file_is_visible(view, ffd.cFileName,
				ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			view->filtered++;
			continue;
		}
		else if(view->hide_dot && ffd.cFileName[0] == '.')
		{
			view->filtered++;
			continue;
		}

		view->dir_entry = realloc(view->dir_entry,
				(view->list_rows + 1) * sizeof(dir_entry_t));
		if(view->dir_entry == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			FindClose(hfind);
			return -1;
		}

		dir_entry = view->dir_entry + view->list_rows;

		name_len = strlen(ffd.cFileName);
		/* Allocate extra for adding / to directories. */
		dir_entry->name = malloc(name_len + 1 + 1);
		if(dir_entry->name == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			FindClose(hfind);
			return -1;
		}

		strcpy(dir_entry->name, ffd.cFileName);

		/* All files start as unselected and unmatched */
		dir_entry->selected = 0;
		dir_entry->search_match = 0;

		dir_entry->size = ((uintmax_t)ffd.nFileSizeHigh << 32) + ffd.nFileSizeLow;
		dir_entry->attrs = ffd.dwFileAttributes;
		dir_entry->mtime = win_to_unix_time(ffd.ftLastWriteTime);
		dir_entry->atime = win_to_unix_time(ffd.ftLastAccessTime);
		dir_entry->ctime = win_to_unix_time(ffd.ftCreationTime);

		if(is_win_symlink(ffd.dwFileAttributes, ffd.dwReserved0))
		{
			if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				strcat(dir_entry->name, "/");
			dir_entry->type = LINK;
		}
		else if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			strcat(dir_entry->name, "/");
			dir_entry->type = DIRECTORY;
			name_len++;
		}
		else if(is_win_executable(dir_entry->name))
		{
			dir_entry->type = EXECUTABLE;
		}
		else
		{
			dir_entry->type = REGULAR;
		}
		view->list_rows++;
		name_len += get_filetype_decoration_width(dir_entry->type);
		view->max_filename_len = MAX(view->max_filename_len, name_len);
	}
	while(FindNextFileA(hfind, &ffd));
	FindClose(hfind);

	/* Not all Windows file systems contain or show dot directories. */
	if(!with_parent_dir && parent_dir_is_visible(is_root))
	{
		add_parent_dir(view);
		with_parent_dir = 1;
	}

#endif

	if(!with_parent_dir && !is_root)
	{
		if((cfg.dot_dirs & DD_NONROOT_PARENT) || view->list_rows == 0)
		{
			add_parent_dir(view);
		}
	}

	return 0;
}

/* Checks whether file/directory passes filename filters of the view.  Returns
 * non-zero if given filename passes filter and should be visible, otherwise
 * zero is returned, in which case the file should be hidden. */
TSTATIC int
file_is_visible(FileView *view, const char filename[], int is_dir)
{
	char name_with_slash[strlen(filename) + 1 + 1];
	sprintf(name_with_slash, "%s%c", filename, is_dir ? '/' : '\0');

	if(filter_matches(&view->auto_filter, name_with_slash) > 0)
	{
		return 0;
	}

	if(filter_matches(&view->local_filter.filter, name_with_slash) == 0)
	{
		return 0;
	}

	if(filter_is_empty(&view->name_filter))
	{
		return 1;
	}

	if(filter_matches(&view->name_filter, name_with_slash) > 0)
	{
		return !view->invert;
	}
	else
	{
		return view->invert;
	}
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
populate_dir_list(FileView *view, int reload)
{
	(void)populate_dir_list_internal(view, reload);
}

void
load_dir_list(FileView *view, int reload)
{
	if(populate_dir_list_internal(view, reload) != 0)
	{
		return;
	}

	draw_dir_list(view);

	if(view == curr_view)
	{
		if(strnoscmp(view->curr_dir, cfg.fuse_home, strlen(cfg.fuse_home)) == 0 &&
				stroscmp(other_view->curr_dir, view->curr_dir) == 0)
			load_dir_list(other_view, 1);
	}
}

/* Loads filelist for the view.  The reload parameter should be set in case of
 * view refresh operation.  Returns non-zero on error. */
static int
populate_dir_list_internal(FileView *view, int reload)
{
	int old_list = view->list_rows;
	int need_free = (view->selected_filelist == NULL);

	view->filtered = 0;

	if(update_dir_mtime(view) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't get directory mtime \"%s\"", view->curr_dir);
		return 1;
	}

	if(!reload && is_dir_big(view->curr_dir))
	{
		if(get_mode() != CMDLINE_MODE)
		{
			status_bar_message("Reading directory...");
		}
	}

	if(curr_stats.load_stage < 2)
		update_all_windows();

	/* this is needed for lstat() below */
	if(my_chdir(view->curr_dir) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() into \"%s\"", view->curr_dir);
		return 1;
	}

	if(reload && view->selected_files > 0 && view->selected_filelist == NULL)
		get_all_selected_files(view);

	if(view->dir_entry != NULL)
	{
		int x;
		for(x = 0; x < old_list; x++)
			free(view->dir_entry[x].name);

		free(view->dir_entry);
		view->dir_entry = NULL;
	}
	view->dir_entry = malloc(sizeof(dir_entry_t));
	if(view->dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory.");
		return 1;
	}

	if(fill_dir_list(view) != 0)
	{
		/* we don't have read access, only execute, or there were other problems */
		/* all memory from file names was released in a loop above */
		view->list_rows = 0;
		add_parent_dir(view);
	}

	sort_dir_list(!reload, view);

	if(!reload && get_mode() != CMDLINE_MODE)
	{
		clean_status_bar();
	}

	view->column_count = calculate_columns_count(view);

	/* If reloading the same directory don't jump to
	 * history position.  Stay at the current line
	 */
	if(!reload)
		check_view_dir_history(view);

	view->color_scheme = check_directory_for_color_scheme(view == &lwin,
			view->curr_dir);

	if(view->list_rows < 1)
	{
		rescue_from_empty_filelist(view);
		return 1;
	}

	if(reload && view->selected_files)
		reset_selected_files(view, need_free);
	else if(view->selected_files)
		view->selected_files = 0;

	return 0;
}

/* Checks for subjectively relative size of a directory specified by the path
 * parameter.  Returns non-zero if size of the directory in question is
 * considered to be big. */
static int
is_dir_big(const char path[])
{
#ifndef _WIN32
	struct stat s;
	if(stat(path, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", path);
		return 1;
	}
	return s.st_size > s.st_blksize;
#else
	return 1;
#endif
}

void
resort_dir_list(int msg, FileView *view)
{
	/* Using this pointer after sorting is safe, because file entries are just
	 * moved in the array. */
	const char *const filename = (view->list_pos < view->list_rows) ?
			view->dir_entry[view->list_pos].name : NULL;
	const int top_delta = view->list_pos - view->top_line;

	sort_dir_list(msg, view);

	if(filename != NULL)
	{
		const int new_pos = find_file_pos_in_list(view, filename);
		if(new_pos != -1)
		{
			view->top_line = new_pos - top_delta;
			view->list_pos = new_pos;
		}
	}
}

/* Resorts view without reloading it.  msg parameter controls whether to show
 * "Sorting..." statusbar message. */
static void
sort_dir_list(int msg, FileView *view)
{
	if(msg && view->list_rows > 2048 && get_mode() != CMDLINE_MODE)
	{
		status_bar_message("Sorting directory...");
	}

	sort_view(view);

	if(msg && get_mode() != CMDLINE_MODE)
	{
		clean_status_bar();
	}
}

/* Performs actions needed to rescue from abnormal situation with empty
 * filelist. */
static void
rescue_from_empty_filelist(FileView * view)
{
	/*
	 * It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directories will always show at least the
	 * ../ file.  This resets the filter and reloads the directory.
	 */
	if(is_path_absolute(view->curr_dir) && !filter_is_empty(&view->name_filter))
	{
		show_error_msgf("Filter error",
				"The %s\"%s\" pattern did not match any files. It was reset.",
				view->invert ? "" : "inverted ", view->name_filter.raw);
		filter_clear(&view->auto_filter);
		filter_clear(&view->name_filter);
		view->invert = 1;

		load_dir_list(view, 1);
	}
	else
	{
		add_parent_dir(view);
	}

	if(curr_stats.load_stage >= 2)
		draw_dir_list(view);
}

/* Adds parent directory entry (..) to filelist. */
static void
add_parent_dir(FileView *view)
{
	dir_entry_t *dir_entry;
	struct stat s;

	view->dir_entry = realloc(view->dir_entry,
			sizeof(dir_entry_t)*(view->list_rows + 1));
	if(view->dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	dir_entry = &view->dir_entry[view->list_rows];

	dir_entry->name = strdup("../");
	if(dir_entry->name == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}
	view->max_filename_len = MAX(view->max_filename_len, strlen(dir_entry->name));

	view->list_rows++;

	/* All files start as unselected and unmatched */
	dir_entry->selected = 0;
	dir_entry->search_match = 0;

	dir_entry->type = DIRECTORY;

	/* Load the inode info */
	if(lstat(dir_entry->name, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s/%s\"", view->curr_dir,
				dir_entry->name);
		log_cwd();

		dir_entry->size = 0;
#ifndef _WIN32
		dir_entry->mode = 0;
		dir_entry->uid = -1;
		dir_entry->gid = -1;
#endif
		dir_entry->mtime = 0;
		dir_entry->atime = 0;
		dir_entry->ctime = 0;
	}
	else
	{
		dir_entry->size = (uintmax_t)s.st_size;
#ifndef _WIN32
		dir_entry->mode = s.st_mode;
		dir_entry->uid = s.st_uid;
		dir_entry->gid = s.st_gid;
#endif
		dir_entry->mtime = s.st_mtime;
		dir_entry->atime = s.st_atime;
		dir_entry->ctime = s.st_ctime;
	}
}

void
filter_selected_files(FileView *view)
{
	int i;

	if(!view->selected_files)
		view->dir_entry[view->list_pos].selected = 1;

	for(i = 0; i < view->list_rows; i++)
	{
		const dir_entry_t *const entry = &view->dir_entry[i];

		if(entry->selected && !is_parent_dir(entry->name))
		{
			filter_append(&view->auto_filter, entry->name);
		}
	}

	/* reload view */
	clean_status_bar();
	load_dir_list(view, 1);
	move_to_list_pos(view, view->list_pos);
	view->selected_files = 0;
}

void
set_dot_files_visible(FileView *view, int visible)
{
	view->hide_dot = !visible;
	load_saving_pos(view, 1);
}

void
toggle_dot_files(FileView *view)
{
	set_dot_files_visible(view, view->hide_dot);
}

void
remove_filename_filter(FileView *view)
{
	(void)replace_string(&view->prev_name_filter, view->name_filter.raw);
	filter_clear(&view->name_filter);
	(void)replace_string(&view->prev_auto_filter, view->auto_filter.raw);
	filter_clear(&view->auto_filter);

	view->prev_invert = view->invert;
	view->invert = cfg.filter_inverted_by_default ? 1 : 0;
	load_saving_pos(view, 0);
}

void
restore_filename_filter(FileView *view)
{
	(void)filter_set(&view->name_filter, view->prev_name_filter);
	(void)filter_set(&view->auto_filter, view->prev_auto_filter);
	view->invert = view->prev_invert;
	load_saving_pos(view, 0);
}

void
toggle_filter_inversion(FileView *view)
{
	view->invert = !view->invert;
	load_dir_list(view, 1);
	move_to_list_pos(view, 0);
}

void
local_filter_set(FileView *view, const char filter[])
{
	const int current_file_pos = view->local_filter.in_progress
		? get_unfiltered_pos(view, view->list_pos)
		: load_unfiltered_list(view);

	if(current_file_pos >= 0)
	{
		store_local_filter_position(view, current_file_pos);
	}

	(void)filter_change(&view->local_filter.filter, filter,
			!regexp_should_ignore_case(filter));

	update_filtering_lists(view, 1, 0);
}

/* Loads full list of files into unfiltered list of the view.  Returns positon
 * of file under cursor in the unfiltered list. */
static int
load_unfiltered_list(FileView *const view)
{
	int current_file_pos = view->list_pos;

	view->local_filter.in_progress = 1;

	view->local_filter.saved = strdup(view->local_filter.filter.raw);

	if(view->filtered > 0)
	{
		char *const filename = strdup(view->dir_entry[view->list_pos].name);

		filter_clear(&view->local_filter.filter);
		populate_dir_list(view, 1);

		/* Resolve current file position in updated list. */
		current_file_pos = find_file_pos_in_list(view, filename);
		free(filename);
	}

	view->local_filter.unfiltered = view->dir_entry;
	view->local_filter.unfiltered_count = view->list_rows;
	view->dir_entry = NULL;

	return current_file_pos;
}

/* Gets position of an item in dir_entry list at position pos in the unfiltered
 * list.  Returns index on success, otherwise -1 is returned. */
static int
get_unfiltered_pos(const FileView *const view, int pos)
{
	const size_t count = view->local_filter.unfiltered_count;
	const char *const filename = view->dir_entry[pos].name;
	while(pos < count)
	{
		/* Compare pointers, which are the same for same entry in two arrays. */
		if(view->local_filter.unfiltered[pos].name == filename)
		{
			return pos;
		}
		pos++;
	}
	return -1;
}

/* Adds local filter position (in unfiltered list) to position history. */
static void
store_local_filter_position(FileView *const view, int pos)
{
	size_t *const len = &view->local_filter.poshist_len;
	int *const arr = realloc(view->local_filter.poshist, sizeof(int)*(*len + 1));
	if(arr != NULL)
	{
		view->local_filter.poshist = arr;
		arr[*len] = pos;
		++*len;
	}
}

void
local_filter_update_view(FileView *view, int rel_pos)
{
	int pos = extract_previously_selected_pos(view);

	if(pos < 0)
	{
		pos = find_nearest_neighour(view);
	}

	if(pos >= 0)
	{
		view->list_pos = pos;
		view->top_line = pos - rel_pos;
	}
}

/* Finds one of previously selected files and updates list of visited files
 * if needed.  Returns file position in current list of files or -1. */
static int
extract_previously_selected_pos(FileView *const view)
{
	size_t i;
	for(i = 0; i < view->local_filter.poshist_len; i++)
	{
		const int unfiltered_pos = view->local_filter.poshist[i];
		const char *const file = view->local_filter.unfiltered[unfiltered_pos].name;
		const int filtered_pos = find_file_pos_in_list(view, file);

		if(filtered_pos >= 0)
		{
			clear_local_filter_hist_after(view, i);
			return filtered_pos;
		}
	}
	return -1;
}

/* Clears all positions after the pos. */
static void
clear_local_filter_hist_after(FileView *const view, int pos)
{
	view->local_filter.poshist_len -= view->local_filter.poshist_len - (pos + 1);
}

/* Find nearest filtered neighbour.  Returns index of nearest unfiltered
 * neighbour of the entry initially pointed to by cursor. */
static int
find_nearest_neighour(const FileView *const view)
{
	const int count = view->local_filter.unfiltered_count;

	if(view->local_filter.poshist_len > 0U)
	{
		const int unfiltered_orig_pos = view->local_filter.poshist[0U];
		int i;
		for(i = unfiltered_orig_pos; i < count; i++)
		{
			const int filtered_pos = find_file_pos_in_list(view,
					view->local_filter.unfiltered[i].name);
			if(filtered_pos >= 0)
			{
				return filtered_pos;
			}
		}
	}

	assert(view->list_rows > 0 && "List of files is empty.");
	return view->list_rows - 1;
}

void
local_filter_accept(FileView *view)
{
	if(!view->local_filter.in_progress)
	{
		return;
	}

	update_filtering_lists(view, 0, 1);

	local_filter_finish(view);

	save_filter_history(view->local_filter.filter.raw);
}

void
local_filter_apply(FileView *view, const char filter[])
{
	if(view->local_filter.in_progress)
	{
		assert(!view->local_filter.in_progress && "Wrong local filter applying.");
		return;
	}

	(void)filter_set(&view->local_filter.filter, filter);
	save_filter_history(view->local_filter.filter.raw);
}

void
local_filter_cancel(FileView *view)
{
	if(!view->local_filter.in_progress)
	{
		return;
	}

	(void)filter_set(&view->local_filter.filter, view->local_filter.saved);

	free(view->dir_entry);
	view->dir_entry = NULL;
	view->list_rows = 0;

	update_filtering_lists(view, 1, 1);
	local_filter_finish(view);
}

/* Copies/moves elements of the unfiltered list into dir_entry list.  add
 * parameter controls whether entries matching filter are copied into dir_entry
 * list.  clear parameter controls whether entries not matching filter are
 * cleared in unfiltered list. */
static void
update_filtering_lists(FileView *view, int add, int clear)
{
	size_t i;
	size_t list_size = 0U;

	for(i = 0; i < view->local_filter.unfiltered_count; i++)
	{
		const dir_entry_t *const entry = &view->local_filter.unfiltered[i];
		if(is_parent_dir(entry->name))
		{
			if(add && parent_dir_is_visible(is_root_dir(view->curr_dir)))
			{
				(void)add_dir_entry(&view->dir_entry, &list_size, entry);
			}
		}
		else if(filter_matches(&view->local_filter.filter, entry->name) != 0)
		{
			if(add)
			{
				(void)add_dir_entry(&view->dir_entry, &list_size, entry);
			}
		}
		else
		{
			if(clear)
			{
				free(entry->name);
			}
		}
	}

	if(add)
	{
		view->list_rows = list_size;
		view->filtered = view->local_filter.unfiltered_count - list_size;

		if(list_size == 0U)
		{
			add_parent_dir(view);
		}
	}
}

/* Finishes filtering process and frees associated resources. */
static void
local_filter_finish(FileView *view)
{
	free(view->local_filter.unfiltered);
	free(view->local_filter.saved);
	view->local_filter.in_progress = 0;

	free(view->local_filter.poshist);
	view->local_filter.poshist = NULL;
	view->local_filter.poshist_len = 0U;
}

void
local_filter_remove(FileView *view)
{
	(void)replace_string(&view->local_filter.prev, view->local_filter.filter.raw);
	filter_clear(&view->local_filter.filter);
}

void
local_filter_restore(FileView *view)
{
	(void)filter_set(&view->local_filter.filter, view->local_filter.prev);
	(void)replace_string(&view->local_filter.prev, "");
}

/* Adds new entry to the *list of length *list_size and updates them
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
static int
add_dir_entry(dir_entry_t **list, size_t *list_size, const dir_entry_t *entry)
{
	dir_entry_t *const new_entry_list = realloc(*list,
			sizeof(dir_entry_t)*(*list_size + 1));
	if(new_entry_list == NULL)
	{
		return 1;
	}

	*list = new_entry_list;
	new_entry_list[*list_size] = *entry;
	++*list_size;
	return 0;
}

void
redraw_view(FileView *view)
{
	if(curr_stats.need_update == UT_NONE && !curr_stats.restart_in_progress)
	{
		if(window_shows_dirlist(view))
		{
			draw_dir_list(view);
			if(view == curr_view)
			{
				move_to_list_pos(view, view->list_pos);
			}
		}
	}
}

void
redraw_current_view(void)
{
	redraw_view(curr_view);
}

static void
reload_window(FileView *view)
{
	if(!window_shows_dirlist(view))
		return;

	curr_stats.skip_history = 1;

	load_saving_pos(view, is_dir_list_loaded(view));

	if(view != curr_view)
	{
		put_inactive_mark(view);
		refresh_view_win(view);
	}

	curr_stats.skip_history = 0;
}

/*
 * This checks the modified times of the directories.
 */
void
check_if_filelists_have_changed(FileView *view)
{
	if(is_on_slow_fs(view->curr_dir))
		return;

#ifndef _WIN32
	struct stat s;
	if(stat(view->curr_dir, &s) != 0)
#else
	int r;
	if(is_unc_root(view->curr_dir))
		return;
	r = check_dir_changed(view);
	if(r < 0)
#endif
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		log_cwd();

		show_error_msgf("Directory Change Check", "Cannot open %s", view->curr_dir);

		leave_invalid_dir(view);
		(void)change_directory(view, view->curr_dir);
		clean_selected_files(view);
		reload_window(view);
		return;
	}

#ifndef _WIN32
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	if(memcmp(&s.st_mtim, &view->dir_mtime, sizeof(view->dir_mtime)) != 0)
#else
	if(s.st_mtime != view->dir_mtime)
#endif
#else
	if(r > 0)
#endif
		reload_window(view);
}

int
cd_is_possible(const char *path)
{
	if(!is_valid_dir(path))
	{
		LOG_SERROR_MSG(errno, "Can't access \"%s\"", path);

		show_error_msgf("Destination doesn't exist", "\"%s\"", path);
		return 0;
	}
	else if(!directory_accessible(path))
	{
		LOG_SERROR_MSG(errno, "Can't access(,X_OK) \"%s\"", path);

		show_error_msgf("Permission denied", "\"%s\"", path);
		return 0;
	}
	else
	{
		return 1;
	}
}

void
load_saving_pos(FileView *view, int reload)
{
	char filename[NAME_MAX];
	int pos;

	if(curr_stats.load_stage < 2)
		return;

	if(!window_shows_dirlist(view))
		return;

	if(view->local_filter.in_progress)
	{
		return;
	}

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	load_dir_list(view, reload);
	pos = find_file_pos_in_list(view, filename);
	pos = (pos >= 0) ? pos : view->list_pos;
	if(view == curr_view)
	{
		move_to_list_pos(view, pos);
	}
	else
	{
		mvwaddstr(view->win, view->curr_line, 0, " ");
		view->list_pos = pos;
		if(move_curr_line(view))
			draw_dir_list(view);
		put_inactive_mark(view);
	}

	if(curr_stats.number_of_windows != 1 || view == curr_view)
	{
		refresh_view_win(view);
	}
}

int
window_shows_dirlist(const FileView *const view)
{
	if(curr_stats.number_of_windows == 1 && view == other_view)
		return 0;

	if(view->explore_mode)
		return 0;
	if(view == other_view && get_mode() == VIEW_MODE)
		return 0;
	if(view == other_view && curr_stats.view)
		return 0;
	if(get_mode() != NORMAL_MODE && get_mode() != VISUAL_MODE &&
			get_mode() != VIEW_MODE && get_mode() != CMDLINE_MODE)
		return 0;
	return 1;
}

void
change_sort_type(FileView *view, char type, char descending)
{
	view->sort[0] = descending ? -type : type;
	memset(&view->sort[1], NO_SORT_OPTION, sizeof(view->sort) - 1);

	reset_view_sort(view);

	load_sort_option(view);

	load_saving_pos(view, 1);
}

int
pane_in_dir(FileView *view, const char *path)
{
	char pane_dir[PATH_MAX];
	char dir[PATH_MAX];

	if(realpath(view->curr_dir, pane_dir) != pane_dir)
		return 0;
	if(realpath(path, dir) != dir)
		return 0;
	return stroscmp(pane_dir, dir) == 0;
}

/* Will remove dot and regexp filters if it's needed to make file visible.
 *
 * Returns non-zero if file was found.
 */
int
ensure_file_is_selected(FileView *view, const char name[])
{
	int file_pos;

	file_pos = find_file_pos_in_list(view, name);
	if(file_pos < 0 && file_can_be_displayed(view->curr_dir, name))
	{
		if(name[0] == '.')
		{
			set_dot_files_visible(view, 1);
			file_pos = find_file_pos_in_list(view, name);
		}

		if(file_pos < 0)
		{
			remove_filename_filter(view);
			file_pos = find_file_pos_in_list(view, name);
		}
	}

	move_to_list_pos(view, (file_pos < 0) ? 0 : file_pos);
	return file_pos >= 0;
}

/* Checks if file specified can be displayed. Used to filter some files, that
 * are hidden intensionally.  Returns non-zero if file can be made visible. */
static int
file_can_be_displayed(const char directory[], const char filename[])
{
	if(is_parent_dir(filename))
	{
		return parent_dir_is_visible(is_root_dir(directory));
	}
	return path_exists_at(directory, filename);
}

/* Returns non-zero if ../ directory can be displayed. */
static int
parent_dir_is_visible(int in_root)
{
	return ((in_root && (cfg.dot_dirs & DD_ROOT_PARENT)) ||
			(!in_root && (cfg.dot_dirs & DD_NONROOT_PARENT)));
}

int
cd(FileView *view, const char *base_dir, const char *path)
{
	char dir[PATH_MAX];
	int updir = 0;

	if(path != NULL)
	{
		char *arg = expand_tilde(strdup(path));
#ifndef _WIN32
		if(is_path_absolute(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
#else
		strcpy(dir, base_dir);
		break_at(dir + 2, '/');
		if(is_path_absolute(arg) && *arg != '/')
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_root(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_path(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_path(base_dir))
			sprintf(dir + strlen(dir), "/%s", arg + 1);
		else if(stroscmp(arg, "/") == 0 && is_unc_path(base_dir))
			snprintf(dir, strchr(base_dir + 2, '/') - base_dir + 1, "%s", base_dir);
		else if(*arg == '/')
			snprintf(dir, sizeof(dir), "%c:%s", base_dir[0], arg);
#endif
		else if(strcmp(arg, "-") == 0)
			snprintf(dir, sizeof(dir), "%s", view->last_dir);
		else
			snprintf(dir, sizeof(dir), "%s/%s", base_dir, arg);
		updir = is_parent_dir(arg);
		free(arg);
	}
	else
	{
		snprintf(dir, sizeof(dir), "%s", cfg.home_dir);
	}

	if(!cd_is_possible(dir))
	{
		return 0;
	}
	if(updir)
	{
		cd_updir(view);
	}
	else if(change_directory(view, dir) < 0)
	{
		return 0;
	}

	load_dir_list(view, 0);
	if(view == curr_view)
	{
		move_to_list_pos(view, view->list_pos);
	}
	else
	{
		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}
	return 0;
}

int
view_needs_cd(const FileView *view, const char path[])
{
	if(path[0] == '\0' || stroscmp(view->curr_dir, path) == 0)
		return 0;
	return 1;
}

void
set_view_path(FileView *view, const char path[])
{
	if(view_needs_cd(view, path))
	{
		copy_str(view->curr_dir, sizeof(view->curr_dir), path);
		exclude_file_name(view->curr_dir);
	}
}

uint64_t
get_file_size_by_entry(const FileView *view, size_t pos)
{
	uint64_t size = 0;
	dir_entry_t *entry = &view->dir_entry[pos];

	if(entry->type == DIRECTORY)
	{
		char buf[PATH_MAX];
		snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, entry->name);
		tree_get_data(curr_stats.dirsize_cache, buf, &size);
	}

	return (size == 0) ? entry->size : size;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
