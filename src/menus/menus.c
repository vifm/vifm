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
#include "../engine/mode.h"
#include "../int/term_title.h"
#include "../int/vim.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../modes/modes.h"
#include "../ui/cancellation.h"
#include "../ui/color_scheme.h"
#include "../ui/colors.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/mem.h"
#include "../utils/path.h"
#include "../utils/regexp.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../flist_pos.h"
#include "../macros.h"
#include "../marks.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"

static void deinit_menu_data(menu_data_t *m);
static void show_position_in_menu(const menu_data_t *m);
static void open_selected_file(const char path[], int line_num);
static void navigate_to_selected_file(view_t *view, const char path[]);
static void draw_menu_item(menu_state_t *ms, int pos, int line, int clear);
static void draw_search_match(char str[], int start, int end, int line,
		int width, const cchar_t *attrs);
static void normalize_top(menu_state_t *ms);
static void draw_menu_frame(const menu_state_t *ms);
static void output_handler(const char line[], void *arg);
static void append_to_string(char **str, const char suffix[]);
static char * expand_tabulation_a(const char line[], size_t tab_stops);
static void init_menu_state(menu_state_t *ms, menu_data_t *m, view_t *view);
static void replace_menu_data(menu_data_t *m);
static int can_stash_menu(const menu_data_t *m);
static void stash_menu(menu_data_t *m);
static int stash_is_displayed(void);
static void unstash_menu_at(menu_state_t *ms, int index);
static void move_menu_data(menu_data_t *to, menu_data_t *from);
static const char * get_relative_path_base(const menu_data_t *m,
		const view_t *view);
static int menu_and_view_are_in_sync(const menu_data_t *m, const view_t *view);
static int search_menu(menu_state_t *ms, int print_errors);
static int search_menu_forwards(menu_state_t *ms, int start_pos);
static int search_menu_backwards(menu_state_t *ms, int start_pos);
static int navigate_to_match(menu_state_t *ms, int pos);
static int get_match_index(const menu_state_t *ms);
TSTATIC void menus_drop_stash(void);
TSTATIC void menus_set_active(menu_data_t *m);

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
	/* View associated with the menu (e.g. to navigate to a file in it). */
	view_t *view;
}
menu_state;

/* Storage for data of stashable menus in chronological order (newest to
 * oldest). */
static menu_data_t menu_data_stash[10];
/* Index of the last viewed stashed menu. */
static int menu_stash_index;
/* Number of stashed menus. */
static int menu_stash_depth;

void
menus_remove_current(menu_state_t *ms)
{
	menu_data_t *const m = ms->d;
	menus_erase_current(ms);

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
	menus_partial_redraw(ms);

	menus_set_pos(ms, m->pos);
}

void
menus_erase_current(menu_state_t *ms)
{
	draw_menu_item(ms, ms->d->pos, ms->current, 1);
}

void
menus_init_data(menu_data_t *m, view_t *view, char title[], char empty_msg[])
{
	if(m->initialized)
	{
		menus_reset_data(m);
	}

	m->title = escape_unreadable(title);
	free(title);

	m->top = 0;
	m->len = 0;
	m->pos = 0;
	m->hor_pos = 0;
	m->items = NULL;
	m->data = NULL;
	m->void_data = NULL;
	m->key_handler = NULL;
	m->extra_data = 0;
	m->execute_handler = NULL;
	m->empty_msg = empty_msg;
	m->cwd = strdup(flist_get_dir(view));
	m->state = &menu_state;
	m->initialized = 1;
}

void
menus_reset_data(menu_data_t *m)
{
	if(!m->initialized)
	{
		return;
	}

	if(can_stash_menu(m))
	{
		stash_menu(m);
	}
	else
	{
		deinit_menu_data(m);
	}
}

/* Frees resources taken up by data at most once.  Accepts zero-initialized
 * input as well. */
static void
deinit_menu_data(menu_data_t *m)
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
	free(m->cwd);
	m->initialized = 0;
}

void
menus_set_pos(menu_state_t *ms, int pos)
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
	if(pos > modmenu_last_line(m))
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
		if(pos > modmenu_last_line(m) - s)
		{
			m->top += s - (modmenu_last_line(m) - pos);
			normalize_top(ms);
			redraw = 1;
		}
	}

	ms->current = 1 + (pos - m->top);
	m->pos = pos;

	if(redraw)
	{
		menus_partial_redraw(ms);
	}
	else
	{
		draw_menu_item(ms, m->pos, ms->current, 0);
		show_position_in_menu(m);
	}

	checked_wmove(menu_win, ms->current, 2);
}

/* Displays current menu position on a ruler. */
static void
show_position_in_menu(const menu_data_t *m)
{
	if(modes_is_cmdline_like())
	{
		return;
	}

	char pos_buf[32];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);

	ui_ruler_set(pos_buf);
}

void
menus_full_redraw(menu_state_t *ms)
{
	if(resize_for_menu_like() != 0)
	{
		return;
	}

	ms->win_rows = getmaxy(menu_win);

	menus_partial_redraw(ms);
	menus_set_pos(ms, ms->d->pos);
	ui_refresh_win(menu_win);
}

int
menus_goto_file(menu_data_t *m, view_t *view, const char spec[], int try_open)
{
	char *path_buf;
	int line_num;

	path_buf = parse_file_spec(spec, &line_num, get_relative_path_base(m, view));
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
navigate_to_selected_file(view_t *view, const char path[])
{
	char name[NAME_MAX + 1];
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

		(void)fpos_ensure_selected(view, name);
	}
	else
	{
		show_error_msgf("Invalid path", "Cannot change dir to \"%s\"", dir);
	}

	free(dir);
}

void
menus_goto_dir(view_t *view, const char path[])
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
menus_partial_redraw(menu_state_t *ms)
{
	int i, pos;
	const int y = getmaxy(menu_win);

	normalize_top(ms);

	werase(menu_win);
	draw_menu_frame(ms);

	for(i = 0, pos = ms->d->top; i < y - 2 && pos < ms->d->len; ++i, ++pos)
	{
		draw_menu_item(ms, pos, i + 1, 0);
	}

	show_position_in_menu(ms->d);
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
	int color_pair = cs_load_color(&col);

	char *escaped = escape_unreadable(m->items[pos]);

	/* Calculate offset of m->hor_pos's character in item text. */
	off = 0;
	i = m->hor_pos;
	while(i-- > 0 && escaped[0] != '\0')
	{
		off += utf8_chrw(escaped + off);
	}

	item_tail = escaped + off;
	replace_char(item_tail, '\t', ' ');

	ui_set_attr(menu_win, &col, color_pair);

	/* Clear the area. */
	checked_wmove(menu_win, line, 1);
	if(curr_stats.load_stage > 0)
	{
		wprintw(menu_win, "%*s", width, "");
	}

	/* Truncate the item to fit the screen if needed. */
	if(utf8_strsw(item_tail) > (size_t)(width - 2))
	{
		char *ellipsed = right_ellipsis(item_tail, width - 2, curr_stats.ellipsis);
		free(escaped);
		escaped = ellipsed;
		item_tail = ellipsed;
	}
	else
	{
		const size_t len = utf8_nstrsnlen(item_tail, width - 2 + 1);
		item_tail[len] = '\0';
	}

	checked_wmove(menu_win, line, 2);
	wprint(menu_win, item_tail);

	if(ms->search_highlight && ms->matches != NULL && ms->matches[pos][0] >= 0)
	{
		const cchar_t cch = cs_color_to_cchar(&col, color_pair);
		draw_search_match(item_tail, ms->matches[pos][0] - off,
				ms->matches[pos][1] - off, line, width, &cch);
	}

	free(escaped);
}

/* Draws search match highlight on the element. */
static void
draw_search_match(char str[], int start, int end, int line, int width,
		const cchar_t *attrs)
{
	if(start == end)
	{
		/* Empty match is not drawn. */
		return;
	}

	const int len = strlen(str);

	/* XXX: conditions for displaying <<< and >>> are for some reason different in
	 *      file views. */

	if(end <= 0)
	{
		/* Match is completely on the left. */

		checked_wmove(menu_win, line, 2);
		wprinta(menu_win, "<<<", attrs, A_REVERSE);
	}
	else if(start >= len)
	{
		/* Match is completely on the right. */

		checked_wmove(menu_win, line, width - 3);
		wprinta(menu_win, ">>>", attrs, A_REVERSE);
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
		wprinta(menu_win, str + start, attrs, A_REVERSE | A_UNDERLINE);
	}
}

/* Ensures that value of m->top lies in a correct range. */
static void
normalize_top(menu_state_t *ms)
{
	ms->d->top = MAX(0, MIN(ms->d->len - (ms->win_rows - 2), ms->d->top));
}

/* Draws box and title of the menu. */
static void
draw_menu_frame(const menu_state_t *ms)
{
	char *title = menus_format_title(ms->d, ms->view);
	if(stash_is_displayed())
	{
		char *full_title = format_str("[%d/%d] %s",
				menu_stash_depth - menu_stash_index, menu_stash_depth, title);
		free(title);
		title = full_title;
	}

	const size_t title_len = getmaxx(menu_win) - 2*4;
	char *const ellipsed = right_ellipsis(title, title_len, curr_stats.ellipsis);
	free(title);

	ui_set_attr(menu_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);

	box(menu_win, 0, 0);
	wattron(menu_win, A_BOLD);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, " ");
	wprint(menu_win, ellipsed);
	wprint(menu_win, " ");
	wattroff(menu_win, A_BOLD);

	free(ellipsed);
}

char *
menus_format_title(const menu_data_t *m, struct view_t *view)
{
	const char *const suffix = menu_and_view_are_in_sync(m, view)
	                         ? ""
	                         : replace_home_part(m->cwd);
	const char *const at = (suffix[0] == '\0' ? "" : " @ ");
	return format_str("%s%s%s", m->title, at, suffix);
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
menus_enter(menu_data_t *m, view_t *view)
{
	if(m->len < 1)
	{
		ui_sb_msg(m->empty_msg);
		menus_reset_data(m);
		return 1;
	}

	if(vle_mode_is(MENU_MODE))
	{
		menus_switch_to(m);
		return 0;
	}

	init_menu_state(m->state, m, view);

	ui_setup_for_menu_like();
	term_title_update(m->title);
	menus_partial_redraw(m->state);
	menus_set_pos(m->state, m->pos);
	modmenu_enter(m, view);
	return 0;
}

/* Initializes menu state structure with default/initial values. */
static void
init_menu_state(menu_state_t *ms, menu_data_t *m, view_t *view)
{
	free(ms->regexp);
	free(ms->matches);

	ms->d = m;
	ms->current = 1;
	ms->win_rows = getmaxy(menu_win);
	ms->backward_search = 0;
	ms->matching_entries = 0;
	ms->search_highlight = 1;
	ms->matches = NULL;
	ms->regexp = NULL;
	ms->search_repeat = 0;
	ms->view = view;
}

void
menus_switch_to(menu_data_t *m)
{
	menu_state_t *ms = m->state;

	if(stash_is_displayed())
	{
		move_menu_data(&menu_data_stash[menu_stash_index], ms->d);
	}
	else if(ms->d != NULL && can_stash_menu(ms->d))
	{
		stash_menu(ms->d);
	}

	replace_menu_data(m);
}

/* Changes active menu data. */
static void
replace_menu_data(menu_data_t *m)
{
	modmenu_set_data(m);

	menu_state.current = 1;
	menu_state.matching_entries = 0;
	free(menu_state.matches);
	menu_state.matches = NULL;

	menu_state.d = m;

	menus_partial_redraw(m->state);
	menus_set_pos(m->state, m->pos);
	ui_refresh_win(menu_win);
}

char *
menus_get_targets(view_t *view)
{
	if(view->selected_files > 0 ||
			(view->pending_marking && flist_count_marked(view) > 0))
	{
		return ma_expand("%f", NULL, NULL, MER_SHELL_OP);
	}

	if(!flist_custom_active(view))
	{
		return strdup(".");
	}

	return (vifm_chdir(flist_get_dir(view)) == 0) ? strdup(".") : NULL;
}

/* Checks whether menu can be stashed.  Returns non-zero if so. */
static int
can_stash_menu(const menu_data_t *m)
{
	/* Interested only in non-empty stashable menus. */
	return (m->stashable && m->len > 0);
}

/* Stores menu data for restoring it later. */
static void
stash_menu(menu_data_t *m)
{
	if(stash_is_displayed())
	{
		/* Re-use the same stash and do not drop newer menus until another one is
		 * added. */
	}
	else if(menu_stash_index > 0)
	{
		/* Release previously stashed menus from the head of the stack, if any. */
		int i;
		for(i = 0; i < menu_stash_index; ++i)
		{
			deinit_menu_data(&menu_data_stash[i]);
		}

		/* Drop the head. */
		mem_shl(menu_data_stash, ARRAY_LEN(menu_data_stash),
				sizeof(menu_data_stash[0]), /*offset=*/(menu_stash_index - 1));
		menu_stash_depth -= menu_stash_index - 1;
		menu_stash_index = 0;
	}
	else
	{
		if(++menu_stash_depth > (int)ARRAY_LEN(menu_data_stash))
		{
			deinit_menu_data(&menu_data_stash[ARRAY_LEN(menu_data_stash) - 1]);
			menu_stash_depth = ARRAY_LEN(menu_data_stash);
		}

		mem_shr(menu_data_stash, ARRAY_LEN(menu_data_stash),
				sizeof(menu_data_stash[0]), /*offset=*/1);

		/* We stole data of this item while moving memory. */
		menu_data_stash[0].initialized = 0;
	}

	move_menu_data(&menu_data_stash[menu_stash_index], m);
}

/* Checks whether menu displays a stash.  Returns non-zero if so. */
static int
stash_is_displayed(void)
{
	return menu_stash_index < menu_stash_depth
	    && !menu_data_stash[menu_stash_index].initialized;
}

void
menus_put_on_stash(menu_state_t *ms)
{
	if(can_stash_menu(ms->d))
	{
		stash_menu(ms->d);

		/* Unstash the menu to keep using it. */
		move_menu_data(ms->d, &menu_data_stash[menu_stash_index]);
	}
}

int
menus_unstash(view_t *view)
{
	static menu_data_t menu_data_storage;

	if(menu_stash_depth == 0)
	{
		ui_sb_msg("No saved menu to display");
		return 1;
	}

	move_menu_data(&menu_data_storage, &menu_data_stash[menu_stash_index]);
	menu_state.d = &menu_data_storage;

	return menus_enter(&menu_data_storage, view);
}

int
menus_unstash_older(menu_state_t *ms)
{
	/* Starting viewing stashes requires special handling. */
	if(!stash_is_displayed())
	{
		if(menu_stash_depth == 0)
		{
			/* An older stash doesn't exist. */
			return 1;
		}

		if(!can_stash_menu(ms->d))
		{
			/* Behave as :copen in this case. */
			unstash_menu_at(ms, menu_stash_index);
			return 0;
		}

		/* Stash current menu the same way it would be done on closing it and
		 * proceed as usual, again producing the same result as leaving the mode,
		 * running :copen and then :colder. */
		stash_menu(ms->d);
	}

	if(menu_stash_index >= menu_stash_depth - 1)
	{
		return 1;
	}

	unstash_menu_at(ms, menu_stash_index + 1);
	return 0;
}

int
menus_unstash_newer(menu_state_t *ms)
{
	if((can_stash_menu(ms->d) && !stash_is_displayed()) || menu_stash_index == 0)
	{
		return 1;
	}

	unstash_menu_at(ms, menu_stash_index - 1);
	return 0;
}

void
menus_unstash_at(menu_state_t *ms, int index)
{
	unstash_menu_at(ms, menu_stash_depth - 1 - index);
}

/* Loads a stashed menu specified by its index.  If another stashed menu was
 * active, it's saved back to the stash. */
static void
unstash_menu_at(menu_state_t *ms, int index)
{
	static menu_data_t menu_data_storage;

	if(stash_is_displayed())
	{
		move_menu_data(&menu_data_stash[menu_stash_index], ms->d);
	}

	move_menu_data(&menu_data_storage, &menu_data_stash[index]);

	menu_stash_index = index;
	replace_menu_data(&menu_data_storage);
}

/* Changes location of menu data while managing its initialized flag. */
static void
move_menu_data(menu_data_t *to, menu_data_t *from)
{
	assert(to != from && "Can't move menu data into itself.");
	assert(from->initialized && "Uninitialized menus shouldn't be moved.");

	deinit_menu_data(to);
	*to = *from;
	from->initialized = 0;
}

const menu_data_t *
menus_get_stash(int index, int *current)
{
	if(index >= menu_stash_depth)
	{
		return NULL;
	}

	int real_index = menu_stash_depth - 1 - index;
	*current = (real_index == menu_stash_index);
	return &menu_data_stash[real_index];
}

KHandlerResponse
menus_def_khandler(view_t *view, menu_data_t *ms, const wchar_t keys[])
{
	if(wcscmp(keys, L"gf") == 0)
	{
		(void)menus_goto_file(ms, view, ms->items[ms->pos], 0);
		return KHR_CLOSE_MENU;
	}
	else if(wcscmp(keys, L"e") == 0)
	{
		(void)menus_goto_file(ms, view, ms->items[ms->pos], 1);
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"c") == 0)
	{
		/* Insert just file name. */
		int line_num;
		const char *const rel_base = get_relative_path_base(ms, view);
		char *const path = parse_file_spec(ms->items[ms->pos], &line_num, rel_base);
		if(path == NULL)
		{
			show_error_msg("Command insertion", "No valid filename found");
			return KHR_REFRESH_WINDOW;
		}
		modmenu_morph_into_cline(CLS_COMMAND, path, 1);
		free(path);
		return KHR_MORPHED_MENU;
	}

	return KHR_UNHANDLED;
}

int
menus_to_custom_view(menu_state_t *ms, view_t *view, int very)
{
	int i;
	char *current = NULL;
	const char *const rel_base = get_relative_path_base(ms->d, view);

	flist_custom_start(view, ms->d->title);

	for(i = 0; i < ms->d->len; ++i)
	{
		char *path;
		int line_num;

		/* Skip empty lines. */
		if(skip_whitespace(ms->d->items[i])[0] == '\0')
		{
			continue;
		}

		path = parse_file_spec(ms->d->items[i], &line_num, rel_base);
		if(path == NULL)
		{
			continue;
		}

		flist_custom_add(view, path);

		/* Use either exact position or the next path. */
		if(i == ms->d->pos || (current == NULL && i > ms->d->pos))
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
		char full_path[PATH_MAX + 1];
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

/* Gets base for relative paths navigated to from the menu.  Returns the
 * path.  The purpose is to omit "././" or "..././..." in paths shown to a
 * user. */
static const char *
get_relative_path_base(const menu_data_t *m, const view_t *view)
{
	if(menu_and_view_are_in_sync(m, view))
	{
		return ".";
	}
	return m->cwd;
}

/* Checks whether menu working directory and current directory of the view are
 * in sync.  Returns non-zero if so, otherwise zero is returned. */
static int
menu_and_view_are_in_sync(const menu_data_t *m, const view_t *view)
{
	/* NULL check is for tests. */
	return (view == NULL || paths_are_same(m->cwd, flist_get_dir(view)));
}

int
menus_capture(view_t *view, const char cmd[], int user_sh, menu_data_t *m,
		MacroFlags flags)
{
	if(ma_flags_present(flags, MF_CUSTOMVIEW_OUTPUT) ||
			ma_flags_present(flags, MF_VERYCUSTOMVIEW_OUTPUT))
	{
		rn_for_flist(view, cmd, m->title, user_sh, flags);
		menus_reset_data(m);
		return 0;
	}

	FILE *input_tmp = make_in_file(view, flags);

	if(process_cmd_output("Loading menu", cmd, input_tmp, user_sh, 0,
				&output_handler, m) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 0;
	}

	if(input_tmp != NULL)
	{
		fclose(input_tmp);
	}

	if(ui_cancellation_requested())
	{
		append_to_string(&m->title, "(cancelled)");
		append_to_string(&m->empty_msg, " (cancelled)");
	}

	return menus_enter(m, view);
}

void
menus_search_repeat(menu_state_t *ms, int backward)
{
	if(is_null_or_empty(ms->regexp))
	{
		ui_sb_err("No search pattern set");
		curr_stats.save_msg = 1;
		return;
	}

	ms->backward_search = backward;
	(void)menus_search(NULL, ms->d, 1);
	ui_refresh_win(menu_win);

	if(ms->matching_entries > 0)
	{
		ui_sb_msgf("(%d of %d) %c%s", get_match_index(ms), ms->matching_entries,
				backward ? '?' : '/', ms->regexp);
	}

	curr_stats.save_msg = 1;
}

int
menus_search(const char pattern[], menu_data_t *m, int print_errors)
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
		if(search_menu(ms, print_errors) != 0)
		{
			menus_partial_redraw(ms);
			menus_set_pos(ms, m->pos);
			return -1;
		}
		menus_partial_redraw(ms);
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
search_menu(menu_state_t *ms, int print_errors)
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
	err = regexp_compile(&re, ms->regexp, cflags);
	if(err != 0)
	{
		if(print_errors)
		{
			ui_sb_errf("Regexp error: %s", get_regexp_error(err, &re));
		}
		regfree(&re);
		return -1;
	}

	for(i = 0; i < m->len; ++i)
	{
		regmatch_t matches[1];
		const char *item = m->items[i];
		if(regexec(&re, item, 1, matches, 0) == 0)
		{
			ms->matches[i][0] = matches[0].rm_so
			                  + escape_unreadableo(item, matches[0].rm_so);
			ms->matches[i][1] = matches[0].rm_eo
			                  + escape_unreadableo(item, matches[0].rm_eo);

			++ms->matching_entries;
		}
	}
	regfree(&re);
	return 0;
}

/* Looks for next matching element in forward direction from current position.
 * Returns new value for save_msg flag. */
static int
search_menu_forwards(menu_state_t *ms, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = 0; i < ms->d->len; ++i)
	{
		if(ms->matches[i][0] < 0)
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
		ui_sb_errf("Search hit BOTTOM without match for: %s", ms->regexp);
		return 1;
	}

	return navigate_to_match(ms, (match_down > -1) ? match_down : match_up);
}

/* Looks for next matching element in backward direction from current position.
 * Returns new value for save_msg flag. */
static int
search_menu_backwards(menu_state_t *ms, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int i;

	for(i = ms->d->len - 1; i > -1; --i)
	{
		if(ms->matches[i][0] < 0)
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
		ui_sb_errf("Search hit TOP without match for: %s", ms->regexp);
		return 1;
	}

	return navigate_to_match(ms, (match_up > -1) ? match_up : match_down);
}

/* Tries to navigate to menu search match specified via pos argument.  If pos is
 * negative, match wasn't found and the message is printed.  Returns new value
 * for save_msg flag. */
static int
navigate_to_match(menu_state_t *ms, int pos)
{
	if(pos > -1)
	{
		if(!ms->search_highlight)
		{
			/* Might need to highlight other items, so redraw whole menu. */
			ms->search_highlight = 1;
			ms->d->pos = pos;
			menus_partial_redraw(ms);
		}
		else
		{
			menus_erase_current(ms);
			menus_set_pos(ms, pos);
		}
		menus_search_print_msg(ms->d);
	}
	else
	{
		menus_set_pos(ms, ms->d->pos);
		if(cfg.wrap_scan)
		{
			menus_search_print_msg(ms->d);
		}
	}
	return 1;
}

void
menus_search_print_msg(const menu_data_t *m)
{
	const menu_state_t *ms = m->state;
	int cflags;
	regex_t re;
	int err;

	/* Can be NULL after regex compilation failure. */
	if(ms->regexp == NULL)
	{
		return;
	}

	cflags = get_regexp_cflags(ms->regexp);
	err = regexp_compile(&re, ms->regexp, cflags);

	if(err != 0)
	{
		ui_sb_errf("Regexp (%s) error: %s", ms->regexp, get_regexp_error(err, &re));
		regfree(&re);
		return;
	}

	regfree(&re);

	if(ms->matching_entries > 0)
	{
		ui_sb_msgf("%d of %d %s", get_match_index(ms), ms->matching_entries,
				(ms->matching_entries == 1) ? "match" : "matches");
	}
	else
	{
		ui_sb_errf("No matches for: %s", ms->regexp);
	}
}

/* Calculates the index of the current match from the list of matches.  Returns
 * the index. */
static int
get_match_index(const menu_state_t *ms)
{
	int n, i;

	n = (ms->matches[0][0] >= 0 ? 1 : 0);
	i = 0;
	while(i++ < ms->d->pos)
	{
		if(ms->matches[i][0] >= 0)
		{
			++n;
		}
	}

	return n;
}

void
menus_search_reset_hilight(menu_state_t *ms)
{
	ms->search_highlight = 0;
	menus_full_redraw(ms);
}

int
menus_search_matched(const menu_data_t *m)
{
	return m->state->matching_entries;
}

void
menus_search_reset(menu_state_t *ms, int backward, int new_repeat_count)
{
	ms->search_repeat = new_repeat_count;
	ms->backward_search = backward;
	update_string(&ms->regexp, NULL);
}

/* Clears stashed navigation menus. */
TSTATIC void
menus_drop_stash(void)
{
	int i;
	for(i = 0; i < menu_stash_depth; ++i)
	{
		deinit_menu_data(&menu_data_stash[i]);
	}

	menu_stash_depth = 0;
	menu_stash_index = 0;
}

TSTATIC void
menus_set_active(menu_data_t *m)
{
	menu_state.d = m;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
