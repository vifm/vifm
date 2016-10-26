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
#include "../flist_pos.h"
#include "../macros.h"
#include "../marks.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"

static void reset_menu_state(menu_state_t *ms);
static void show_position_in_menu(const menu_data_t *m);
static void open_selected_file(const char path[], int line_num);
static void navigate_to_selected_file(FileView *view, const char path[]);
static void draw_menu_item(menu_state_t *ms, int pos, int line, int clear);
static void draw_search_match(char str[], int start, int end, int line,
		int width, int attrs);
static void normalize_top(menu_state_t *m);
static void draw_menu_frame(const menu_state_t *m);
static void output_handler(const char line[], void *arg);
static void append_to_string(char **str, const char suffix[]);
static char * expand_tabulation_a(const char line[], size_t tab_stops);
static void init_menu_state(menu_state_t *ms);
static int search_menu(menu_state_t *ms, int start_pos, int print_errors);
static int search_menu_forwards(menu_state_t *m, int start_pos);
static int search_menu_backwards(menu_state_t *m, int start_pos);
static int navigate_to_match(menu_state_t *m, int pos);
static int get_match_index(const menu_state_t *m);

struct menu_state_t
{
	menu_data_t *d;
	int current; /* Cursor position on the menu_win. */
	int win_rows;
	int backward_search; /* Search direction. */
	/* Number of menu entries that actually match the regexp. */
	int matching_entries;
	/* Whether search highlight matches are currently highlighted. */
	int search_highlight;
	/* Start and end positions of search match.  If there is no match, values are
	 * equal to -1. */
	short int (*matches)[2];
	char *regexp;
	/* Number of times to repeat search. */
	int search_repeat;
}
menu_state;

void
remove_current_item(menu_state_t *ms)
{
	menu_data_t *const m = ms->d;
	menu_current_line_erase(ms);

	remove_from_string_array(m->items, m->len, m->pos);

	if(m->data != NULL)
	{
		remove_from_string_array(m->data, m->len, m->pos);
	}

	if(m->void_data != NULL)
	{
		memmove(m->void_data + m->pos, m->void_data + m->pos + 1,
				sizeof(*m->void_data)*((m->len - 1) - m->pos));
	}

	if(ms->matches != NULL)
	{
		if(ms->matches[m->pos][0] >= 0)
		{
			--ms->matching_entries;
		}
		memmove(ms->matches + m->pos, ms->matches + m->pos + 1,
				sizeof(*ms->matches)*((m->len - 1) - m->pos));
	}

	--m->len;
	draw_menu(ms);

	move_to_menu_pos(m->pos, ms);
}

void
menu_current_line_erase(menu_state_t *m)
{
	draw_menu_item(m, m->d->pos, m->current, 1);
}

void
init_menu_data(menu_data_t *m, char title[], char empty_msg[])
{
	if(m->initialized)
	{
		reset_menu_data(m);
	}

	if(menu_state.d != NULL)
	{
		menu_state.d->state = NULL;
	}
	menu_state.d = m;

	m->top = 0;
	m->len = 0;
	m->pos = 0;
	m->hor_pos = 0;
	m->title = title;
	m->items = NULL;
	m->data = NULL;
	m->void_data = NULL;
	m->key_handler = NULL;
	m->extra_data = 0;
	m->execute_handler = NULL;
	m->empty_msg = empty_msg;
	m->state = &menu_state;
	m->initialized = 1;
}

void
reset_menu_data(menu_data_t *m)
{
	if(!m->initialized)
	{
		return;
	}

	/* Menu elements don't always have data associated with them, but len isn't
	 * zero.  That's why we need this check. */
	if(m->data != NULL)
	{
		free_string_array(m->data, m->len);
		m->data = NULL;
	}
	free_string_array(m->items, m->len);
	free(m->void_data);
	free(m->title);
	free(m->empty_msg);
	m->initialized = 0;

	reset_menu_state(m->state);
}

/* Frees resources associated with menu mode.  ms can be NULL. */
static void
reset_menu_state(menu_state_t *ms)
{
	if(ms != NULL)
	{
		update_string(&ms->regexp, NULL);
		free(ms->matches);
		ms->matches = NULL;

		if(menu_state.d != NULL)
		{
			menu_state.d->state = NULL;
		}
		menu_state.d->state = NULL;
	}
}

void
setup_menu(void)
{
	scrollok(menu_win, FALSE);
	curs_set(0);
	werase(menu_win);
	werase(status_bar);
	werase(ruler_win);
	wrefresh(status_bar);
	wrefresh(ruler_win);
}

void
move_to_menu_pos(int pos, menu_state_t *ms)
{
	menu_data_t *const m = ms->d;
	int redraw;

	pos = MIN(m->len - 1, MAX(0, pos));
	if(pos < 0)
	{
		return;
	}

	normalize_top(ms);

	redraw = 0;
	if(pos > get_last_visible_line(m))
	{
		m->top = pos - (ms->win_rows - 2 - 1);
		redraw = 1;
	}
	else if(pos < m->top)
	{
		m->top = pos;
		redraw = 1;
	}

	if(cfg.scroll_off > 0)
	{
		int s = MIN(DIV_ROUND_UP(ms->win_rows - 2, 2), cfg.scroll_off);
		if(pos - m->top < s && m->top > 0)
		{
			m->top -= s - (pos - m->top);
			normalize_top(ms);
			redraw = 1;
		}
		if(pos > get_last_visible_line(m) - s)
		{
			m->top += s - (get_last_visible_line(m) - pos);
			normalize_top(ms);
			redraw = 1;
		}
	}

	ms->current = 1 + (pos - m->top);
	m->pos = pos;

	if(redraw)
	{
		draw_menu(ms);
	}
	else
	{
		draw_menu_item(ms, m->pos, ms->current, 0);
	}

	show_position_in_menu(m);
}

/* Displays current menu position on a ruler. */
static void
show_position_in_menu(const menu_data_t *m)
{
	char pos_buf[POS_WIN_MIN_WIDTH + 1];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);

	ui_ruler_set(pos_buf);
}

void
redraw_menu(menu_state_t *m)
{
	if(resize_for_menu_like() != 0)
	{
		return;
	}

	m->win_rows = getmaxy(menu_win);

	draw_menu(m);
	move_to_menu_pos(m->d->pos, m);
	wrefresh(menu_win);
}

int
goto_selected_file(FileView *view, const char spec[], int try_open)
{
	char *path_buf;
	int line_num;

	path_buf = parse_file_spec(spec, &line_num, ".");
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
		flist_hist_clear(view);
		curr_stats.ch_pos = 0;
	}
	navigate_to(view, path);
	if(!cfg.auto_ch_pos)
	{
		curr_stats.ch_pos = 1;
	}
}

void
draw_menu(menu_state_t *m)
{
	int i, pos;
	const int y = getmaxy(menu_win);

	normalize_top(m);

	werase(menu_win);
	draw_menu_frame(m);

	for(i = 0, pos = m->d->top; i < y - 2 && pos < m->d->len; ++i, ++pos)
	{
		draw_menu_item(m, pos, i + 1, 0);
	}
}

/* Draws single menu item at position specified by line argument.  Non-zero
 * clear argument suppresses drawing current items in different color. */
static void
draw_menu_item(menu_state_t *ms, int pos, int line, int clear)
{
	menu_data_t *const m = ms->d;
	int i;
	int off;
	char *item_tail;
	const int width = (curr_stats.load_stage == 0) ? 100 : getmaxx(menu_win) - 2;

	/* Calculate color for the line. */
	int attrs;
	col_attr_t col = cfg.cs.color[WIN_COLOR];
	if(cfg.hl_search && ms->search_highlight &&
			ms->matches != NULL && ms->matches[pos][0] >= 0)
	{
		cs_mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
	}
	if(!clear && pos == m->pos)
	{
		cs_mix_colors(&col, &cfg.cs.color[CURR_LINE_COLOR]);
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
		void *p;
		const size_t len = utf8_nstrsnlen(item_tail, width - 3 - 2 + 1);
		memset(item_tail + len, ' ', strlen(item_tail) - len);
		p = realloc(item_tail, len + 4);
		if(p != NULL)
		{
			item_tail = p;
			strcpy(item_tail + len - 1, "...");
		}
		wprint(menu_win, item_tail);
	}
	else
	{
		const size_t len = utf8_nstrsnlen(item_tail, width - 2 + 1);
		item_tail[len] = '\0';
		wprint(menu_win, item_tail);
	}

	wattroff(menu_win, attrs);

	if(ms->search_highlight && ms->matches != NULL && ms->matches[pos][0] >= 0)
	{
		draw_search_match(item_tail, ms->matches[pos][0] - m->hor_pos,
				ms->matches[pos][1] - m->hor_pos, line, width, attrs);
	}

	free(item_tail);
}

/* Draws search match highlight on the element. */
static void
draw_search_match(char str[], int start, int end, int line, int width,
		int attrs)
{
	const int len = strlen(str);

	if(end <= 0)
	{
		/* Match is completely at the left. */

		checked_wmove(menu_win, line, 2);
		wprinta(menu_win, "<<<", attrs ^ A_REVERSE);
	}
	else if(start >= len)
	{
		/* Match is completely at the right. */

		checked_wmove(menu_win, line, width - 3);
		wprinta(menu_win, ">>>", attrs ^ A_REVERSE);
	}
	else
	{
		/* Match is at least partially visible. */

		char c;
		int match_start;

		if(start < 0)
		{
			start = 0;
		}
		if(end < len)
		{
			str[end] = '\0';
		}

		/* Calculate number of screen characters before the match. */
		c = str[start];
		str[start] = '\0';
		match_start = utf8_strsw(str);
		str[start] = c;

		checked_wmove(menu_win, line, 2 + match_start);
		wprinta(menu_win, str + start, attrs ^ (A_REVERSE | A_UNDERLINE));
	}
}

/* Ensures that value of m->top lies in a correct range. */
static void
normalize_top(menu_state_t *m)
{
	m->d->top = MAX(0, MIN(m->d->len - (m->win_rows - 2), m->d->top));
}

/* Draws box and title of the menu. */
static void
draw_menu_frame(const menu_state_t *m)
{
	const size_t title_len = getmaxx(menu_win) - 2*4;
	char *const title = strdup(m->d->title);

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
		menu_state_t *m)
{
	if(process_cmd_output("Loading menu", cmd, user_sh, 0, &output_handler,
				m->d) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 0;
	}

	if(ui_cancellation_requested())
	{
		append_to_string(&m->d->title, "(cancelled)");
		append_to_string(&m->d->empty_msg, " (cancelled)");
	}

	return display_menu(m, view);
}

/* Implements process_cmd_output() callback that loads lines to a menu. */
static void
output_handler(const char line[], void *arg)
{
	menu_data_t *const m = arg;
	char *expanded_line;

	m->items = reallocarray(m->items, m->len + 1, sizeof(char *));
	expanded_line = expand_tabulation_a(line, cfg.tab_stop);
	if(expanded_line != NULL)
	{
		m->items[m->len++] = expanded_line;
	}
}

/* Replaces *str with a copy of the with string extended by the suffix.  *str
 * can be NULL in which case it's treated as empty string, equal to the with
 * (then function does nothing).  Returns non-zero if memory allocation
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

int
display_menu(menu_state_t *m, FileView *view)
{
	if(m->d->len < 1)
	{
		status_bar_message(m->d->empty_msg);
		reset_menu_data(m->d);
		return 1;
	}

	init_menu_state(m);

	setup_menu();
	draw_menu(m);
	move_to_menu_pos(m->d->pos, m);
	enter_menu_mode(m->d, view);
	return 0;
}

/* Initializes menu state structure with default/initial value. */
static void
init_menu_state(menu_state_t *ms)
{
	ms->current = 1;
	ms->win_rows = getmaxy(menu_win);
	ms->backward_search = 0;
	ms->matching_entries = 0;
	ms->search_highlight = 1;
	ms->matches = NULL;
	ms->regexp = NULL;
	ms->search_repeat = 0;
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
filelist_khandler(FileView *view, menu_data_t *m, const wchar_t keys[])
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
		char *const path = parse_file_spec(m->items[m->pos], &line_num, ".");
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
menu_to_custom_view(menu_state_t *m, FileView *view, int very)
{
	int i;
	char *current = NULL;

	flist_custom_start(view, m->d->title);

	for(i = 0; i < m->d->len; ++i)
	{
		char *path;
		int line_num;

		/* Skip empty lines. */
		if(skip_whitespace(m->d->items[i])[0] == '\0')
		{
			continue;
		}

		path = parse_file_spec(m->d->items[i], &line_num, ".");
		if(path == NULL)
		{
			continue;
		}

		flist_custom_add(view, path);

		/* Use either exact position or the next path. */
		if(i == m->d->pos || (current == NULL && i > m->d->pos))
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

	if(flist_custom_finish(view, very ? CV_VERY : CV_REGULAR, 0) != 0)
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
capture_output(FileView *view, const char cmd[], int user_sh, menu_data_t *m,
		int custom_view, int very_custom_view)
{
	if(custom_view || very_custom_view)
	{
		reset_menu_data(m);
		output_to_custom_flist(view, cmd, very_custom_view, 0);
		return 0;
	}

	return capture_output_to_menu(view, cmd, user_sh, m->state);
}

void
menus_search(menu_state_t *m, int backward)
{
	if(m->regexp == NULL)
	{
		status_bar_error("No search pattern set");
		curr_stats.save_msg = 1;
		return;
	}

	m->backward_search = backward;
	(void)search_menu_list(NULL, m->d, 1);
	wrefresh(menu_win);

	if(m->matching_entries > 0)
	{
		status_bar_messagef("(%d of %d) %c%s", get_match_index(m),
				m->matching_entries, backward ? '?' : '/', m->regexp);
	}

	curr_stats.save_msg = 1;
}

int
search_menu_list(const char pattern[], menu_data_t *m, int print_errors)
{
	menu_state_t *const ms = m->state;
	const int do_search = (pattern != NULL || ms->matches == NULL);
	int save = 0;
	int i;

	if(pattern != NULL)
	{
		replace_string(&ms->regexp, pattern);
	}

	if(do_search)
	{
		/* Reactivate match highlighting on search. */
		ms->search_highlight = 1;
		if(search_menu(ms, m->pos, print_errors) != 0)
		{
			draw_menu(ms);
			move_to_menu_pos(m->pos, ms);
			return -1;
		}
		draw_menu(ms);
	}

	for(i = 0; i < ms->search_repeat; ++i)
	{
		if(ms->backward_search)
		{
			save = search_menu_backwards(ms, m->pos - 1);
		}
		else
		{
			save = search_menu_forwards(ms, m->pos + 1);
		}
	}
	return save;
}

/* Goes through all menu items and marks those that match search pattern.
 * Returns non-zero on error. */
static int
search_menu(menu_state_t *ms, int start_pos, int print_errors)
{
	menu_data_t *const m = ms->d;
	int cflags;
	regex_t re;
	int err;
	int i;

	if(ms->matches == NULL)
	{
		ms->matches = reallocarray(NULL, m->len, sizeof(*ms->matches));
	}

	memset(ms->matches, -1, 2*sizeof(**ms->matches)*m->len);
	ms->matching_entries = 0;

	if(ms->regexp[0] == '\0')
	{
		return 0;
	}

	cflags = get_regexp_cflags(ms->regexp);
	err = regcomp(&re, ms->regexp, cflags);
	if(err != 0)
	{
		if(print_errors)
		{
			status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		}
		regfree(&re);
		return -1;
	}

	for(i = 0; i < m->len; ++i)
	{
		regmatch_t matches[1];
		if(regexec(&re, m->items[i], 1, matches, 0) == 0)
		{
			ms->matches[i][0] = matches[0].rm_so;
			ms->matches[i][1] = matches[0].rm_eo;

			++ms->matching_entries;
		}
	}
	regfree(&re);
	return 0;
}

/* Looks for next matching element in forward direction from current position.
 * Returns new value for save_msg flag. */
static int
search_menu_forwards(menu_state_t *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = 0; i < m->d->len; ++i)
	{
		if(m->matches[i][0] < 0)
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
search_menu_backwards(menu_state_t *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = m->d->len - 1; i > -1; --i)
	{
		if(m->matches[i][0] < 0)
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
navigate_to_match(menu_state_t *m, int pos)
{
	if(pos > -1)
	{
		if(!m->search_highlight)
		{
			/* Might need to highlight other items, so redraw whole menu. */
			m->search_highlight = 1;
			m->d->pos = pos;
			draw_menu(m);
		}
		else
		{
			menu_current_line_erase(m);
			move_to_menu_pos(pos, m);
		}
		menu_print_search_msg(m);
	}
	else
	{
		move_to_menu_pos(m->d->pos, m);
		if(cfg.wrap_scan)
		{
			menu_print_search_msg(m);
		}
	}
	return 1;
}

void
menu_print_search_msg(const menu_state_t *m)
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
get_match_index(const menu_state_t *m)
{
	int n, i;

	n = (m->matches[0][0] >= 0 ? 1 : 0);
	i = 0;
	while(i++ < m->d->pos)
	{
		if(m->matches[i][0] >= 0)
		{
			++n;
		}
	}

	return n;
}

void
menus_reset_search_highlight(menu_state_t *m)
{
	m->search_highlight = 0;
	redraw_menu(m);
}

int
menu_get_matches(menu_state_t *m)
{
	return m->matching_entries;
}

void
menu_new_search(menu_state_t *m, int backward, int new_repeat_count)
{
	m->search_repeat = new_repeat_count;
	m->backward_search = backward;
	update_string(&m->regexp, NULL);
}

void
menus_replace_menu(menu_data_t *m)
{
	menu_state.current = 1;
	menu_state.matching_entries = 0;
	free(menu_state.matches);
	menu_state.matches = NULL;

	if(menu_state.d != NULL)
	{
		menu_state.d->state = NULL;
	}
	menu_state.d = m;
	m->state = &menu_state;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
