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

#define _GNU_SOURCE /* I don't know how portable this is but it is
                     * needed in Linux for the ncurses wide char
                     * functions
                     */

#include <curses.h>

#include <limits.h>

#include <wchar.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "../config.h"

#include "attr_dialog.h"
#include "bookmarks.h"
#include "cmds.h"
#include "color_scheme.h"
#include "commands.h"
#include "completion.h"
#include "config.h"
#include "filelist.h"
#include "keys.h"
#include "menu.h"
#include "menus.h"
#include "modes.h"
#include "options.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

#include "cmdline.h"

#ifndef TEST

typedef enum
{
	HIST_NONE,
	HIST_GO,
	HIST_SEARCH
}HIST;

typedef struct
{
	wchar_t *line;            /* the line reading */
	int index;                /* index of the current character */
	int curs_pos;             /* position of the cursor */
	int len;                  /* length of the string */
	int cmd_pos;              /* position in the history */
	wchar_t prompt[NAME_MAX]; /* prompt */
	int prompt_wid;           /* width of prompt */
	int complete_continue;    /* if non-zero, continue the previous completion */
	HIST history_search;      /* HIST_* */
	int hist_search_len;      /* length of history search pattern */
	wchar_t *line_buf;        /* content of line before using history */
	int reverse_completion;
	complete_cmd_func complete;
	int search_mode;
	int old_top;              /* for search_mode */
	int old_pos;              /* for search_mode */
}line_stats_t;

#endif

static int *mode;
static int prev_mode;
static CMD_LINE_SUBMODES sub_mode;
static line_stats_t input_stat;
static int line_width = 1;
static void *sub_mode_ptr;

static int def_handler(wchar_t key);
static void update_cmdline_size(void);
static void update_cmdline_text(void);
static void input_line_changed(void);
static wchar_t * wcsins(wchar_t *src, wchar_t *ins, int pos);
static void prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd,
		complete_cmd_func complete);
static void leave_cmdline_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void do_completion(void);
static void draw_wild_menu(int op);
static void cmd_ctrl_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_down(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_b(key_info_t key_info, keys_info_t *keys_info);
static void find_prev_word(void);
static void cmd_meta_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_f(key_info_t key_info, keys_info_t *keys_info);
static void find_next_word(void);
static void cmd_left(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right(key_info_t key_info, keys_info_t *keys_info);
static void cmd_home(key_info_t key_info, keys_info_t *keys_info);
static void cmd_end(key_info_t key_info, keys_info_t *keys_info);
static void cmd_delete(key_info_t key_info, keys_info_t *keys_info);
static void update_cmdline(void);
static void complete_cmd_next(void);
static void complete_search_next(void);
static void cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_up(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
static void complete_cmd_prev(void);
static void complete_search_prev(void);
#ifndef TEST
static
#endif
int line_completion(line_stats_t *stat);
static void update_line_stat(line_stats_t *stat, int new_len);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
static void stop_completion(void);

static keys_add_info_t builtin_cmds[] = {
	{L"\x03",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* backspace */
	{L"\x08",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{L"\x09",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0b",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_k}}},
	{L"\x0d",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_n}}},
	{L"\x10",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_p}}},
	/* escape */
	{L"\x1b",         {BUILTIN_WAIT_POINT, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* escape escape */
	{L"\x1b\x1b",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* ascii Delete */
	{L"\x7f",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_BACKSPACE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{{KEY_DOWN},      {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_down}}},
	{{KEY_UP},        {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_up}}},
	{{KEY_LEFT},      {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	{{KEY_RIGHT},     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	{{KEY_HOME},      {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	{{KEY_END},       {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
  {{KEY_DC},        {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
  {{KEY_BTAB},      {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
#endif /* ENABLE_EXTENDED_KEYS */
	{L"\x1b"L"[Z",    {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
	/* ctrl b */
	{L"\x02",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	/* ctrl f */
	{L"\x06",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	/* ctrl a */
	{L"\x01",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	/* ctrl e */
	{L"\x05",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
	/* ctrl d */
	{L"\x04",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
	{L"\x15",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_w}}},
#ifndef __PDCURSES__
	{L"\x1b"L"b",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_b}}},
	{L"\x1b"L"d",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_d}}},
	{L"\x1b"L"f",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_f}}},
#else
	{{ALT_B},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_b}}},
	{{ALT_D},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_d}}},
	{{ALT_F},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_f}}},
#endif
	{L"\x1f",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_underscore}}},
};

void
init_cmdline_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;
	set_def_handler(CMDLINE_MODE, def_handler);

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), CMDLINE_MODE);
	assert(ret_code == 0);
}

static int
def_handler(wchar_t key)
{
	void *p;
	wchar_t buf[2] = {key, L'\0'};

	input_stat.history_search = HIST_NONE;

	if(input_stat.complete_continue
			&& input_stat.line[input_stat.index - 1] == L'/' && key == '/')
	{
		stop_completion();
		return 0;
	}

	stop_completion();

	if(key != L'\r' && !iswprint(key))
		return 0;

	p = realloc(input_stat.line, (input_stat.len + 2) * sizeof(wchar_t));
	if(p == NULL)
	{
		leave_cmdline_mode();
		return 0;
	}

	input_stat.line = (wchar_t *) p;

	if(input_stat.len == 0)
		input_stat.line[0] = L'\0';

	input_stat.index++;
	wcsins(input_stat.line, buf, input_stat.index);
	input_stat.len++;

	input_stat.curs_pos += wcwidth(key);

	update_cmdline_size();
	update_cmdline_text();

	return 0;
}

static void
update_cmdline_size(void)
{
	int d;

	d = (input_stat.prompt_wid + input_stat.len + 1 + line_width - 1)/line_width;
	if(d >= getmaxy(status_bar))
	{
		int y = getmaxy(stdscr);

		mvwin(status_bar, y - d, 0);
		wresize(status_bar, d, line_width);

		if(prev_mode != MENU_MODE)
		{
			if(cfg.last_status)
			{
				mvwin(stat_win, y - d - 1, 0);
				wrefresh(stat_win);
			}
		}
		else
		{
			wresize(menu_win, y - d, getmaxx(stdscr));
			update_menu();
			wrefresh(menu_win);
		}
	}
}

static void
update_cmdline_text(void)
{
	int attr;

	input_line_changed();

	werase(status_bar);

	attr = cfg.cs.color[CMD_LINE_COLOR].attr;
	wattron(status_bar, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR) | attr);

	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	if(input_stat.line != NULL)
		mvwaddwstr(status_bar, input_stat.prompt_wid/line_width,
				input_stat.prompt_wid%line_width, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
	wrefresh(status_bar);
}

static void
input_line_changed(void)
{
	static wchar_t *previous;

	if(!cfg.inc_search || !input_stat.search_mode)
		return;

	if(prev_mode != MENU_MODE)
	{
		curr_view->top_line = input_stat.old_top;
		curr_view->list_pos = input_stat.old_pos;
	}
	else
	{
		load_menu_pos();
	}

	if(input_stat.line == NULL || input_stat.line[0] == L'\0')
	{
		if(cfg.hl_search)
		{
			clean_selected_files(curr_view);
			draw_dir_list(curr_view, curr_view->top_line);
			move_to_list_pos(curr_view, curr_view->list_pos);
		}
		free(previous);
		previous = NULL;
	}
	else if(previous == NULL || wcscmp(previous, input_stat.line) != 0)
	{
		char *p;

		free(previous);
		previous = my_wcsdup(input_stat.line);

		p = to_multibyte(input_stat.line);

		if(sub_mode == SEARCH_FORWARD_SUBMODE)
			exec_command(p, curr_view, GET_FSEARCH_PATTERN);
		else if(sub_mode == SEARCH_BACKWARD_SUBMODE)
			exec_command(p, curr_view, GET_BSEARCH_PATTERN);
		else if(sub_mode == MENU_SEARCH_FORWARD_SUBMODE ||
				sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
			search_menu_list(p, sub_mode_ptr);
		else if(sub_mode == VSEARCH_FORWARD_SUBMODE)
			exec_command(p, curr_view, GET_VFSEARCH_PATTERN);
		else if(sub_mode == VSEARCH_BACKWARD_SUBMODE)
			exec_command(p, curr_view, GET_VBSEARCH_PATTERN);

		free(p);
	}

	if(prev_mode != MENU_MODE)
	{
		draw_dir_list(curr_view, curr_view->top_line);
		move_to_list_pos(curr_view, curr_view->list_pos);
	}
	else
	{
		menu_redraw();
	}
}

/* Insert a string into another string
 * For example, wcsins(L"foor", L"ba", 4) returns L"foobar"
 * If pos is larger then wcslen(src), the character will
 * be added at the end of the src */
static wchar_t *
wcsins(wchar_t *src, wchar_t *ins, int pos)
{
	int i;
	wchar_t *p;

	pos--;

	for (p = src + pos; *p; p++)
		;

	for (i = 0; ins[i]; i++)
		;

	for (; p >= src + pos; p--)
		*(p + i) = *p;
	p++;

	for (; *ins; ins++, p++)
		*p = *ins;

	return src;
}

void
enter_cmdline_mode(CMD_LINE_SUBMODES cl_sub_mode, const wchar_t *cmd, void *ptr)
{
	const wchar_t *prompt;

	sub_mode_ptr = ptr;
	sub_mode = cl_sub_mode;

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
		prompt = L":";
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == VSEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == VIEW_SEARCH_FORWARD_SUBMODE)
		prompt = L"/";
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VSEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VIEW_SEARCH_BACKWARD_SUBMODE)
		prompt = L"?";
	else
		prompt = L"E";

	prepare_cmdline_mode(prompt, cmd, complete_cmd);
}

void
enter_prompt_mode(const wchar_t *prompt, const char *cmd, prompt_cb cb,
		complete_cmd_func complete)
{
	wchar_t *buf;

	sub_mode_ptr = cb;
	sub_mode = PROMPT_SUBMODE;

	buf = to_wide(cmd);
	if(buf == NULL)
		return;

	prepare_cmdline_mode(prompt, buf, complete);
	free(buf);
}

void
redraw_cmdline(void)
{
	if(prev_mode == MENU_MODE)
	{
		menu_redraw();
	}
	else
	{
		redraw_window();
		if(prev_mode == SORT_MODE)
			redraw_sort_dialog();
		else if(prev_mode == ATTR_MODE)
			redraw_attr_dialog();
	}

	line_width = getmaxx(stdscr);
	curs_set(TRUE);
	update_cmdline_size();
	update_cmdline_text();

	if(cfg.wild_menu)
		draw_wild_menu(-1);
}

static void
prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd,
		complete_cmd_func complete)
{
	line_width = getmaxx(stdscr);
	prev_mode = *mode;
	*mode = CMDLINE_MODE;

	input_stat.line = NULL;
	input_stat.index = wcslen(cmd);
	input_stat.curs_pos = 0;
	input_stat.len = input_stat.index;
	input_stat.cmd_pos = -1;
	input_stat.complete_continue = 0;
	input_stat.history_search = HIST_NONE;
	input_stat.line_buf = NULL;
	input_stat.reverse_completion = 0;
	input_stat.complete = complete;
	input_stat.search_mode = 0;

	if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == VSEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE
	    || sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VSEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
	{
		input_stat.search_mode = 1;
		if(prev_mode != MENU_MODE)
		{
			input_stat.old_top = curr_view->top_line;
			input_stat.old_pos = curr_view->list_pos;
		}
		else
		{
			save_menu_pos();
		}
	}

	wcsncpy(input_stat.prompt, prompt, ARRAY_LEN(input_stat.prompt));
	input_stat.prompt_wid = input_stat.curs_pos = wcslen(input_stat.prompt);

	if(input_stat.len != 0)
	{
		input_stat.line = malloc(sizeof(wchar_t)*(input_stat.len + 1));
		if(input_stat.line == NULL)
		{
			input_stat.index = 0;
			input_stat.len = 0;
		}
		else
		{
			wcscpy(input_stat.line, cmd);
			input_stat.curs_pos += input_stat.len;
		}
	}

	curs_set(TRUE);

	update_cmdline_size();
	update_cmdline_text();

	curr_stats.save_msg = 1;

	if(prev_mode == NORMAL_MODE)
		init_commands();
}

static void
leave_cmdline_mode(void)
{
	int attr;

	if(getmaxy(status_bar) > 1)
	{
		curr_stats.need_redraw = 2;
		wresize(status_bar, 1, getmaxx(stdscr) - 19);
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		if(prev_mode == MENU_MODE)
		{
			wresize(menu_win, getmaxy(stdscr) - 1, getmaxx(stdscr));
			update_menu();
		}
	}
	else
	{
		wresize(status_bar, 1, getmaxx(stdscr) - 19);
	}

	curs_set(FALSE);
	curr_stats.save_msg = 0;
	free(input_stat.line);
	free(input_stat.line_buf);
	clean_status_bar();

	if(*mode == CMDLINE_MODE)
		*mode = prev_mode;

	if(*mode != MENU_MODE)
		update_pos_window(curr_view);

	attr = cfg.cs.color[CMD_LINE_COLOR].attr;
	wattroff(status_bar, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR) | attr);

	if(prev_mode != MENU_MODE && prev_mode != VIEW_MODE)
	{
		draw_dir_list(curr_view, curr_view->top_line);
		move_to_list_pos(curr_view, curr_view->list_pos);
	}
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	if(input_stat.line != NULL)
		input_stat.line[0] = L'\0';
	input_line_changed();

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE)
	{
		leave_visual_mode(curr_stats.save_msg, 1, 1);
		move_to_list_pos(curr_view, check_mark_directory(curr_view, '<'));
	}
	if(sub_mode == CMD_SUBMODE)
	{
		int save_hist = !keys_info->mapped;
		curr_stats.save_msg = exec_commands("", curr_view, save_hist, GET_COMMAND);
	}
}

static void
cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index == 0 && input_stat.len == 0 && sub_mode != PROMPT_SUBMODE)
	{
		cmd_ctrl_c(key_info, keys_info);
		return;
	}
	if(input_stat.index == 0)
		return;

	if(input_stat.index == input_stat.len)
	{
		input_stat.index--;
		input_stat.len--;

		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);

		input_stat.line[input_stat.index] = L'\0';
	}
	else
	{
		input_stat.index--;
		input_stat.len--;

		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wcsdel(input_stat.line, input_stat.index + 1, 1);
	}

	update_cmdline_text();
}

static void
cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	input_stat.reverse_completion = 0;

	if(input_stat.complete_continue && get_completion_count() == 2)
		input_stat.complete_continue = 0;

	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);
}

static void
cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	input_stat.reverse_completion = 1;

	if(input_stat.complete_continue && get_completion_count() == 2)
		input_stat.complete_continue = 0;

	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);
}

static void
do_completion(void)
{
	if(input_stat.complete == NULL)
		return;

	if(input_stat.line == NULL)
	{
		input_stat.line = my_wcsdup(L"");
		if(input_stat.line == NULL)
			return;
	}

	line_completion(&input_stat);

	update_cmdline_size();
	update_cmdline_text();
}

/*
 * op == 0 - draw
 * op < 0 - redraw
 * op > 0 - reset
 */
static void
draw_wild_menu(int op)
{
	static int last_pos;

	const char ** list = get_completion_list();
	int pos = get_completion_pos();
	int count = get_completion_count() - 1;
	int i;
	int len = getmaxx(stdscr);
	
	if(sub_mode == MENU_CMD_SUBMODE || input_stat.complete == NULL)
		return;

	if(count < 2)
		return;

	if(op > 0)
	{
		last_pos = 0;
		return;
	}

	if(pos == 0 || pos == count)
		last_pos = 0;
	if(last_pos == 0 && pos == count - 1)
		last_pos = count;
	if(pos < last_pos)
	{
		int l = len;
		while(last_pos > 0 && l > 2)
		{
			last_pos--;
			l -= strlen(list[last_pos]);
			if(last_pos != 0)
				l -= 2;
		}
		if(l < 2)
			last_pos++;
	}

	werase(stat_win);
	wmove(stat_win, 0, 0);

	for(i = last_pos; i < count && len > 0; i++)
	{
		len -= strlen(list[i]);
		if(i != 0)
			len -= 2;

		if(i == last_pos && last_pos > 0)
		{
			wprintw(stat_win, "< ");
		}
		else if(i > last_pos)
		{
			if(len < 2)
			{
				wprintw(stat_win, " >");
				break;
			}
			wprintw(stat_win, "  ");
		}

		if(i == pos)
		{
			col_attr_t col;
			col = cfg.cs.color[STATUS_LINE_COLOR];
			mix_colors(&col, &cfg.cs.color[MENU_COLOR]);

			init_pair(DCOLOR_BASE + MENU_COLOR, col.fg, col.bg);
			wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + MENU_COLOR) | col.attr);
		}
		wprint(stat_win, list[i]);
		if(i == pos)
		{
			wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
					cfg.cs.color[STATUS_LINE_COLOR].attr);
			pos = -pos;
		}
	}
	if(pos > 0 && pos != count)
	{
		last_pos = pos;
		draw_wild_menu(op);
		return;
	}
	if(op == 0 && len < 2 && i - 1 == pos)
		last_pos = i;
	wrefresh(stat_win);
}

static void
cmd_ctrl_k(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index == input_stat.len)
		return;

	wcsdel(input_stat.line, input_stat.index + 1,
			input_stat.len - input_stat.index);
	input_stat.len = input_stat.index;

	update_cmdline_text();
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	char* p;
	int save_hist = !keys_info->mapped;

	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	if((!input_stat.line || !input_stat.line[0]) && sub_mode == MENU_CMD_SUBMODE)
	{
		leave_cmdline_mode();
		return;
	}

	p = input_stat.line ? to_multibyte(input_stat.line) : NULL;

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE && sub_mode != VSEARCH_FORWARD_SUBMODE &&
			sub_mode != VSEARCH_BACKWARD_SUBMODE)
		leave_visual_mode(curr_stats.save_msg, 1, 0);

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
	{
		char* s = (p != NULL) ? p : "";
		while(*s == ' ' || *s == ':')
			s++;
		if(sub_mode == CMD_SUBMODE)
			curr_stats.save_msg = exec_commands(s, curr_view, save_hist, GET_COMMAND);
		else
			curr_stats.save_msg = exec_commands(s, curr_view, save_hist,
					GET_MENU_COMMAND);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		prompt_cb cb;

		if(p != NULL && p[0] != '\0')
			save_prompt_history(p);
		modes_post();
		modes_pre();
		cb = (prompt_cb)sub_mode_ptr;
		cb(p);
	}
	else if(!cfg.inc_search || prev_mode == VIEW_MODE)
	{
		if(sub_mode == SEARCH_FORWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_FSEARCH_PATTERN);
		}
		else if(sub_mode == SEARCH_BACKWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_BSEARCH_PATTERN);
		}
		else if(sub_mode == MENU_SEARCH_FORWARD_SUBMODE ||
				sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
		{
			curr_stats.need_redraw = 1;
			search_menu_list(p, sub_mode_ptr);
		}
		else if(sub_mode == VSEARCH_FORWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_VFSEARCH_PATTERN);
		}
		else if(sub_mode == VSEARCH_BACKWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_VBSEARCH_PATTERN);
		}
		else if(sub_mode == VIEW_SEARCH_FORWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_VWFSEARCH_PATTERN);
		}
		else if(sub_mode == VIEW_SEARCH_BACKWARD_SUBMODE)
		{
			curr_stats.save_msg = exec_command(p, curr_view, GET_VWBSEARCH_PATTERN);
		}
	}

	free(p);
}

static void
save_users_input(void)
{
	free(input_stat.line_buf);
	input_stat.line_buf = my_wcsdup((input_stat.line != NULL) ?
			input_stat.line : L"");
}

static void
restore_user_input(void)
{
	input_stat.cmd_pos = -1;
	free(input_stat.line);
	input_stat.line = my_wcsdup(input_stat.line_buf);
	input_stat.len = wcslen(input_stat.line);
	update_cmdline();
}

/* Returns 0 on success */
static int
replace_input_line(const char *new)
{
	size_t len;
	wchar_t *p;

	len = mbstowcs(NULL, new, 0);
	p = realloc(input_stat.line, (len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return -1;

	input_stat.line = p;
	input_stat.len = len;
	mbstowcs(input_stat.line, new, len + 1);
	return 0;
}

static void
complete_next(char **hist, int num, size_t len)
{
	if(num < 0)
		return;

	if(input_stat.history_search != HIST_SEARCH)
	{
		if(input_stat.cmd_pos <= 0)
		{
			restore_user_input();
			return;
		}
		input_stat.cmd_pos--;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(--pos >= 0)
		{
			wchar_t *buf;

			buf = to_wide(hist[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos < 0)
		{
			restore_user_input();
			return;
		}
		input_stat.cmd_pos = pos;
	}

	replace_input_line(hist[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > len - 1)
		input_stat.cmd_pos = len - 1;
}

static void
complete_cmd_next(void)
{
	complete_next(cfg.cmd_history, cfg.cmd_history_num, cfg.cmd_history_len);
}

static void
complete_search_next(void)
{
	complete_next(cfg.search_history, cfg.search_history_num,
			cfg.search_history_len);
}

static void
complete_prompt_next(void)
{
	complete_next(cfg.prompt_history, cfg.prompt_history_num,
			cfg.prompt_history_len);
}

static void
search_next(void)
{
	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_next();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE ||
			sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_next();
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		complete_prompt_next();
	}
}

static void
cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	input_stat.history_search = HIST_GO;

	search_next();
}

#ifdef ENABLE_EXTENDED_KEYS
static void
cmd_down(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	if(input_stat.history_search != HIST_SEARCH)
	{
		input_stat.history_search = HIST_SEARCH;
		input_stat.hist_search_len = input_stat.len;
	}

	search_next();
}
#endif /* ENABLE_EXTENDED_KEYS */

static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index == 0)
		return;

	input_stat.len -= input_stat.index;

	input_stat.curs_pos = input_stat.prompt_wid;
	wcsdel(input_stat.line, 1, input_stat.index);

	input_stat.index = 0;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info)
{
	int old;

	input_stat.history_search = HIST_NONE;
	stop_completion();

	old = input_stat.index;

	while(input_stat.index > 0 && iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
	if(iswalnum(input_stat.line[input_stat.index - 1]))
	{
		while(input_stat.index > 0 &&
				iswalnum(input_stat.line[input_stat.index - 1]))
		{
			input_stat.index--;
			input_stat.curs_pos--;
		}
	}
	else
	{
		while(input_stat.index > 0 &&
				!iswalnum(input_stat.line[input_stat.index - 1]) &&
				!iswspace(input_stat.line[input_stat.index - 1]))
		{
			input_stat.index--;
			input_stat.curs_pos--;
		}
	}

	if(input_stat.index != old)
	{
		wcsdel(input_stat.line, input_stat.index + 1, old - input_stat.index);
		input_stat.len -= old - input_stat.index;
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
		return;
	rewind_completion();

	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	input_stat.reverse_completion = 0;
	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);

	stop_completion();
}

static void
cmd_meta_b(key_info_t key_info, keys_info_t *keys_info)
{
	find_prev_word();
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
find_prev_word(void)
{
	while(input_stat.index > 0 && iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
	while(input_stat.index > 0 &&
			!iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
}

static void
cmd_meta_d(key_info_t key_info, keys_info_t *keys_info)
{
	int old_i, old_c;

	old_i = input_stat.index;
	old_c = input_stat.curs_pos;
	find_next_word();

	if(input_stat.index == old_i)
	{
		return;
	}

	wcsdel(input_stat.line, old_i + 1, input_stat.index - old_i);
	input_stat.len -= input_stat.index - old_i;
	input_stat.index = old_i;
	input_stat.curs_pos = old_c;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_meta_f(key_info_t key_info, keys_info_t *keys_info)
{
	find_next_word();
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
find_next_word(void)
{
	while(input_stat.index < input_stat.len
			&& iswspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
	while(input_stat.index < input_stat.len
			&& !iswspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
}

static void
cmd_left(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index > 0)
	{
		input_stat.index--;
		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
cmd_right(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index < input_stat.len)
	{
		input_stat.curs_pos += wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
cmd_home(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = wcslen(input_stat.prompt);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_end(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid + input_stat.len;
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_delete(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index == input_stat.len)
		return;

	wcsdel(input_stat.line, input_stat.index+1, 1);
	input_stat.len--;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
complete_prev(char **hist, int num, size_t len)
{
	if(num < 0)
		return;

	if(input_stat.history_search != HIST_SEARCH)
	{
		if(input_stat.cmd_pos == num)
			return;
		input_stat.cmd_pos++;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(++pos <= num)
		{
			wchar_t *buf;

			buf = to_wide(hist[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos > num)
			return;
		input_stat.cmd_pos = pos;
	}

	replace_input_line(hist[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos >= len - 1)
		input_stat.cmd_pos = len - 1;
}

static void
complete_cmd_prev(void)
{
	complete_prev(cfg.cmd_history, cfg.cmd_history_num, cfg.cmd_history_len);
}

static void
complete_search_prev(void)
{
	complete_prev(cfg.search_history, cfg.search_history_num,
			cfg.search_history_len);
}

static void
complete_prompt_prev(void)
{
	complete_prev(cfg.prompt_history, cfg.prompt_history_num,
			cfg.prompt_history_len);
}

static void
search_prev(void)
{
	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_prev();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE ||
			sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_prev();
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		complete_prompt_prev();
	}
}

static void
cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	input_stat.history_search = HIST_GO;

	search_prev();
}

#ifdef ENABLE_EXTENDED_KEYS
static void
cmd_up(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	if(input_stat.history_search != HIST_SEARCH)
	{
		input_stat.history_search = HIST_SEARCH;
		input_stat.hist_search_len = input_stat.len;
	}

	search_prev();
}
#endif /* ENABLE_EXTENDED_KEYS */

static void
update_cmdline(void)
{
	int d;
	input_stat.curs_pos = input_stat.prompt_wid +
			wcswidth(input_stat.line, input_stat.len);
	input_stat.index = input_stat.len;

	d = (input_stat.prompt_wid + input_stat.len + 1 + line_width - 1)/line_width;
	if(d >= getmaxy(status_bar))
		update_cmdline_size();

	update_cmdline_text();
}

/*
 * p - begin of part that being completed
 * completed - new part of command line
 */
static int
line_part_complete(line_stats_t *stat, const char *line_mb, const char *p,
		const char *completed)
{
	void *t;
	wchar_t *line_ending;
	int new_len;

	new_len = (p - line_mb) + mbstowcs(NULL, completed, 0)
			+ (stat->len - stat->index) + 1;

	line_ending = my_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
		return -1;

	if((t = realloc(stat->line, new_len * sizeof(wchar_t))) == NULL)
		return -1;
	stat->line = (wchar_t *) t;

#ifndef _WIN32
	swprintf(stat->line + (p - line_mb), new_len, L"%s%ls", completed,
			line_ending);
#else
	swprintf(stat->line + (p - line_mb), L"%S%s", completed, line_ending);
#endif
	free(line_ending);

	update_line_stat(stat, new_len);
	update_cmdline_size();
	update_cmdline_text();
	return 0;
}

/* Returns non-zero on error */
#ifndef TEST
static
#endif
int
line_completion(line_stats_t *stat)
{
	static int offset;
	static char *line_mb, *line_mb_cmd;

	char *completion;
	int result;

	if(!stat->complete_continue)
	{
		int i;
		void *p;
		wchar_t t;

		/* only complete the part before the cursor
		 * so just copy that part to line_mb */
		t = stat->line[stat->index];
		stat->line[stat->index] = L'\0';

		i = wcstombs(NULL, stat->line, 0) + 1;

		if((p = realloc(line_mb, i * sizeof(char))) == NULL)
		{
			free(line_mb);
			line_mb = NULL;
			return -1;
		}

		line_mb = (char *)p;
		wcstombs(line_mb, stat->line, i);
		line_mb_cmd = find_last_command(line_mb);

		stat->line[stat->index] = t;

		reset_completion();
		offset = stat->complete(line_mb_cmd);
	}

	set_completion_order(input_stat.reverse_completion);

	if(get_completion_count() == 0)
		return 0;

	completion = next_completion();
	result = line_part_complete(stat, line_mb, line_mb_cmd + offset, completion);
	free(completion);

	if(get_completion_count() >= 2)
		stat->complete_continue = 1;

	return result;
}

static void
update_line_stat(line_stats_t *stat, int new_len)
{
	stat->index += (new_len - 1) - stat->len;
	stat->curs_pos = stat->prompt_wid + wcswidth(stat->line, stat->index);
	stat->len = new_len - 1;
}

/* Delete a character in a string
 * For example, wcsdelch(L"fooXXbar", 4, 2) returns L"foobar" */
static wchar_t *
wcsdel(wchar_t *src, int pos, int len)
{
	int p;

	pos--;

	for(p = pos; pos - p < len; pos++)
		if(! src[pos])
		{
			src[p] = L'\0';
			return src;
		}

	pos--;

	while(src[++pos] != L'\0')
		src[pos-len] = src[pos];
	src[pos-len] = src[pos];

	return src;
}

static void
stop_completion(void)
{
	if(!input_stat.complete_continue)
		return;

	input_stat.complete_continue = 0;
	reset_completion();
	if(cfg.wild_menu &&
			(sub_mode != MENU_CMD_SUBMODE && input_stat.complete != NULL))
	{
		if(cfg.last_status)
		{
			update_stat_window(curr_view);
		}
		else
		{
			touchwin(lwin.win);
			touchwin(rwin.win);
			touchwin(lborder);
			touchwin(mborder);
			touchwin(rborder);
			wnoutrefresh(lwin.win);
			wnoutrefresh(rwin.win);
			wnoutrefresh(lborder);
			wnoutrefresh(mborder);
			wnoutrefresh(rborder);
			doupdate();
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
