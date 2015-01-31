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

#include "menus.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memmove() memset() strdup() strcat() strncat() strchr()
                       strlen() strrchr() */
#include <wchar.h> /* wchar_t wcscmp() */

#include "../cfg/config.h"
#include "../compat/os.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../modes/modes.h"
#include "../ui/cancellation.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/file_streams.h"
#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../bookmarks.h"
#include "../color_manager.h"
#include "../color_scheme.h"
#include "../colors.h"
#include "../filelist.h"
#include "../macros.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "../vim.h"

static void open_selected_file(const char path[], int line_num);
static void navigate_to_selected_file(FileView *view, const char path[]);
static void normalize_top(menu_info *m);
static void append_to_string(char **str, const char suffix[]);
static char * expand_tabulation_a(const char line[], size_t tab_stops);
static size_t chars_in_str(const char s[], char c);
TSTATIC char * parse_file_spec(const char spec[], int *line_num);

static void
show_position_in_menu(menu_info *m)
{
	char pos_buf[POS_WIN_MIN_WIDTH + 1];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);

	ui_ruler_set(pos_buf);
}

void
remove_current_item(menu_info *m)
{
	clean_menu_position(m);

	remove_from_string_array(m->items, m->len, m->pos);
	if(m->matches != NULL)
	{
		if(m->matches[m->pos])
			m->matching_entries--;
		memmove(m->matches + m->pos, m->matches + m->pos + 1,
				sizeof(int)*((m->len - 1) - m->pos));
	}
	m->len--;
	draw_menu(m);

	move_to_menu_pos(m->pos, m);
}

void
clean_menu_position(menu_info *m)
{
	int x, z;
	int off = 0;
	char * buf = (char *)NULL;
	col_attr_t col;

	x = getmaxx(menu_win) + get_utf8_overhead(m->items[m->pos]);

	buf = malloc(x + 2);

	/* TODO: check if this can ever be false. */
	if(m->items[m->pos] != NULL)
	{
		z = m->hor_pos;
		while(z-- > 0 && m->items[m->pos][off] != '\0')
		{
			size_t l = get_char_width(m->items[m->pos] + off);
			off += l;
			x -= l - 1;
		}
		snprintf(buf, x, " %s", m->items[m->pos] + off);
	}
	else
	{
		buf[0] = '\0';
	}

	for(z = 0; buf[z] != '\0'; z++)
		if(buf[z] == '\t')
			buf[z] = ' ';

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	col = cfg.cs.color[WIN_COLOR];

	if(cfg.hl_search && m->matches != NULL && m->matches[m->pos])
	{
		mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
	}

	wattrset(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

	checked_wmove(menu_win, m->current, 1);
	if(get_screen_string_length(m->items[m->pos] + off) > getmaxx(menu_win) - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		memset(buf + len, ' ', strlen(buf) - len);
		if(strlen(buf) > len + 3)
		{
			buf[len + 3] = '\0';
		}
		wprint(menu_win, buf);
		mvwaddstr(menu_win, m->current, getmaxx(menu_win) - 5, "...");
	}
	else
	{
		size_t len = get_normal_utf8_string_widthn(buf, getmaxx(menu_win) - 4 + 1);
		buf[len] = '\0';
		wprint(menu_win, buf);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

	free(buf);
}

void
init_menu_info(menu_info *m, int menu_type, char empty_msg[])
{
	m->top = 0;
	m->current = 1;
	m->len = 0;
	m->pos = 0;
	m->hor_pos = 0;
	m->win_rows = getmaxy(menu_win);
	m->type = menu_type;
	m->match_dir = NONE;
	m->matching_entries = 0;
	m->matches = NULL;
	m->regexp = NULL;
	m->title = NULL;
	m->args = NULL;
	m->items = NULL;
	m->data = NULL;
	m->key_handler = NULL;
	m->extra_data = 0;
	m->execute_handler = NULL;
	m->empty_msg = empty_msg;
}

void
reset_popup_menu(menu_info *m)
{
	free(m->args);
	/* Menu elements don't always have data associated with them.  That's why we
	 * need this check. */
	if(m->data != NULL)
	{
		free_string_array(m->data, m->len);
	}
	free_string_array(m->items, m->len);
	free(m->regexp);
	free(m->matches);
	free(m->title);
	free(m->empty_msg);

	werase(menu_win);
}

void
setup_menu(void)
{
	scrollok(menu_win, FALSE);
	curs_set(FALSE);
	werase(menu_win);
	werase(status_bar);
	werase(ruler_win);
	wrefresh(status_bar);
	wrefresh(ruler_win);
}

void
move_to_menu_pos(int pos, menu_info *m)
{
	/* TODO: refactor this function move_to_menu_pos() */

	int redraw = 0;
	int x, z;
	char *buf = NULL;
	col_attr_t col;
	int off = 0;

	pos = MIN(m->len - 1, MAX(0, pos));
	if(pos < 0)
		return;

	normalize_top(m);

	if(pos > get_last_visible_line(m))
	{
		m->top = pos - (m->win_rows - 2 - 1);
		redraw = 1;
	}
	else if(pos < m->top)
	{
		m->top = pos;
		redraw = 1;
	}

	if(cfg.scroll_off > 0)
	{
		int s = MIN(DIV_ROUND_UP(m->win_rows - 2, 2), cfg.scroll_off);
		if(pos - m->top < s && m->top > 0)
		{
			m->top -= s - (pos - m->top);
			normalize_top(m);
			redraw = 1;
		}
		if(pos > get_last_visible_line(m) - s)
		{
			m->top += s - (get_last_visible_line(m) - pos);
			normalize_top(m);
			redraw = 1;
		}
	}

	m->current = 1 + (pos - m->top);

	if(redraw)
		draw_menu(m);

	x = getmaxx(menu_win) + get_utf8_overhead(m->items[pos]);
	buf = malloc(x + 2);
	if(buf == NULL)
		return;
	/* TODO: check if this can be false. */
	if(m->items[pos] != NULL)
	{
		z = m->hor_pos;
		while(z-- > 0 && m->items[pos][off] != '\0')
		{
			size_t l = get_char_width(m->items[pos] + off);
			off += l;
			x -= l - 1;
		}

		snprintf(buf, x, " %s", m->items[pos] + off);
	}
	else
	{
		buf[0] = '\0';
	}

	for(z = 0; buf[z] != '\0'; z++)
		if(buf[z] == '\t')
			buf[z] = ' ';

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	col = cfg.cs.color[WIN_COLOR];

	if(cfg.hl_search && m->matches != NULL && m->matches[pos])
		mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);

	mix_colors(&col, &cfg.cs.color[CURR_LINE_COLOR]);

	wattrset(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

	checked_wmove(menu_win, m->current, 1);
	if(get_screen_string_length(m->items[pos] + off) > getmaxx(menu_win) - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		memset(buf + len, ' ', strlen(buf) - len);
		if(strlen(buf) > len + 3)
		{
			buf[len + 3] = '\0';
		}
		wprint(menu_win, buf);
		mvwaddstr(menu_win, m->current, getmaxx(menu_win) - 5, "...");
	}
	else
	{
		size_t len = get_normal_utf8_string_widthn(buf, getmaxx(menu_win) - 4 + 1);
		buf[len] = '\0';
		wprint(menu_win, buf);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

	m->pos = pos;
	free(buf);
	show_position_in_menu(m);
}

void
redraw_menu(menu_info *m)
{
	resize_for_menu_like();
	m->win_rows = getmaxy(menu_win);

	draw_menu(m);
	move_to_menu_pos(m->pos, m);
	wrefresh(menu_win);
}

void
goto_selected_file(FileView *view, const char spec[], int try_open)
{
	char *path_buf;
	int line_num;

	path_buf = parse_file_spec(spec, &line_num);
	if(path_buf == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(path_exists(path_buf, DEREF))
	{
		if(try_open)
		{
			open_selected_file(path_buf, line_num);
		}
		else
		{
			navigate_to_selected_file(view, path_buf);
		}
	}
	else
	{
		show_error_msgf("Missing file", "File \"%s\" doesn't exist", path_buf);
	}

	free(path_buf);
}

/* Opens file specified by its path on the given line number. */
static void
open_selected_file(const char path[], int line_num)
{
	if(os_access(path, R_OK) == 0)
	{
		(void)vim_view_file(path, line_num, -1, 1);
	}
	else
	{
		show_error_msgf("Can't read file", "File \"%s\" is not readable", path);
	}
}

/* Navigates the view to a given dir/file combination specified by the path. */
static void
navigate_to_selected_file(FileView *view, const char path[])
{
	char name[NAME_MAX];
	char *dir = strdup(path);
	char *const last_slash = find_slashr(dir);

	if(last_slash == NULL)
	{
		copy_str(name, sizeof(name), dir);
	}
	else
	{
		*last_slash = '\0';
		copy_str(name, sizeof(name), last_slash + 1);
	}

	if(change_directory(view, dir) >= 0)
	{
		ui_sb_quick_msgf("%s", "Finding the correct directory...");

		load_dir_list(view, 0);

		(void)ensure_file_is_selected(view, name);
	}
	else
	{
		show_error_msgf("Invalid path", "Cannot change dir to \"%s\"", dir);
	}

	free(dir);
}

void
goto_selected_directory(FileView *view, menu_info *m)
{
	if(!cfg.auto_ch_pos)
	{
		clean_positions_in_history(curr_view);
		curr_stats.ch_pos = 0;
	}
	navigate_to(view, m->items[m->pos]);
	if(!cfg.auto_ch_pos)
	{
		curr_stats.ch_pos = 1;
	}
}

void
draw_menu(menu_info *m)
{
	int i;
	int win_len;
	int x, y;

	getmaxyx(menu_win, y, x);
	win_len = x;
	werase(menu_win);

	normalize_top(m);

	x = m->top;

	box(menu_win, 0, 0);
	wattron(menu_win, A_BOLD);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, m->title);
	wattroff(menu_win, A_BOLD);

	for(i = 1; x < m->len; i++, x++)
	{
		int z, off;
		char *buf;
		char *ptr = NULL;
		col_attr_t col;

		chomp(m->items[x]);
		if((ptr = strchr(m->items[x], '\n')) || (ptr = strchr(m->items[x], '\r')))
			*ptr = '\0';

		col = cfg.cs.color[WIN_COLOR];

		if(cfg.hl_search && m->matches != NULL && m->matches[x])
		{
			mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
		}

		wattron(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

		z = m->hor_pos;
		off = 0;
		while(z-- > 0 && m->items[x][off] != '\0')
		{
			size_t l = get_char_width(m->items[x] + off);
			off += l;
		}

		buf = strdup(m->items[x] + off);
		for(z = 0; buf[z] != '\0'; z++)
			if(buf[z] == '\t')
				buf[z] = ' ';

		checked_wmove(menu_win, i, 2);
		if(get_screen_string_length(buf) > win_len - 4)
		{
			size_t len = get_normal_utf8_string_widthn(buf, win_len - 3 - 4);
			memset(buf + len, ' ', strlen(buf) - len);
			buf[len + 3] = '\0';
			wprint(menu_win, buf);
			mvwaddstr(menu_win, i, win_len - 5, "...");
		}
		else
		{
			const size_t len = get_normal_utf8_string_widthn(buf, win_len - 4);
			buf[len] = '\0';
			wprint(menu_win, buf);
		}
		waddstr(menu_win, " ");

		free(buf);

		wattroff(menu_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);

		if(i + 3 > y)
			break;
	}
}

/* Ensures that value of m->top lies in a correct range. */
static void
normalize_top(menu_info *m)
{
	m->top = MAX(0, MIN(m->len - (m->win_rows - 2), m->top));
}

int
capture_output_to_menu(FileView *view, const char cmd[], menu_info *m)
{
	FILE *file, *err;
	char *line = NULL;
	int x;
	pid_t pid;

	LOG_INFO_MSG("Capturing output of the command to a menu: %s", cmd);

	pid = background_and_capture((char *)cmd, &file, &err);
	if(pid == (pid_t)-1)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 0;
	}

	show_progress("", 0);

	ui_cancellation_reset();
	ui_cancellation_enable();

	wait_for_data_from(pid, file, 0);

	x = 0;
	while((line = read_line(file, line)) != NULL)
	{
		char *expanded_line;
		show_progress("Loading menu", 1000);
		m->items = realloc(m->items, sizeof(char *)*(x + 1));
		expanded_line = expand_tabulation_a(line, cfg.tab_stop);
		if(expanded_line != NULL)
		{
			m->items[x++] = expanded_line;
		}

		wait_for_data_from(pid, file, 0);
	}
	m->len = x;

	ui_cancellation_disable();

	fclose(file);
	show_errors_from_file(err);

	if(ui_cancellation_requested())
	{
		append_to_string(&m->title, "(cancelled) ");
		append_to_string(&m->empty_msg, " (cancelled)");
	}

	return display_menu(m, view);
}

/* Replaces *str with a copy of the with string extended by the suffix.  *str
 * can be NULL in which case it's treated as empty string. equal to the with (then function does nothing).  Returns non-zero if memory allocation
 * failed. */
static void
append_to_string(char **str, const char suffix[])
{
	const char *const non_null_str = (*str == NULL) ? "" : *str;
	char *const appended_str = format_str("%s%s", non_null_str, suffix);
	if(appended_str != NULL)
	{
		free(*str);
		*str = appended_str;
	}
}

/* Clones the line replacing all occurrences of horizontal tabulation character
 * with appropriate number of spaces.  The tab_stops parameter shows how many
 * character position are taken by one tabulation.  Returns newly allocated
 * string. */
static char *
expand_tabulation_a(const char line[], size_t tab_stops)
{
	const size_t tab_count = chars_in_str(line, '\t');
	const size_t extra_line_len = tab_count*tab_stops;
	const size_t expanded_line_len = (strlen(line) - tab_count) + extra_line_len;
	char *const expanded_line = malloc(expanded_line_len + 1);

	if(expanded_line != NULL)
	{
		const char *const end = expand_tabulation(line, (size_t)-1, tab_stops,
				expanded_line);
		assert(*end == '\0' && "The line should be processed till the end");
	}

	return expanded_line;
}

/* Returns number of c char occurrences in the s string. */
static size_t
chars_in_str(const char s[], char c)
{
	size_t char_count = 0;
	while(*s != '\0')
	{
		if(*s++ == c)
		{
			char_count++;
		}
	}
	return char_count;
}

int
display_menu(menu_info *m, FileView *view)
{
	if(m->len < 1)
	{
		status_bar_message(m->empty_msg);
		reset_popup_menu(m);
		return 1;
	}
	else
	{
		setup_menu();
		draw_menu(m);
		move_to_menu_pos(m->pos, m);
		enter_menu_mode(m, view);
		return 0;
	}
}

char *
get_cmd_target(void)
{
	return (curr_view->selected_files > 0) ?
		expand_macros("%f", NULL, NULL, 1) : strdup(".");
}

KHandlerResponse
filelist_khandler(menu_info *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"gf") == 0)
	{
		goto_selected_file(curr_view, m->items[m->pos], 0);
		return KHR_CLOSE_MENU;
	}
	else if(wcscmp(keys, L"e") == 0)
	{
		goto_selected_file(curr_view, m->items[m->pos], 1);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

int
menu_to_custom_view(menu_info *m, FileView *view)
{
	int i;
	char *current = NULL;

	flist_custom_start(view, m->title);

	for(i = 0; i < m->len; ++i)
	{
		char *path;
		int line_num;

		/* Skip empty lines. */
		if(m->items[i][0] == '\0')
		{
			continue;
		}

		path = parse_file_spec(m->items[i], &line_num);
		if(path == NULL)
		{
			continue;
		}

		flist_custom_add(view, path);

		if(i == m->pos)
		{
			current = path;
			continue;
		}

		free(path);
	}

	if(flist_custom_finish(view) != 0)
	{
		free(current);
		return 1;
	}

	flist_goto_by_path(view, current);
	free(current);
	return 0;
}

/* Extracts path and line number from the spec (default line number is 1).
 * Returns path in as newly allocated string and sets *line_num to line number,
 * otherwise NULL is returned. */
TSTATIC char *
parse_file_spec(const char spec[], int *line_num)
{
	char *path_buf;
	const char *colon;
	int colon_lookup_offset = 0;
	const size_t bufs_len = 2 + strlen(spec) + 1 + 1;

	path_buf = malloc(bufs_len);
	if(path_buf == NULL)
	{
		return NULL;
	}

	if(is_path_absolute(spec))
	{
		path_buf[0] = '\0';
	}
	else
	{
		copy_str(path_buf, bufs_len, "./");
	}

#ifdef _WIN32
	if(is_path_absolute(spec))
	{
		colon_lookup_offset = 2;
	}
#endif

	colon = strchr(spec + colon_lookup_offset, ':');
	if(colon != NULL)
	{
		strncat(path_buf, spec, colon - spec);
		*line_num = atoi(colon + 1);
	}
	else
	{
		strcat(path_buf, spec);
		*line_num = 1;
	}

	chomp(path_buf);

#ifdef _WIN32
	to_forward_slash(path_buf);
#endif

	return path_buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
