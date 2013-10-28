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

#include "cmdline.h"

#include <curses.h>

#include <limits.h>

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h>
#include <wchar.h> /* wcswidth() */
#include <wctype.h>

#include "../cfg/config.h"
#include "../engine/cmds.h"
#include "../engine/completion.h"
#include "../engine/keys.h"
#include "../engine/options.h"
#include "../menus/menus.h"
#include "../utils/fs_limits.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utils.h"
#include "../bookmarks.h"
#include "../color_scheme.h"
#include "../commands.h"
#include "../filelist.h"
#include "../search.h"
#include "../status.h"
#include "../ui.h"
#include "dialogs/attr_dialog.h"
#include "dialogs/sort_dialog.h"
#include "menu.h"
#include "modes.h"
#include "visual.h"

#ifndef TEST

typedef enum
{
	HIST_NONE,
	HIST_GO,
	HIST_SEARCH
}
HIST;

/* Holds state of the command-line editing mode. */
typedef struct
{
	wchar_t *line;            /* the line reading */
	wchar_t *initial_line;    /* initial state of the line */
	int index;                /* index of the current character in cmdline */
	int curs_pos;             /* position of the cursor in status bar*/
	int len;                  /* length of the string */
	int cmd_pos;              /* position in the history */
	wchar_t prompt[NAME_MAX]; /* prompt */
	int prompt_wid;           /* width of prompt */
	int complete_continue;    /* if non-zero, continue the previous completion */
	int dot_pos;              /* history position for dot completion, or < 0 */
	size_t dot_index;         /* dot completion line index */
	size_t dot_len;           /* dot completion previous completion len */
	HIST history_search;      /* HIST_* */
	int hist_search_len;      /* length of history search pattern */
	wchar_t *line_buf;        /* content of line before using history */
	int reverse_completion;
	complete_cmd_func complete;
	int search_mode;
	int old_top;              /* for search_mode */
	int old_pos;              /* for search_mode */
	int line_edited;          /* Cache for whether input line changed flag. */
}
line_stats_t;

#endif

static int *mode;
static int prev_mode;
static CMD_LINE_SUBMODES sub_mode;
static line_stats_t input_stat;
static int line_width = 1;
static void *sub_mode_ptr;
static int sub_mode_allows_ee;

static int def_handler(wchar_t key);
static void update_cmdline_size(void);
static void update_cmdline_text(void);
static void input_line_changed(void);
static void set_local_filter(const char value[]);
static wchar_t * wcsins(wchar_t src[], const wchar_t ins[], int pos);
static void prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd,
		complete_cmd_func complete);
static void save_view_port(void);
static void set_view_port(void);
static int is_line_edited(void);
static void leave_cmdline_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static int submode_to_editable_command_type(int sub_mode);
static void extedit_prompt(const char input[], int cursor_col);
static void cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info);
static int should_quit_on_backspace(void);
static int no_initial_line(void);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void do_completion(void);
static void draw_wild_menu(int op);
static void cmd_ctrl_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void save_input_to_history(const keys_info_t *keys_info,
		const char input[]);
static void finish_prompt_submode(const char input[]);
static int is_forward_search(CMD_LINE_SUBMODES sub_mode);
static int is_backward_search(CMD_LINE_SUBMODES sub_mode);
static void cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_down(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
static void search_next(void);
static void complete_next(const hist_t *hist, size_t len);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_b(key_info_t key_info, keys_info_t *keys_info);
static void find_prev_word(void);
static void cmd_meta_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_dot(key_info_t key_info, keys_info_t *keys_info);
static void remove_previous_dot_completion(void);
static wchar_t * next_dot_completion(void);
static int insert_dot_completion(const wchar_t completion[]);
static void find_next_word(void);
static void cmd_left(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right(key_info_t key_info, keys_info_t *keys_info);
static void cmd_home(key_info_t key_info, keys_info_t *keys_info);
static void cmd_end(key_info_t key_info, keys_info_t *keys_info);
static void cmd_delete(key_info_t key_info, keys_info_t *keys_info);
static void update_cursor(void);
static void search_prev(void);
static void complete_prev(const hist_t *hist, size_t len);
static int replace_input_line(const char new[]);
static void update_cmdline(void);
static void cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_t(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_up(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
TSTATIC int line_completion(line_stats_t *stat);
static void update_line_stat(line_stats_t *stat, int new_len);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
static void stop_completion(void);
static void stop_dot_completion(void);
static void stop_history_completion(void);

static keys_add_info_t builtin_cmds[] = {
	{L"\x03",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x07",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_g}}},
	/* backspace */
	{L"\x08",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{L"\x09",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0b",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_k}}},
	{L"\x0d",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_n}}},
	{L"\x10",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_p}}},
	{L"\x14",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_t}}},
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
	{L"\x1b"L".",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_dot}}},
#else
	{{ALT_B},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_b}}},
	{{ALT_D},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_d}}},
	{{ALT_F},         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_f}}},
	{{ALT_PERIOD},    {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_dot}}},
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

	input_stat.curs_pos += vifm_wcwidth(key);

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

/* Also updates cursor. */
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
	update_cursor();
	wrefresh(status_bar);
}

/* Callback-like function, which is called every time input line is changed. */
static void
input_line_changed(void)
{
	static wchar_t *previous;

	if(!cfg.inc_search || (!input_stat.search_mode && sub_mode != FILTER_SUBMODE))
		return;

	set_view_port();

	if(input_stat.line == NULL || input_stat.line[0] == L'\0')
	{
		if(cfg.hl_search)
		{
			/* clear selection */
			if(prev_mode != MENU_MODE)
			{
				clean_selected_files(curr_view);
			}
			else
			{
				search_menu_list("", sub_mode_ptr);
			}
		}
		free(previous);
		previous = NULL;

		if(sub_mode == FILTER_SUBMODE)
		{
			set_local_filter("");
		}
	}
	else if(previous == NULL || wcscmp(previous, input_stat.line) != 0)
	{
		char *mbinput;

		free(previous);
		previous = my_wcsdup(input_stat.line);

		mbinput = to_multibyte(input_stat.line);

		switch(sub_mode)
		{
			case SEARCH_FORWARD_SUBMODE:
				exec_command(mbinput, curr_view, GET_FSEARCH_PATTERN);
				break;
			case SEARCH_BACKWARD_SUBMODE:
				exec_command(mbinput, curr_view, GET_BSEARCH_PATTERN);
				break;
			case VSEARCH_FORWARD_SUBMODE:
				exec_command(mbinput, curr_view, GET_VFSEARCH_PATTERN);
				break;
			case VSEARCH_BACKWARD_SUBMODE:
				exec_command(mbinput, curr_view, GET_VBSEARCH_PATTERN);
				break;
			case MENU_SEARCH_FORWARD_SUBMODE:
			case MENU_SEARCH_BACKWARD_SUBMODE:
				search_menu_list(mbinput, sub_mode_ptr);
				break;
			case FILTER_SUBMODE:
				set_local_filter(mbinput);
				break;

			default:
				assert("Unexpected filter type.");
				break;
		}

		free(mbinput);
	}

	if(prev_mode != MENU_MODE && prev_mode != VISUAL_MODE)
	{
		redraw_current_view();
	}
	else if(prev_mode != VISUAL_MODE)
	{
		menu_redraw();
	}
}

/* Updates value of the local filter of the current view. */
static void
set_local_filter(const char value[])
{
	const int rel_pos = input_stat.old_pos - input_stat.old_top;
	local_filter_set(curr_view, value);
	local_filter_update_view(curr_view, rel_pos);
}

/* Insert a string into another string
 * For example, wcsins(L"foor", L"ba", 4) returns L"foobar"
 * If pos is larger then wcslen(src), the character will
 * be added at the end of the src */
static wchar_t *
wcsins(wchar_t src[], const wchar_t ins[], int pos)
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
	sub_mode_allows_ee = 0;

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
	{
		prompt = L":";
	}
	else if(sub_mode == FILTER_SUBMODE)
	{
		prompt = L"=";
	}
	else if(is_forward_search(sub_mode))
	{
		prompt = L"/";
	}
	else if(is_backward_search(sub_mode))
	{
		prompt = L"?";
	}
	else
	{
		assert(0 && "Unknown command line submode.");
		prompt = L"E";
	}

	prepare_cmdline_mode(prompt, cmd, complete_cmd);
}

void
enter_prompt_mode(const wchar_t prompt[], const char cmd[], prompt_cb cb,
		complete_cmd_func complete, int allow_ee)
{
	wchar_t *buf;

	sub_mode_ptr = cb;
	sub_mode = PROMPT_SUBMODE;
	sub_mode_allows_ee = allow_ee;

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
		update_screen(UT_FULL);
		if(prev_mode == SORT_MODE)
			redraw_sort_dialog();
		else if(prev_mode == ATTR_MODE)
			redraw_attr_dialog();
	}

	line_width = getmaxx(stdscr);
	curs_set(TRUE);
	update_cmdline_size();
	update_cmdline_text();

	if(input_stat.complete_continue && cfg.wild_menu)
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
	input_stat.initial_line = NULL;
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
	input_stat.dot_pos = -1;
	input_stat.line_edited = 0;

	if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == VSEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VSEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
	{
		input_stat.search_mode = 1;
	}

	if(input_stat.search_mode || sub_mode == FILTER_SUBMODE)
	{
		save_view_port();
	}

	wcsncpy(input_stat.prompt, prompt, ARRAY_LEN(input_stat.prompt));
	input_stat.prompt_wid = wcslen(input_stat.prompt);
	input_stat.curs_pos = input_stat.prompt_wid;

	if(input_stat.len != 0)
	{
		input_stat.line = malloc(sizeof(wchar_t)*(input_stat.len + 1));
		if(input_stat.line == NULL)
		{
			input_stat.index = 0;
			input_stat.len = 0;
			input_stat.initial_line = NULL;
		}
		else
		{
			wcscpy(input_stat.line, cmd);
			input_stat.curs_pos += wcswidth(input_stat.line, (size_t)-1);
			input_stat.initial_line = my_wcsdup(input_stat.line);
		}
	}

	curs_set(TRUE);

	update_cmdline_size();
	update_cmdline_text();

	curr_stats.save_msg = 1;

	if(prev_mode == NORMAL_MODE)
		init_commands();
}

/* Stores view port parameters (top line, current position). */
static void
save_view_port(void)
{
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

/* Sets view port parameters to appropriate for current submode state. */
static void
set_view_port(void)
{
	if(prev_mode == MENU_MODE)
	{
		load_menu_pos();
		return;
	}

	if(sub_mode != FILTER_SUBMODE || !is_line_edited())
	{
		curr_view->top_line = input_stat.old_top;
		curr_view->list_pos = input_stat.old_pos;
	}

	if(prev_mode == VISUAL_MODE)
	{
		update_visual_mode();
	}
}

/* Checks whether line was edited since entering command-line mode. */
static int
is_line_edited(void)
{
	if(input_stat.line_edited)
	{
		return 1;
	}

	if(input_stat.line == NULL && input_stat.initial_line == NULL)
	{
		/* Do nothing, nothing was changed. */
	}
	else if(input_stat.line == NULL || input_stat.initial_line == NULL)
	{
		input_stat.line_edited = 1;
	}
	else
	{
		input_stat.line_edited = wcscmp(input_stat.line, input_stat.initial_line);
	}
	return input_stat.line_edited;
}

static void
leave_cmdline_mode(void)
{
	int attr;

	if(getmaxy(status_bar) > 1)
	{
		curr_stats.need_update = UT_FULL;
		wresize(status_bar, 1, getmaxx(stdscr) - FIELDS_WIDTH);
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		if(prev_mode == MENU_MODE)
		{
			wresize(menu_win, getmaxy(stdscr) - 1, getmaxx(stdscr));
			update_menu();
		}
	}
	else
	{
		wresize(status_bar, 1, getmaxx(stdscr) - FIELDS_WIDTH);
	}

	curs_set(FALSE);
	curr_stats.save_msg = 0;
	free(input_stat.line);
	free(input_stat.initial_line);
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
		redraw_current_view();
	}
}

/* Initiates leaving of command-line mode and reverting related changes in other
 * parts of the interface. */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	if(input_stat.line != NULL)
	{
		char *mbstr = to_multibyte(input_stat.line);
		save_input_to_history(keys_info, mbstr);
		free(mbstr);

		input_stat.line[0] = L'\0';
	}
	if(sub_mode != FILTER_SUBMODE)
	{
		input_line_changed();
	}

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE)
	{
		if(!input_stat.search_mode)
		{
			leave_visual_mode(curr_stats.save_msg, 1, 1);
			move_to_list_pos(curr_view, check_mark_directory(curr_view, '<'));
		}
	}
	if(sub_mode == CMD_SUBMODE)
	{
		curr_stats.save_msg = exec_commands("", curr_view, GET_COMMAND);
	}
	else if(sub_mode == FILTER_SUBMODE)
	{
		local_filter_cancel(curr_view);
		curr_view->top_line = input_stat.old_top;
		curr_view->list_pos = input_stat.old_pos;
		redraw_current_view();
	}
}

/* Opens the editor with already typed in characters, gets entered line and
 * executes it as if it was typed. */
static void
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	const int type = submode_to_editable_command_type(sub_mode);
	const int prompt_ee = sub_mode == PROMPT_SUBMODE && sub_mode_allows_ee;
	if(type != -1 || prompt_ee)
	{
		char *const mbstr = (input_stat.line == NULL) ?
			strdup("") : to_multibyte(input_stat.line);
		leave_cmdline_mode();

		if(sub_mode == FILTER_SUBMODE)
		{
			local_filter_cancel(curr_view);
		}

		if(prompt_ee)
		{
			extedit_prompt(mbstr, input_stat.index + 1);
		}
		else
		{
			get_and_execute_command(mbstr, input_stat.index + 1, type);
		}

		free(mbstr);
	}
}

/* Converts command-line sub-mode to type of command for the commands.c unit,
 * which supports editing.  Returns -1 when there is no appropriate command
 * type. */
static int
submode_to_editable_command_type(int sub_mode)
{
	switch(sub_mode)
	{
		case CMD_SUBMODE:
			return GET_COMMAND;
		case SEARCH_FORWARD_SUBMODE:
			return GET_FSEARCH_PATTERN;
		case SEARCH_BACKWARD_SUBMODE:
			return GET_BSEARCH_PATTERN;
		case VSEARCH_FORWARD_SUBMODE:
			return GET_VFSEARCH_PATTERN;
		case VSEARCH_BACKWARD_SUBMODE:
			return GET_VBSEARCH_PATTERN;
		case FILTER_SUBMODE:
			return GET_FILTER_PATTERN;

		default:
			return -1;
	}
}

/* Queries prompt input using external editor. */
static void
extedit_prompt(const char input[], int cursor_col)
{
	char *const ext_cmd = get_ext_command(input, cursor_col, GET_PROMPT_INPUT);

	if(ext_cmd != NULL)
	{
		finish_prompt_submode(ext_cmd);
	}
	else
	{
		save_prompt_history(input);

		status_bar_error("Error querying data from external source.");
		curr_stats.save_msg = 1;
	}

	free(ext_cmd);
}

/* Handles backspace. */
static void
cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(should_quit_on_backspace())
	{
		cmd_ctrl_c(key_info, keys_info);
		return;
	}

	if(input_stat.index == 0)
	{
		return;
	}

	input_stat.index--;
	input_stat.len--;

	input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index]);

	if(input_stat.index == input_stat.len)
	{
		input_stat.line[input_stat.index] = L'\0';
	}
	else
	{
		wcsdel(input_stat.line, input_stat.index + 1, 1);
	}

	update_cmdline_text();
}

/* Checks whether backspace key pressed in current state should quit
 * command-line mode.  Returns non-zero if so, otherwise zero is returned. */
static int
should_quit_on_backspace(void)
{
	return input_stat.index == 0
	    && input_stat.len == 0
	    && sub_mode != PROMPT_SUBMODE
	    && (sub_mode != FILTER_SUBMODE || no_initial_line());
}

/* Checks whether initial line was empty.  Returns non-zero if so, otherwise
 * non-zero is returned. */
static int
no_initial_line(void)
{
	return input_stat.initial_line == NULL
	    || input_stat.initial_line[0] == L'\0';
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
	checked_wmove(stat_win, 0, 0);

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

	update_cursor();
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
	/* TODO: refactor this cmd_ctrl_m() function. */
	char* p;

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

	save_input_to_history(keys_info, p);

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
	{
		char* s = (p != NULL) ? p : "";
		while(*s == ' ' || *s == ':')
			s++;
		if(sub_mode == CMD_SUBMODE)
			curr_stats.save_msg = exec_commands(s, curr_view, GET_COMMAND);
		else
			curr_stats.save_msg = exec_commands(s, curr_view, GET_MENU_COMMAND);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		finish_prompt_submode(p);
	}
	else if(sub_mode == FILTER_SUBMODE)
	{
		if(cfg.inc_search)
		{
			local_filter_accept(curr_view);
		}
		else
		{
			local_filter_apply(curr_view, p);
			load_saving_pos(curr_view, 1);
		}
	}
	else if(!cfg.inc_search || prev_mode == VIEW_MODE)
	{
		switch(sub_mode)
		{
			case SEARCH_FORWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_FSEARCH_PATTERN);
				break;
			case SEARCH_BACKWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_BSEARCH_PATTERN);
				break;
			case VSEARCH_FORWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_VFSEARCH_PATTERN);
				break;
			case VSEARCH_BACKWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_VBSEARCH_PATTERN);
				break;
			case VIEW_SEARCH_FORWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_VWFSEARCH_PATTERN);
				break;
			case VIEW_SEARCH_BACKWARD_SUBMODE:
				curr_stats.save_msg = exec_command(p, curr_view, GET_VWBSEARCH_PATTERN);
				break;
			case MENU_SEARCH_FORWARD_SUBMODE:
			case MENU_SEARCH_BACKWARD_SUBMODE:
				curr_stats.need_update = UT_FULL;
				search_menu_list(p, sub_mode_ptr);
				break;

			default:
				assert(0 && "Unknown command line submode.");
				break;
		}
	}
	else if(cfg.inc_search && input_stat.search_mode)
	{
		/* In case of successful search and 'hlsearch' option set, a message like
		 * "n files selected" is printed automatically. */
		if(curr_view->matches == 0 || !cfg.hl_search)
		{
			print_search_msg(curr_view, is_backward_search(sub_mode));
			curr_stats.save_msg = 1;
		}
	}

	free(p);
}

/* Saves command-line input into appropriate history.  input can be NULL, in
 * which case it's ignored. */
static void
save_input_to_history(const keys_info_t *keys_info, const char input[])
{
	if(input == NULL)
	{
		return;
	}
	else if(input_stat.search_mode)
	{
		save_search_history(input);
	}
	else if(sub_mode == CMD_SUBMODE)
	{
		if(!keys_info->mapped && !keys_info->recursive)
		{
			save_command_history(input);
		}
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		save_prompt_history(input);
	}
}

/* Performs final actions on successful querying of prompt input. */
static void
finish_prompt_submode(const char input[])
{
	const prompt_cb cb = (prompt_cb)sub_mode_ptr;

	modes_post();
	modes_pre();

	cb(input);
}

/* Checks whether specified mode is one of forward searching modes.  Returns
 * non-zero if it is, otherwise zero is returned. */
static int
is_forward_search(CMD_LINE_SUBMODES sub_mode)
{
	return sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == VSEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == VIEW_SEARCH_FORWARD_SUBMODE;
}

/* Checks whether specified mode is one of backward searching modes.  Returns
 * non-zero if it is, otherwise zero is returned. */
static int
is_backward_search(CMD_LINE_SUBMODES sub_mode)
{
	return sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VSEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE
			|| sub_mode == VIEW_SEARCH_BACKWARD_SUBMODE;
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
search_next(void)
{
	if(sub_mode == CMD_SUBMODE)
	{
		complete_next(&cfg.cmd_hist, cfg.history_len);
	}
	else if(input_stat.search_mode)
	{
		complete_next(&cfg.search_hist, cfg.history_len);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		complete_next(&cfg.prompt_hist, cfg.history_len);
	}
	else if(sub_mode == FILTER_SUBMODE)
	{
		complete_next(&cfg.filter_hist, cfg.history_len);
	}
}

static void
complete_next(const hist_t *hist, size_t len)
{
	if(hist_is_empty(hist))
	{
		return;
	}

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
			wchar_t *const buf = to_wide(hist->items[pos]);
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

	(void)replace_input_line(hist->items[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > len - 1)
	{
		input_stat.cmd_pos = len - 1;
	}
}

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

	update_cmdline_text();
}

/* Handler of Ctrl-W shortcut, which remove all to the left until beginning of
 * the word is found (i.e. until first delimiter character). */
static void
cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info)
{
	int old;

	input_stat.history_search = HIST_NONE;
	stop_completion();

	old = input_stat.index;

	while(input_stat.index > 0 && iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
	}
	if(iswalnum(input_stat.line[input_stat.index - 1]))
	{
		while(input_stat.index > 0 &&
				iswalnum(input_stat.line[input_stat.index - 1]))
		{
			const wchar_t curr_wchar = input_stat.line[input_stat.index - 1];
			input_stat.curs_pos -= vifm_wcwidth(curr_wchar);
			input_stat.index--;
		}
	}
	else
	{
		while(input_stat.index > 0 &&
				!iswalnum(input_stat.line[input_stat.index - 1]) &&
				!iswspace(input_stat.line[input_stat.index - 1]))
		{
			const wchar_t curr_wchar = input_stat.line[input_stat.index - 1];
			input_stat.curs_pos -= vifm_wcwidth(curr_wchar);
			input_stat.index--;
		}
	}

	if(input_stat.index != old)
	{
		wcsdel(input_stat.line, input_stat.index + 1, old - input_stat.index);
		input_stat.len -= old - input_stat.index;
	}

	update_cmdline_text();
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
	update_cursor();
}

/* Moves cursor to the beginning of the current or previous word on Meta+B. */
static void
find_prev_word(void)
{
	while(input_stat.index > 0 && iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
	}
	while(input_stat.index > 0 &&
			!iswspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
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

	update_cmdline_text();
}

static void
cmd_meta_f(key_info_t key_info, keys_info_t *keys_info)
{
	find_next_word();
	update_cursor();
}

/* Inserts last part of previous command to current cursor position.  Each next
 * call will insert last part of older command. */
static void
cmd_meta_dot(key_info_t key_info, keys_info_t *keys_info)
{
	wchar_t *wide;

	if(sub_mode != CMD_SUBMODE)
	{
		return;
	}

	stop_history_completion();

	if(hist_is_empty(&cfg.cmd_hist))
	{
		return;
	}

	if(input_stat.dot_pos < 0)
	{
		input_stat.dot_pos = input_stat.cmd_pos + 1;
	}
	else
	{
		remove_previous_dot_completion();
	}

	wide = next_dot_completion();

	if(insert_dot_completion(wide) == 0)
	{
		update_cmdline_text();
	}
	else
	{
		stop_dot_completion();
	}

	free(wide);
}

/* Removes previous dot completion from any part of command line. */
static void
remove_previous_dot_completion(void)
{
	size_t start = input_stat.dot_index;
	size_t end = start + input_stat.dot_len;
	memmove(input_stat.line + start, input_stat.line + end,
			sizeof(wchar_t)*(input_stat.len - end + 1));
	input_stat.len -= input_stat.dot_len;
	input_stat.index -= input_stat.dot_len;
	input_stat.curs_pos -= input_stat.dot_len;
}

/* Gets next dot completion from history of commands. Returns new string, caller
 * should free it. */
static wchar_t *
next_dot_completion(void)
{
	size_t len;
	char *last;
	wchar_t *wide;

	if(input_stat.dot_pos <= cfg.cmd_hist.pos)
	{
		last = get_last_argument(cfg.cmd_hist.items[input_stat.dot_pos++], &len);
	}
	else
	{
		last = "";
		len = 0;
	}
	last = strdup(last);
	last[len] = '\0';
	wide = to_wide(last);
	free(last);

	return wide;
}

/* Inserts dot completion into current cursor position in the command line.
 * Returns zero on success. */
static int
insert_dot_completion(const wchar_t completion[])
{
	wchar_t *ptr;
	size_t len = wcslen(completion);

	ptr = realloc(input_stat.line, (input_stat.len + len + 1)*sizeof(wchar_t));
	if(ptr == NULL)
	{
		return 1;
	}

	input_stat.dot_index = input_stat.index;
	input_stat.dot_len = len;

	if(input_stat.line == NULL)
	{
		ptr[0] = L'\0';
	}
	input_stat.line = ptr;
	wcsins(input_stat.line, completion, input_stat.index + 1);
	input_stat.index += len;
	input_stat.curs_pos += len;
	input_stat.len += len;
	return 0;
}

/* Moves cursor to the end of the current or next word on Meta+F. */
static void
find_next_word(void)
{
	while(input_stat.index < input_stat.len
			&& iswspace(input_stat.line[input_stat.index]))
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
	}
	while(input_stat.index < input_stat.len
			&& !iswspace(input_stat.line[input_stat.index]))
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
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
		input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index]);
		update_cursor();
	}
}

static void
cmd_right(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index < input_stat.len)
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
		update_cursor();
	}
}

static void
cmd_home(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = wcslen(input_stat.prompt);
	update_cursor();
}

/* Moves cursor to the end of command-line on Ctrl+E and End. */
static void
cmd_end(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid +
			wcswidth(input_stat.line, (size_t)-1);
	update_cursor();
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

	update_cmdline_text();
}

static void
update_cursor(void)
{
	checked_wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
search_prev(void)
{
	if(sub_mode == CMD_SUBMODE)
	{
		complete_prev(&cfg.cmd_hist, cfg.history_len);
	}
	else if(input_stat.search_mode)
	{
		complete_prev(&cfg.search_hist, cfg.history_len);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		complete_prev(&cfg.prompt_hist, cfg.history_len);
	}
	else if(sub_mode == FILTER_SUBMODE)
	{
		complete_prev(&cfg.filter_hist, cfg.history_len);
	}
}

static void
complete_prev(const hist_t *hist, size_t len)
{
	if(hist_is_empty(hist))
	{
		return;
	}

	if(input_stat.history_search != HIST_SEARCH)
	{
		if(input_stat.cmd_pos == hist->pos)
			return;
		input_stat.cmd_pos++;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(++pos <= hist->pos)
		{
			wchar_t *buf;

			buf = to_wide(hist->items[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos > hist->pos)
			return;
		input_stat.cmd_pos = pos;
	}

	(void)replace_input_line(hist->items[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > len - 1)
	{
		input_stat.cmd_pos = len - 1;
	}
}

/* Returns 0 on success. */
static int
replace_input_line(const char new[])
{
	wchar_t *const wide_new = to_wide(new);
	if(wide_new == NULL)
	{
		return 1;
	}

	free(input_stat.line);
	input_stat.line = wide_new;
	input_stat.len = wcslen(wide_new);
	return 0;
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

/* Swaps the order of two characters. */
static void
cmd_ctrl_t(key_info_t key_info, keys_info_t *keys_info)
{
	wchar_t char_before_last;
	size_t index;

	stop_completion();

	index = input_stat.index;
	if(index == 0 || input_stat.len == 1)
		return;

	if(index == input_stat.len)
	{
		index--;
	}
	else
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
	}

	char_before_last = input_stat.line[index - 1];
	input_stat.line[index - 1] = input_stat.line[index];
	input_stat.line[index] = char_before_last;

	update_cmdline_text();
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

	const size_t new_len = (p - line_mb) + mbstowcs(NULL, completed, 0)
			+ (stat->len - stat->index) + 1;

	line_ending = my_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
		return -1;

	if((t = realloc(stat->line, new_len * sizeof(wchar_t))) == NULL)
	{
		free(line_ending);
		return -1;
	}
	stat->line = t;

	my_swprintf(stat->line + (p - line_mb), new_len,
			L"%" WPRINTF_MBSTR L"%" WPRINTF_WSTR, completed, line_ending);
	free(line_ending);

	update_line_stat(stat, new_len);
	update_cmdline_size();
	update_cmdline_text();
	return 0;
}

/* Returns non-zero on error */
TSTATIC int
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
	stop_dot_completion();
	stop_history_completion();
}

static void
stop_dot_completion(void)
{
	input_stat.dot_pos = -1;
}

static void
stop_history_completion(void)
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
