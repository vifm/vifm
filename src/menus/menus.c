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
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../int/vim.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../modes/modes.h"
#include "../ui/cancellation.h"
#include "../ui/color_manager.h"
#include "../ui/color_scheme.h"
#include "../ui/colors.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/regexp.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../filelist.h"
#include "../macros.h"
#include "../marks.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"

static void show_position_in_menu(const menu_info *m);
static void open_selected_file(const char path[], int line_num);
static void navigate_to_selected_file(FileView *view, const char path[]);
static void draw_menu_item(menu_info *m, int pos, int line, int clear);
static void normalize_top(menu_info *m);
static void draw_menu_frame(const menu_info *m);
static void output_handler(const char line[], void *arg);
static void append_to_string(char **str, const char suffix[]);
static char * expand_tabulation_a(const char line[], size_t tab_stops);
static size_t chars_in_str(const char s[], char c);
static int search_menu(menu_info *m, int start_pos);
static int search_menu_forwards(menu_info *m, int start_pos);
static int search_menu_backwards(menu_info *m, int start_pos);
static int navigate_to_match(menu_info *m, int pos);
static int get_match_index(const menu_info *m);

void
remove_current_item(menu_info *m)
{
	clean_menu_position(m);

	remove_from_string_array(m->items, m->len, m->pos);

	if(m->data != NULL)
	{
		remove_from_string_array(m->data, m->len, m->pos);
	}

	if(m->matches != NULL)
	{
		if(m->matches[m->pos])
		{
			--m->matching_entries;
		}
		memmove(m->matches + m->pos, m->matches + m->pos + 1,
				sizeof(int)*((m->len - 1) - m->pos));
	}

	--m->len;
	draw_menu(m);

	move_to_menu_pos(m->pos, m);
}

void
clean_menu_position(menu_info *m)
{
	draw_menu_item(m, m->pos, m->current, 1);
}

void
init_menu_info(menu_info *m, char title[], char empty_msg[])
{
	m->top = 0;
	m->current = 1;
	m->len = 0;
	m->pos = 0;
	m->hor_pos = 0;
	m->win_rows = getmaxy(menu_win);
	m->backward_search = 0;
	m->matching_entries = 0;
	m->matches = NULL;
	m->regexp = NULL;
	m->title = title;
	m->args = NULL;
	m->items = NULL;
	m->data = NULL;
	m->key_handler = NULL;
	m->extra_data = 0;
	m->execute_handler = NULL;
	m->empty_msg = empty_msg;
	m->search_repeat = 0;
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
	int redraw;

	pos = MIN(m->len - 1, MAX(0, pos));
	if(pos < 0)
	{
		return;
	}

	normalize_top(m);

	redraw = 0;
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
	m->pos = pos;

	if(redraw)
	{
		draw_menu(m);
	}
	else
	{
		draw_menu_item(m, m->pos, m->current, 0);
	}

	show_position_in_menu(m);
}

/* Displays current menu position on a ruler. */
static void
show_position_in_menu(const menu_info *m)
{
	char pos_buf[POS_WIN_MIN_WIDTH + 1];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);

	ui_ruler_set(pos_buf);
}

void
redraw_menu(menu_info *m)
{
	if(resize_for_menu_like() != 0)
	{
		return;
	}

	m->win_rows = getmaxy(menu_win);

	draw_menu(m);
	move_to_menu_pos(m->pos, m);
	wrefresh(menu_win);
}

int
goto_selected_file(FileView *view, const char spec[], int try_open)
{
	char *path_buf;
	int line_num;

	path_buf = parse_file_spec(spec, &line_num);
	if(path_buf == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 1;
	}

	if(!path_exists(path_buf, NODEREF))
	{
		show_error_msgf("Missing file", "File \"%s\" doesn't exist", path_buf);
		free(path_buf);
		return 1;
	}

	if(try_open)
	{
		open_selected_file(path_buf, line_num);
	}
	else
	{
		navigate_to_selected_file(view, path_buf);
	}

	free(path_buf);
	return 0;
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
goto_selected_directory(FileView *view, const char path[])
{
	if(!cfg.auto_ch_pos)
	{
		clean_positions_in_history(view);
		curr_stats.ch_pos = 0;
	}
	navigate_to(view, path);
	if(!cfg.auto_ch_pos)
	{
		curr_stats.ch_pos = 1;
	}
}

void
draw_menu(menu_info *m)
{
	int i, pos;
	const int y = getmaxy(menu_win);

	normalize_top(m);

	werase(menu_win);
	draw_menu_frame(m);

	for(i = 0, pos = m->top; i < y - 2 && pos < m->len; ++i, ++pos)
	{
		draw_menu_item(m, pos, i + 1, 0);
	}
}

/* Draws single menu item at position specified by line argument.  Non-zero
 * clear argument suppresses drawing current items in different color. */
static void
draw_menu_item(menu_info *m, int pos, int line, int clear)
{
	int i;
	int off;
	char *item_tail;
	const int width = (curr_stats.load_stage == 0) ? 100 : getmaxx(menu_win) - 2;

	/* Calculate color for the line. */
	int attrs;
	col_attr_t col = cfg.cs.color[WIN_COLOR];
	if(cfg.hl_search && m->matches != NULL && m->matches[pos])
	{
		mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
	}
	if(!clear && pos == m->pos)
	{
		mix_colors(&col, &cfg.cs.color[CURR_LINE_COLOR]);
	}
	attrs = COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr;

	/* Calculate offset of m->hor_pos's character in item text. */
	off = 0;
	i = m->hor_pos;
	while(i-- > 0 && m->items[pos][off] != '\0')
	{
		off += utf8_chrw(m->items[pos] + off);
	}

	item_tail = strdup(m->items[pos] + off);
	replace_char(item_tail, '\t', ' ');

	wattron(menu_win, attrs);

	/* Clear the area. */
	checked_wmove(menu_win, line, 1);
	if(curr_stats.load_stage != 0)
	{
		wprintw(menu_win, "%*s", width, "");
	}

	/* Draw visible part of item. */
	checked_wmove(menu_win, line, 2);
	if(utf8_strsw(item_tail) > (size_t)(width - 2))
	{
		size_t len = utf8_nstrsnlen(item_tail, width - 1 - 4 + 1);
		memset(item_tail + len, ' ', strlen(item_tail) - len);
		if(strlen(item_tail) > len + 3)
		{
			item_tail[len + 3] = '\0';
		}
		wprint(menu_win, item_tail);
		mvwaddstr(menu_win, line, width - 3, "...");
	}
	else
	{
		const size_t len = utf8_nstrsnlen(item_tail, width - 2 + 1);
		item_tail[len] = '\0';
		wprint(menu_win, item_tail);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, attrs);

	free(item_tail);
}

/* Ensures that value of m->top lies in a correct range. */
static void
normalize_top(menu_info *m)
{
	m->top = MAX(0, MIN(m->len - (m->win_rows - 2), m->top));
}

/* Draws box and title of the menu. */
static void
draw_menu_frame(const menu_info *m)
{
	const size_t title_len = getmaxx(menu_win) - 2*4;
	char *const title = strdup(m->title);

	if(utf8_strsw(title) > title_len)
	{
		const size_t len = utf8_nstrsnlen(title, title_len - 3);
		strcpy(title + len, "...");
	}

	box(menu_win, 0, 0);
	wattron(menu_win, A_BOLD);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, " ");
	wprint(menu_win, title);
	wprint(menu_win, " ");
	wattroff(menu_win, A_BOLD);

	free(title);
}

int
capture_output_to_menu(FileView *view, const char cmd[], int user_sh,
		menu_info *m)
{
	if(process_cmd_output("Loading menu", cmd, user_sh, &output_handler, m) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 0;
	}

	if(ui_cancellation_requested())
	{
		append_to_string(&m->title, "(cancelled)");
		append_to_string(&m->empty_msg, " (cancelled)");
	}

	return display_menu(m, view);
}

/* Implements process_cmd_output() callback that loads lines to a menu. */
static void
output_handler(const char line[], void *arg)
{
	menu_info *m = arg;
	char *expanded_line;

	m->items = reallocarray(m->items, m->len + 1, sizeof(char *));
	expanded_line = expand_tabulation_a(line, cfg.tab_stop);
	if(expanded_line != NULL)
	{
		m->items[m->len++] = expanded_line;
	}
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
		(void)end;
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
prepare_targets(FileView *view)
{
	if(view->selected_files > 0)
	{
		return expand_macros("%f", NULL, NULL, 1);
	}

	if(!flist_custom_active(view))
	{
		return strdup(".");
	}

	return (vifm_chdir(flist_get_dir(view)) == 0) ? strdup(".") : NULL;
}

KHandlerResponse
filelist_khandler(menu_info *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"gf") == 0)
	{
		(void)goto_selected_file(curr_view, m->items[m->pos], 0);
		return KHR_CLOSE_MENU;
	}
	else if(wcscmp(keys, L"e") == 0)
	{
		(void)goto_selected_file(curr_view, m->items[m->pos], 1);
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"c") == 0)
	{
		/* Insert just file name. */
		int line_num;
		char *const path = parse_file_spec(m->items[m->pos], &line_num);
		if(path == NULL)
		{
			show_error_msg("Command insertion", "No valid filename found");
			return KHR_REFRESH_WINDOW;
		}
		menu_morph_into_cmdline(CLS_COMMAND, path, 1);
		free(path);
		return KHR_MORPHED_MENU;
	}

	return KHR_UNHANDLED;
}

int
menu_to_custom_view(menu_info *m, FileView *view, int very)
{
	int i;
	char *current = NULL;

	flist_custom_start(view, m->title);

	for(i = 0; i < m->len; ++i)
	{
		char *path;
		int line_num;

		/* Skip empty lines. */
		if(skip_whitespace(m->items[i])[0] == '\0')
		{
			continue;
		}

		path = parse_file_spec(m->items[i], &line_num);
		if(path == NULL)
		{
			continue;
		}

		flist_custom_add(view, path);

		/* Use either exact position or the next path. */
		if(i == m->pos || (current == NULL && i > m->pos))
		{
			current = path;
			continue;
		}

		free(path);
	}

	/* If current line and none of the lines below didn't contain valid path, try
	 * to use file above cursor position. */
	if(current == NULL && view->custom.entry_count != 0)
	{
		char full_path[PATH_MAX];
		get_full_path_of(&view->custom.entries[view->custom.entry_count - 1],
				sizeof(full_path), full_path);

		current = strdup(full_path);
	}

	if(flist_custom_finish(view, very) != 0)
	{
		free(current);
		return 1;
	}

	if(current != NULL)
	{
		flist_goto_by_path(view, current);
		free(current);
	}

	return 0;
}

int
capture_output(FileView *view, const char cmd[], int user_sh, menu_info *m,
		int custom_view, int very_custom_view)
{
	if(custom_view || very_custom_view)
	{
		output_to_custom_flist(view, cmd, very_custom_view);
		return 0;
	}

	return capture_output_to_menu(view, cmd, user_sh, m);
}

void
menus_search(menu_info *m, int backward)
{
	if(m->regexp == NULL)
	{
		status_bar_error("No search pattern set");
		curr_stats.save_msg = 1;
		return;
	}

	m->backward_search = backward;
	(void)search_menu_list(NULL, m);
	wrefresh(menu_win);

	if(m->matching_entries > 0)
	{
		status_bar_messagef("(%d of %d) %c%s", get_match_index(m),
				m->matching_entries, backward ? '?' : '/', m->regexp);
	}

	curr_stats.save_msg = 1;
}

int
search_menu_list(const char pattern[], menu_info *m)
{
	int save = 0;
	int i;

	if(pattern != NULL)
	{
		replace_string(&m->regexp, pattern);
		if(search_menu(m, m->pos) != 0)
		{
			draw_menu(m);
			move_to_menu_pos(m->pos, m);
			return 1;
		}
		draw_menu(m);
	}

	for(i = 0; i < m->search_repeat; ++i)
	{
		if(m->backward_search)
		{
			save = search_menu_backwards(m, m->pos - 1);
		}
		else
		{
			save = search_menu_forwards(m, m->pos + 1);
		}
	}
	return save;
}

/* Goes through all menu items and marks those that match search pattern.
 * Returns non-zero on error. */
static int
search_menu(menu_info *m, int start_pos)
{
	int cflags;
	regex_t re;
	int err;
	int i;

	if(m->matches == NULL)
	{
		m->matches = reallocarray(NULL, m->len, sizeof(int));
	}

	memset(m->matches, 0, sizeof(int)*m->len);
	m->matching_entries = 0;

	if(m->regexp[0] == '\0')
	{
		return 0;
	}

	cflags = get_regexp_cflags(m->regexp);
	err = regcomp(&re, m->regexp, cflags);
	if(err != 0)
	{
		status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return -1;
	}

	for(i = 0; i < m->len; ++i)
	{
		if(regexec(&re, m->items[i], 0, NULL, 0) == 0)
		{
			m->matches[i] = 1;
			++m->matching_entries;
		}
	}
	regfree(&re);
	return 0;
}

/* Looks for next matching element in forward direction from current position.
 * Returns new value for save_msg flag. */
static int
search_menu_forwards(menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = 0; i < m->len; ++i)
	{
		if(!m->matches[i])
		{
			continue;
		}

		if(match_up < 0 && i < start_pos)
		{
			match_up = i;
		}
		if(match_down < 0 && i >= start_pos)
		{
			match_down = i;
		}
	}

	if(!cfg.wrap_scan && match_down <= -1)
	{
		status_bar_errorf("Search hit BOTTOM without match for: %s", m->regexp);
		return 1;
	}

	return navigate_to_match(m, (match_down > -1) ? match_down : match_up);
}

/* Looks for next matching element in backward direction from current position.
 * Returns new value for save_msg flag. */
static int
search_menu_backwards(menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = m->len - 1; i > -1; --i)
	{
		if(!m->matches[i])
		{
			continue;
		}

		if(match_up < 0 && i <= start_pos)
		{
			match_up = i;
		}
		if(match_down < 0 && i > start_pos)
		{
			match_down = i;
		}
	}

	if(!cfg.wrap_scan && match_up <= -1)
	{
		status_bar_errorf("Search hit TOP without match for: %s", m->regexp);
		return 1;
	}

	return navigate_to_match(m, (match_up > -1) ? match_up : match_down);
}

/* Tries to navigate to menu search match specified via pos argument.  If pos is
 * negative, match wasn't found and the message is printed.  Returns new value
 * for save_msg flag. */
static int
navigate_to_match(menu_info *m, int pos)
{
	if(pos > -1)
	{
		clean_menu_position(m);
		move_to_menu_pos(pos, m);
		menu_print_search_msg(m);
	}
	else
	{
		move_to_menu_pos(m->pos, m);
		if(cfg.wrap_scan)
		{
			menu_print_search_msg(m);
		}
	}
	return 1;
}

void
menu_print_search_msg(const menu_info *m)
{
	int cflags;
	regex_t re;
	int err;

	/* Can be NULL after regex compilation failure. */
	if(m->regexp == NULL)
	{
		return;
	}

	cflags = get_regexp_cflags(m->regexp);
	err = regcomp(&re, m->regexp, cflags);

	if(err != 0)
	{
		status_bar_errorf("Regexp (%s) error: %s", m->regexp,
				get_regexp_error(err, &re));
		regfree(&re);
		return;
	}

	regfree(&re);

	if(m->matching_entries > 0)
	{
		status_bar_messagef("%d of %d %s", get_match_index(m), m->matching_entries,
				(m->matching_entries == 1) ? "match" : "matches");
	}
	else
	{
		status_bar_errorf("No matches for: %s", m->regexp);
	}
}

/* Calculates the index of the current match from the list of matches.  Returns
 * the index. */
static int
get_match_index(const menu_info *m)
{
	int n, i;

	n = (m->matches[0] ? 1 : 0);
	i = 0;
	while(i++ < m->pos)
	{
		if(m->matches[i])
		{
			++n;
		}
	}

	return n;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
