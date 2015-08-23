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
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcslen() wcsncpy() */
#include <wctype.h>

#include "../cfg/config.h"
#include "../cfg/hist.h"
#include "../compat/curses.h"
#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"
#include "../engine/abbrevs.h"
#include "../engine/cmds.h"
#include "../engine/completion.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/statusbar.h"
#include "../ui/statusline.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utils.h"
#include "../color_manager.h"
#include "../color_scheme.h"
#include "../colors.h"
#include "../commands.h"
#include "../commands_completion.h"
#include "../filelist.h"
#include "../fileview.h"
#include "../filtering.h"
#include "../marks.h"
#include "../search.h"
#include "../status.h"
#include "dialogs/attr_dialog.h"
#include "dialogs/sort_dialog.h"
#include "menu.h"
#include "modes.h"
#include "normal.h"
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
	int entered_by_mapping;   /* The mode was entered by a mapping. */
	int expanding_abbrev;     /* Abbreviation expansion is in progress. */
}
line_stats_t;

#endif

static int prev_mode;
static CmdLineSubmode sub_mode;
static line_stats_t input_stat;
/* Width of the status bar. */
static int line_width = 1;
static void *sub_mode_ptr;
static int sub_mode_allows_ee;

static int def_handler(wchar_t key);
static void update_cmdline_size(void);
static void update_cmdline_text(line_stats_t *stat);
static void input_line_changed(void);
static void set_local_filter(const char value[]);
static wchar_t * wcsins(wchar_t src[], const wchar_t ins[], int pos);
static void prepare_cmdline_mode(const wchar_t prompt[], const wchar_t cmd[],
		complete_cmd_func complete);
static void save_view_port(void);
static void set_view_port(void);
static int is_line_edited(void);
static void leave_cmdline_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static CmdInputType cls_to_editable_cit(CmdLineSubmode sub_mode);
static void extedit_prompt(const char input[], int cursor_col);
static void cmd_ctrl_rb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info);
static int should_quit_on_backspace(void);
static int no_initial_line(void);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void do_completion(void);
static void draw_wild_menu(int op);
static void cmd_ctrl_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static int is_input_line_empty(void);
static void expand_abbrev(void);
TSTATIC const wchar_t * extract_abbrev(line_stats_t *stat, int *pos,
		int *no_remap);
static void exec_abbrev(const wchar_t abbrev_rhs[], int no_remap, int pos);
static void save_input_to_history(const keys_info_t *keys_info,
		const char input[]);
static void finish_prompt_submode(const char input[]);
static CmdInputType search_cls_to_cit(CmdLineSubmode sub_mode);
static int is_forward_search(CmdLineSubmode sub_mode);
static int is_backward_search(CmdLineSubmode sub_mode);
static int replace_wstring(wchar_t **str, const wchar_t with[]);
static void cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_down(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
static void search_next(void);
static void complete_next(const hist_t *hist, size_t len);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xslash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xa(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xc(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxc(key_info_t key_info, keys_info_t *keys_info);
static void paste_short_path(FileView *view);
static void cmd_ctrl_xd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xe(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxe(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xm(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xr(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxr(key_info_t key_info, keys_info_t *keys_info);
static void paste_short_path_root(FileView *view);
static void cmd_ctrl_xt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xequals(key_info_t key_info, keys_info_t *keys_info);
static void paste_name_part(const char name[], int root);
static void paste_str(const char str[], int allow_escaping);
static char * escape_cmd_for_pasting(const char str[]);
static void cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_b(key_info_t key_info, keys_info_t *keys_info);
static void find_prev_word(void);
static void cmd_meta_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_dot(key_info_t key_info, keys_info_t *keys_info);
static void remove_previous_dot_completion(void);
static wchar_t * next_dot_completion(void);
static int insert_dot_completion(const wchar_t completion[]);
static int insert_str(const wchar_t str[]);
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
static int get_required_height(void);
static void cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_t(key_info_t key_info, keys_info_t *keys_info);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_up(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
TSTATIC int line_completion(line_stats_t *stat);
static char * escaped_arg_hook(const char match[]);
static char * squoted_arg_hook(const char match[]);
static char * dquoted_arg_hook(const char match[]);
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
	{L"\x1d",         {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_rb}}},
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
	{L"\x18"L"/",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xslash}}},
	{L"\x18"L"a",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xa}}},
	{L"\x18"L"c",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xc}}},
	{L"\x18\x18"L"c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xxc}}},
	{L"\x18"L"d",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xd}}},
	{L"\x18\x18"L"d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xxd}}},
	{L"\x18"L"e",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xe}}},
	{L"\x18\x18"L"e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xxe}}},
	{L"\x18"L"m",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xm}}},
	{L"\x18"L"r",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xr}}},
	{L"\x18\x18"L"r", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xxr}}},
	{L"\x18"L"t",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xt}}},
	{L"\x18\x18"L"t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xxt}}},
	{L"\x18"L"=",     {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_xequals}}},
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
init_cmdline_mode(void)
{
	int ret_code;

	set_def_handler(CMDLINE_MODE, &def_handler);

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), CMDLINE_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

/* Handles all keys uncaught by shortcuts.  Returns zero on success and non-zero
 * on error. */
static int
def_handler(wchar_t key)
{
	void *p;
	wchar_t buf[2] = {key, L'\0'};

	input_stat.history_search = HIST_NONE;

	if(input_stat.complete_continue
			&& input_stat.line[input_stat.index - 1] == L'/' && key == L'/')
	{
		stop_completion();
		return 0;
	}

	stop_completion();

	/* iswprint(KEY_RESIZE) can return true, but we really don't want to mess up
	 * input because terminal dimensions changed. */
	if(key != L'\r' && (!iswprint(key) || key == KEY_RESIZE))
	{
		return 0;
	}

	if(!cfg_is_word_wchar(key))
	{
		expand_abbrev();
	}

	p = reallocarray(input_stat.line, input_stat.len + 2, sizeof(wchar_t));
	if(p == NULL)
	{
		leave_cmdline_mode();
		return 0;
	}

	input_stat.line = p;

	input_stat.index++;
	wcsins(input_stat.line, buf, input_stat.index);
	input_stat.len++;

	input_stat.curs_pos += vifm_wcwidth(key);

	update_cmdline_size();
	update_cmdline_text(&input_stat);

	return 0;
}

static void
update_cmdline_size(void)
{
	const int required_height = get_required_height();
	if(required_height >= getmaxy(status_bar))
	{
		const int screen_height = getmaxy(stdscr);

		mvwin(status_bar, screen_height - required_height, 0);
		wresize(status_bar, required_height, line_width);

		if(prev_mode != MENU_MODE)
		{
			if(ui_stat_reposition(required_height))
			{
				ui_stat_refresh();
			}
		}
		else
		{
			wresize(menu_win, screen_height - required_height, getmaxx(stdscr));
			update_menu();
			wrefresh(menu_win);
		}
	}
}

/* Update test displayed on the command line and cursor. */
static void
update_cmdline_text(line_stats_t *stat)
{
	int attr;

	input_line_changed();

	werase(status_bar);

	attr = cfg.cs.color[CMD_LINE_COLOR].attr;
	wattron(status_bar, COLOR_PAIR(cfg.cs.pair[CMD_LINE_COLOR]) | attr);

	compat_mvwaddwstr(status_bar, 0, 0, stat->prompt);
	compat_mvwaddwstr(status_bar, stat->prompt_wid/line_width,
			stat->prompt_wid%line_width, stat->line);
	update_cursor();
	wrefresh(status_bar);
}

/* Callback-like function, which is called every time input line is changed. */
static void
input_line_changed(void)
{
	static wchar_t *previous;

	if(!cfg.inc_search || (!input_stat.search_mode && sub_mode != CLS_FILTER))
	{
		return;
	}

	/* Hide cursor during view update, otherwise user might notice it blinking in
	 * wrong place. */
	curs_set(FALSE);

	set_view_port();

	if(is_input_line_empty())
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
				(void)search_menu_list("", sub_mode_ptr);
			}
		}
		free(previous);
		previous = NULL;

		if(prev_mode != MENU_MODE)
		{
			ui_view_reset_search_highlight(curr_view);
		}

		if(sub_mode == CLS_FILTER)
		{
			set_local_filter("");
		}
	}
	else if(previous == NULL || wcscmp(previous, input_stat.line) != 0)
	{
		char *mbinput;
		int backward;

		(void)replace_wstring(&previous, input_stat.line);

		mbinput = to_multibyte(input_stat.line);

		backward = 0;
		switch(sub_mode)
		{
			case CLS_BSEARCH: backward = 1; /* Fall through. */
			case CLS_FSEARCH:
				(void)find_npattern(curr_view, mbinput, backward, 0);
				break;
			case CLS_VFSEARCH:
			case CLS_VBSEARCH:
				{
					const CmdInputType cit = search_cls_to_cit(sub_mode);
					(void)exec_command(mbinput, curr_view, cit);
					break;
				}
			case CLS_MENU_FSEARCH:
			case CLS_MENU_BSEARCH:
				(void)search_menu_list(mbinput, sub_mode_ptr);
				break;
			case CLS_FILTER:
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

	curs_set(TRUE);
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
enter_cmdline_mode(CmdLineSubmode cl_sub_mode, const wchar_t *cmd, void *ptr)
{
	const wchar_t *prompt;
	complete_cmd_func complete_func;

	sub_mode_ptr = ptr;
	sub_mode = cl_sub_mode;
	sub_mode_allows_ee = 0;

	if(sub_mode == CLS_COMMAND || sub_mode == CLS_MENU_COMMAND)
	{
		prompt = L":";
	}
	else if(sub_mode == CLS_FILTER)
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

	complete_func = (sub_mode == CLS_FILTER) ? NULL : complete_cmd;
	prepare_cmdline_mode(prompt, cmd, complete_func);
}

void
enter_prompt_mode(const wchar_t prompt[], const char cmd[], prompt_cb cb,
		complete_cmd_func complete, int allow_ee)
{
	wchar_t *buf;

	sub_mode_ptr = cb;
	sub_mode = CLS_PROMPT;
	sub_mode_allows_ee = allow_ee;

	buf = to_wide(cmd);
	if(buf == NULL)
	{
		/* This is either memory allocation error or broken multi-byte sequence. */
		show_error_msgf("Error", "Unicode conversion failed for: %s", cmd);
		return;
	}

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
	update_cmdline_size();
	update_cmdline_text(&input_stat);
	curs_set(TRUE);

	if(input_stat.complete_continue && cfg.wild_menu)
	{
		draw_wild_menu(-1);
	}
}

/* Performs all necessary preparations for command-line mode to start
 * operating. */
static void
prepare_cmdline_mode(const wchar_t prompt[], const wchar_t cmd[],
		complete_cmd_func complete)
{
	line_width = getmaxx(stdscr);
	prev_mode = vle_mode_get();
	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);

	input_stat.line = vifm_wcsdup(cmd);
	input_stat.initial_line = vifm_wcsdup(input_stat.line);
	input_stat.index = wcslen(cmd);
	input_stat.curs_pos = vifm_wcswidth(input_stat.line, (size_t)-1);
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
	input_stat.entered_by_mapping = is_inside_mapping();

	if((is_forward_search(sub_mode) || is_backward_search(sub_mode)) &&
			sub_mode != CLS_VWFSEARCH && sub_mode != CLS_VWBSEARCH)
	{
		input_stat.search_mode = 1;
	}

	if(input_stat.search_mode || sub_mode == CLS_FILTER)
	{
		save_view_port();
	}

	wcsncpy(input_stat.prompt, prompt, ARRAY_LEN(input_stat.prompt));
	input_stat.prompt_wid = vifm_wcswidth(input_stat.prompt, (size_t)-1);
	input_stat.curs_pos += input_stat.prompt_wid;

	update_cmdline_size();
	update_cmdline_text(&input_stat);
	curs_set(TRUE);

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

	if(sub_mode != CLS_FILTER || !is_line_edited())
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

	input_stat.line_edited = wcscmp(input_stat.line, input_stat.initial_line);
	return input_stat.line_edited;
}

/* Leaves command-line mode. */
static void
leave_cmdline_mode(void)
{
	const int multiline_status_bar = (getmaxy(status_bar) > 1);
	int attr;

	wresize(status_bar, 1, getmaxx(stdscr) - FIELDS_WIDTH());
	if(multiline_status_bar)
	{
		curr_stats.need_update = UT_REDRAW;
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		if(prev_mode == MENU_MODE)
		{
			wresize(menu_win, getmaxy(stdscr) - 1, getmaxx(stdscr));
			update_menu();
		}
	}

	free(input_stat.line);
	free(input_stat.initial_line);
	free(input_stat.line_buf);
	input_stat.line = NULL;
	input_stat.initial_line = NULL;
	input_stat.line_buf = NULL;

	curs_set(FALSE);
	curr_stats.save_msg = 0;
	clean_status_bar();

	if(vle_mode_is(CMDLINE_MODE))
	{
		vle_mode_set(prev_mode, VMT_PRIMARY);
	}

	if(!vle_mode_is(MENU_MODE))
	{
		ui_ruler_update(curr_view);
	}

	attr = cfg.cs.color[CMD_LINE_COLOR].attr;
	wattroff(status_bar, COLOR_PAIR(cfg.cs.pair[CMD_LINE_COLOR]) | attr);

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
	char *mbstr;

	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	mbstr = to_multibyte(input_stat.line);
	save_input_to_history(keys_info, mbstr);
	free(mbstr);

	if(sub_mode != CLS_FILTER)
	{
		input_stat.line[0] = L'\0';
		input_line_changed();
	}

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE)
	{
		if(!input_stat.search_mode)
		{
			leave_visual_mode(curr_stats.save_msg, 1, 1);
			flist_set_pos(curr_view, check_mark_directory(curr_view, '<'));
		}
	}
	if(sub_mode == CLS_COMMAND)
	{
		curr_stats.save_msg = exec_commands("", curr_view, CIT_COMMAND);
	}
	else if(sub_mode == CLS_FILTER)
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
	const CmdInputType type = cls_to_editable_cit(sub_mode);
	const int prompt_ee = sub_mode == CLS_PROMPT && sub_mode_allows_ee;
	if(type != (CmdInputType)-1 || prompt_ee)
	{
		char *const mbstr = to_multibyte(input_stat.line);
		leave_cmdline_mode();

		if(sub_mode == CLS_FILTER)
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

/* Converts command-line sub-mode to type of command input which supports
 * editing.  Returns (CmdInputType)-1 when there is no appropriate command
 * type. */
static CmdInputType
cls_to_editable_cit(CmdLineSubmode sub_mode)
{
	switch(sub_mode)
	{
		case CLS_COMMAND:  return CIT_COMMAND;
		case CLS_FSEARCH:  return CIT_FSEARCH_PATTERN;
		case CLS_BSEARCH:  return CIT_BSEARCH_PATTERN;
		case CLS_VFSEARCH: return CIT_VFSEARCH_PATTERN;
		case CLS_VBSEARCH: return CIT_VBSEARCH_PATTERN;
		case CLS_FILTER:   return CIT_FILTER_PATTERN;

		default:
			return (CmdInputType)-1;
	}
}

/* Queries prompt input using external editor. */
static void
extedit_prompt(const char input[], int cursor_col)
{
	char *const ext_cmd = get_ext_command(input, cursor_col, CIT_PROMPT_INPUT);

	if(ext_cmd != NULL)
	{
		finish_prompt_submode(ext_cmd);
	}
	else
	{
		cfg_save_prompt_history(input);

		status_bar_error("Error querying data from external source.");
		curr_stats.save_msg = 1;
	}

	free(ext_cmd);
}

/* Expands abbreviation to the left of current cursor position, if any
 * present. */
static void
cmd_ctrl_rb(key_info_t key_info, keys_info_t *keys_info)
{
	expand_abbrev();
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

	update_cmdline_text(&input_stat);
}

/* Checks whether backspace key pressed in current state should quit
 * command-line mode.  Returns non-zero if so, otherwise zero is returned. */
static int
should_quit_on_backspace(void)
{
	return input_stat.index == 0
	    && input_stat.len == 0
	    && sub_mode != CLS_PROMPT
	    && (sub_mode != CLS_FILTER || no_initial_line());
}

/* Checks whether initial line was empty.  Returns non-zero if so, otherwise
 * non-zero is returned. */
static int
no_initial_line(void)
{
	return input_stat.initial_line[0] == L'\0';
}

static void
cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	input_stat.reverse_completion = 0;

	if(input_stat.complete_continue && vle_compl_get_count() == 2)
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

	if(input_stat.complete_continue && vle_compl_get_count() == 2)
		input_stat.complete_continue = 0;

	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);
}

static void
do_completion(void)
{
	if(input_stat.complete == NULL)
	{
		return;
	}

	line_completion(&input_stat);

	update_cmdline_size();
	update_cmdline_text(&input_stat);
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

	const char ** list = vle_compl_get_list();
	int pos = vle_compl_get_pos();
	int count = vle_compl_get_count() - 1;
	int i;
	int len = getmaxx(stdscr);

	/* This check should go first to ensure that resetting of wild menu is
	 * processed and no returns will break the expected behaviour. */
	if(op > 0)
	{
		last_pos = 0;
		return;
	}

	if(sub_mode == CLS_MENU_COMMAND || input_stat.complete == NULL || count < 2)
	{
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
			mix_colors(&col, &cfg.cs.color[WILD_MENU_COLOR]);

			wbkgdset(stat_win, COLOR_PAIR(colmgr_get_pair(col.fg, col.bg)) | col.attr);
		}
		wprint(stat_win, list[i]);
		if(i == pos)
		{
			wbkgdset(stat_win, COLOR_PAIR(cfg.cs.pair[STATUS_LINE_COLOR]) |
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

	update_cmdline_text(&input_stat);
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	/* TODO: refactor this cmd_ctrl_m() function. */
	char *input;

	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	if(is_input_line_empty() && sub_mode == CLS_MENU_COMMAND)
	{
		leave_cmdline_mode();
		return;
	}

	expand_abbrev();

	input = to_multibyte(input_stat.line);

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE && sub_mode != CLS_VFSEARCH &&
			sub_mode != CLS_VBSEARCH)
		leave_visual_mode(curr_stats.save_msg, 1, 0);

	save_input_to_history(keys_info, input);

	if(sub_mode == CLS_COMMAND || sub_mode == CLS_MENU_COMMAND)
	{
		const CmdInputType cmd_type = (sub_mode == CLS_COMMAND)
		                            ? CIT_COMMAND
		                            : CIT_MENU_COMMAND;

		const char *real_start = input;
		while(*real_start == ' ' || *real_start == ':')
		{
			real_start++;
		}

		curr_stats.save_msg = exec_commands(real_start, curr_view, cmd_type);
	}
	else if(sub_mode == CLS_PROMPT)
	{
		finish_prompt_submode(input);
	}
	else if(sub_mode == CLS_FILTER)
	{
		if(cfg.inc_search)
		{
			local_filter_accept(curr_view);
		}
		else
		{
			local_filter_apply(curr_view, input);
			ui_view_schedule_reload(curr_view);
		}
	}
	else if(!cfg.inc_search || prev_mode == VIEW_MODE || input[0] == '\0')
	{
		const char *const pattern = (input[0] == '\0')
		                          ? cfg_get_last_search_pattern()
		                          : input;

		switch(sub_mode)
		{
			case CLS_FSEARCH:
			case CLS_BSEARCH:
			case CLS_VFSEARCH:
			case CLS_VBSEARCH:
			case CLS_VWFSEARCH:
			case CLS_VWBSEARCH:
				{
					const CmdInputType cit = search_cls_to_cit(sub_mode);
					curr_stats.save_msg = exec_command(pattern, curr_view, cit);
					break;
				}
			case CLS_MENU_FSEARCH:
			case CLS_MENU_BSEARCH:
				curr_stats.need_update = UT_FULL;
				curr_stats.save_msg = search_menu_list(pattern, sub_mode_ptr);
				break;

			default:
				assert(0 && "Unknown command line submode.");
				break;
		}
	}
	else if(cfg.inc_search && input_stat.search_mode)
	{
		if(prev_mode == MENU_MODE)
		{
			menu_print_search_msg(sub_mode_ptr);
			curr_stats.save_msg = 1;
		}
		else
		{
			/* In case of successful search and 'hlsearch' option set, a message like
			* "n files selected" is printed automatically. */
			if(curr_view->matches == 0 || !cfg.hl_search)
			{
				print_search_msg(curr_view, is_backward_search(sub_mode));
				curr_stats.save_msg = 1;
			}
		}
	}

	free(input);
}

/* Checks whether input line is empty.  Returns non-zero if so, otherwise
 * non-zero is returned. */
static int
is_input_line_empty(void)
{
	return input_stat.line[0] == L'\0';
}

/* Expands abbreviation to the left of current cursor position, if any
 * present, otherwise does nothing. */
static void
expand_abbrev(void)
{
	int pos;
	const wchar_t *abbrev_rhs;
	int no_remap;

	/* No recursion on expanding abbreviations. */
	if(input_stat.expanding_abbrev)
	{
		return;
	}

	abbrev_rhs = extract_abbrev(&input_stat, &pos, &no_remap);
	if(abbrev_rhs != NULL)
	{
		input_stat.expanding_abbrev = 1;
		exec_abbrev(abbrev_rhs, no_remap, pos);
		input_stat.expanding_abbrev = 0;

		update_cmdline_size();
		update_cmdline_text(&input_stat);
	}
}

/* Lookups for an abbreviation to the left of cursor position.  Returns RHS for
 * the abbreviation and fills pointer arguments if found, otherwise NULL is
 * returned. */
TSTATIC const wchar_t *
extract_abbrev(line_stats_t *stat, int *pos, int *no_remap)
{
	const int i = stat->index;
	wchar_t *const line = stat->line;

	int l;
	wchar_t c;
	const wchar_t *abbrev_rhs;

	l = i;
	while(l > 0 && cfg_is_word_wchar(line[l - 1]))
	{
		--l;
	}

	if(l == i)
	{
		return NULL;
	}

	c = line[i];
	line[i] = L'\0';
	abbrev_rhs = vle_abbr_expand(&line[l], no_remap);
	line[i] = c;

	*pos = l;

	return abbrev_rhs;
}

/* Runs RHS of an abbreviation after removing LHS that starts at the pos. */
static void
exec_abbrev(const wchar_t abbrev_rhs[], int no_remap, int pos)
{
	const size_t lhs_len = input_stat.index - pos;

	input_stat.len -= lhs_len;
	input_stat.index -= lhs_len;
	input_stat.curs_pos -= vifm_wcswidth(&input_stat.line[pos], lhs_len);
	(void)wcsdel(input_stat.line, pos + 1, lhs_len);

	if(no_remap)
	{
		(void)execute_keys_timed_out_no_remap(abbrev_rhs);
	}
	else
	{
		(void)execute_keys_timed_out(abbrev_rhs);
	}
}

/* Saves command-line input into appropriate history.  input can be NULL, in
 * which case it's ignored. */
static void
save_input_to_history(const keys_info_t *keys_info, const char input[])
{
	if(input_stat.search_mode)
	{
		cfg_save_search_history(input);
	}
	else if(sub_mode == CLS_COMMAND)
	{
		const int mapped_input = input_stat.entered_by_mapping && keys_info->mapped;
		const int ignore_input = mapped_input || keys_info->recursive;
		if(!ignore_input)
		{
			cfg_save_command_history(input);
		}
	}
	else if(sub_mode == CLS_PROMPT)
	{
		cfg_save_prompt_history(input);
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

/* Converts search command-line sub-mode to type of command input.  Returns
 * (CmdInputType)-1 if there is no appropriate command type. */
static CmdInputType
search_cls_to_cit(CmdLineSubmode sub_mode)
{
	switch(sub_mode)
	{
		case CLS_FSEARCH:   return CIT_FSEARCH_PATTERN;
		case CLS_BSEARCH:   return CIT_BSEARCH_PATTERN;
		case CLS_VFSEARCH:  return CIT_VFSEARCH_PATTERN;
		case CLS_VBSEARCH:  return CIT_VBSEARCH_PATTERN;
		case CLS_VWFSEARCH: return CIT_VWFSEARCH_PATTERN;
		case CLS_VWBSEARCH: return CIT_VWBSEARCH_PATTERN;

		default:
			assert(0 && "Unknown search command-line submode.");
			return (CmdInputType)-1;
	}
}

/* Checks whether specified mode is one of forward searching modes.  Returns
 * non-zero if it is, otherwise zero is returned. */
static int
is_forward_search(CmdLineSubmode sub_mode)
{
	return sub_mode == CLS_FSEARCH
	    || sub_mode == CLS_VFSEARCH
	    || sub_mode == CLS_MENU_FSEARCH
	    || sub_mode == CLS_VWFSEARCH;
}

/* Checks whether specified mode is one of backward searching modes.  Returns
 * non-zero if it is, otherwise zero is returned. */
static int
is_backward_search(CmdLineSubmode sub_mode)
{
	return sub_mode == CLS_BSEARCH
	    || sub_mode == CLS_VBSEARCH
	    || sub_mode == CLS_MENU_BSEARCH
	    || sub_mode == CLS_VWBSEARCH;
}

static void
save_users_input(void)
{
	(void)replace_wstring(&input_stat.line_buf, input_stat.line);
}

static void
restore_user_input(void)
{
	input_stat.cmd_pos = -1;
	(void)replace_wstring(&input_stat.line, input_stat.line_buf);
	input_stat.len = wcslen(input_stat.line);
	update_cmdline();
}

/* Replaces *str with a copy of the with string.  *str can be NULL or equal to
 * the with (then function does nothing).  Returns non-zero if memory allocation
 * failed. */
static int
replace_wstring(wchar_t **str, const wchar_t with[])
{
	if(*str != with)
	{
		wchar_t *const new = vifm_wcsdup(with);
		if(new == NULL)
		{
			return 1;
		}
		free(*str);
		*str = new;
	}
	return 0;
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
	if(sub_mode == CLS_COMMAND)
	{
		complete_next(&cfg.cmd_hist, cfg.history_len);
	}
	else if(input_stat.search_mode)
	{
		complete_next(&cfg.search_hist, cfg.history_len);
	}
	else if(sub_mode == CLS_PROMPT)
	{
		complete_next(&cfg.prompt_hist, cfg.history_len);
	}
	else if(sub_mode == CLS_FILTER)
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
		--input_stat.cmd_pos;
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

	if(input_stat.cmd_pos > (int)len - 1)
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
	compat_mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	compat_mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	update_cmdline_text(&input_stat);
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

	update_cmdline_text(&input_stat);
}

/* Inserts last pattern from search history into current cursor position. */
static void
cmd_ctrl_xslash(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(cfg_get_last_search_pattern(), 0);
}

/* Inserts value of automatic filter of active pane into current cursor
 * position. */
static void
cmd_ctrl_xa(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(curr_view->auto_filter.raw, 0);
}

/* Inserts name of the current file of active pane into current cursor
 * position. */
static void
cmd_ctrl_xc(key_info_t key_info, keys_info_t *keys_info)
{
	paste_short_path(curr_view);
}

/* Inserts name of the current file of inactive pane into current cursor
 * position. */
static void
cmd_ctrl_xxc(key_info_t key_info, keys_info_t *keys_info)
{
	paste_short_path(other_view);
}

/* Pastes short path of the current entry of the view into current cursor
 * position. */
static void
paste_short_path(FileView *view)
{
	if(flist_custom_active(view))
	{
		char short_path[PATH_MAX];
		get_short_path_of(view, get_current_entry(view), 0, sizeof(short_path),
				short_path);
		paste_str(short_path, 1);
		return;
	}

	paste_str(get_current_file_name(view), 1);
}

/* Inserts path to the current directory of active pane into current cursor
 * position. */
static void
cmd_ctrl_xd(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(flist_get_dir(curr_view), 1);
}

/* Inserts path to the current directory of inactive pane into current cursor
 * position. */
static void
cmd_ctrl_xxd(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(flist_get_dir(other_view), 1);
}

/* Inserts extension of the current file of active pane into current cursor
 * position. */
static void
cmd_ctrl_xe(key_info_t key_info, keys_info_t *keys_info)
{
	paste_name_part(get_current_file_name(curr_view), 0);
}

/* Inserts extension of the current file of inactive pane into current cursor
 * position. */
static void
cmd_ctrl_xxe(key_info_t key_info, keys_info_t *keys_info)
{
	paste_name_part(get_current_file_name(other_view), 0);
}

/* Inserts value of manual filter of active pane into current cursor
 * position. */
static void
cmd_ctrl_xm(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(curr_view->manual_filter.raw, 0);
}

/* Inserts name root of current file of active pane into current cursor
 * position. */
static void
cmd_ctrl_xr(key_info_t key_info, keys_info_t *keys_info)
{
	paste_short_path_root(curr_view);
}

/* Inserts name root of current file of inactive pane into current cursor
 * position. */
static void
cmd_ctrl_xxr(key_info_t key_info, keys_info_t *keys_info)
{
	paste_short_path_root(other_view);
}

/* Pastes short root of the current entry of the view into current cursor
 * position. */
static void
paste_short_path_root(FileView *view)
{
	char short_path[PATH_MAX];
	get_short_path_of(view, get_current_entry(view), 0, sizeof(short_path),
			short_path);
	paste_name_part(short_path, 1);
}

/* Inserts last component of path to the current directory of active pane into
 * current cursor position. */
static void
cmd_ctrl_xt(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(get_last_path_component(flist_get_dir(curr_view)), 1);
}

/* Inserts last component of path to the current directory of inactive pane into
 * current cursor position. */
static void
cmd_ctrl_xxt(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(get_last_path_component(flist_get_dir(other_view)), 1);
}

/* Inserts value of local filter of active pane into current cursor position. */
static void
cmd_ctrl_xequals(key_info_t key_info, keys_info_t *keys_info)
{
	paste_str(curr_view->local_filter.filter.raw, 0);
}

/* Inserts root/extension of the name file into current cursor position. */
static void
paste_name_part(const char name[], int root)
{
	int root_len;
	const char *ext;
	char *const name_copy = strdup(name);

	split_ext(name_copy, &root_len, &ext);
	paste_str(root ? name_copy : ext, 1);

	free(name_copy);
}

/* Inserts string into current cursor position and updates command-line on the
 * screen. */
static void
paste_str(const char str[], int allow_escaping)
{
	char *escaped;
	wchar_t *wide;

	if(prev_mode == MENU_MODE)
	{
		return;
	}

	stop_completion();

	escaped = (allow_escaping && sub_mode == CLS_COMMAND)
	        ? escape_cmd_for_pasting(str)
	        : NULL;
	if(escaped != NULL)
	{
		str = escaped;
	}

	wide = to_wide(str);

	if(insert_str(wide) == 0)
	{
		update_cmdline_text(&input_stat);
	}

	free(wide);
	free(escaped);
}

/* Escapes str for inserting into current position of command-line command when
 * needed.  Returns escaped string or NULL when no escaping is needed. */
static char *
escape_cmd_for_pasting(const char str[])
{
	wchar_t *const wide_input = vifm_wcsdup(input_stat.line);
	char *mb_input;
	char *escaped;

	wide_input[input_stat.index] = L'\0';
	mb_input = to_multibyte(wide_input);

	escaped = commands_escape_for_insertion(mb_input, strlen(mb_input), str);

	free(mb_input);
	free(wide_input);

	return escaped;
}

static void
cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
		return;
	vle_compl_rewind();

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
	while(input_stat.index > 0 &&
			!cfg_is_word_wchar(input_stat.line[input_stat.index - 1]))
	{
		input_stat.curs_pos -= vifm_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
	}
	while(input_stat.index > 0 &&
			cfg_is_word_wchar(input_stat.line[input_stat.index - 1]))
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

	update_cmdline_text(&input_stat);
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

	if(sub_mode != CLS_COMMAND)
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
		update_cmdline_text(&input_stat);
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
	const int dot_index = input_stat.index;
	if(insert_str(completion) == 0)
	{
		input_stat.dot_index = dot_index;
		input_stat.dot_len = wcslen(completion);
		return 0;
	}
	return 1;
}

/* Inserts wide string into current cursor position.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
insert_str(const wchar_t str[])
{
	const size_t len = wcslen(str);
	const size_t new_len = input_stat.len + len + 1;

	wchar_t *const ptr = reallocarray(input_stat.line, new_len, sizeof(wchar_t));
	if(ptr == NULL)
	{
		return 1;
	}
	input_stat.line = ptr;

	wcsins(input_stat.line, str, input_stat.index + 1);

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
			&& !cfg_is_word_wchar(input_stat.line[input_stat.index]))
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
	}
	while(input_stat.index < input_stat.len
			&& cfg_is_word_wchar(input_stat.line[input_stat.index]))
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
	input_stat.curs_pos = input_stat.prompt_wid;
	update_cursor();
}

/* Moves cursor to the end of command-line on Ctrl+E and End. */
static void
cmd_end(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.index == input_stat.len)
		return;

	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid +
			vifm_wcswidth(input_stat.line, (size_t)-1);
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

	update_cmdline_text(&input_stat);
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
	if(sub_mode == CLS_COMMAND)
	{
		complete_prev(&cfg.cmd_hist, cfg.history_len);
	}
	else if(input_stat.search_mode)
	{
		complete_prev(&cfg.search_hist, cfg.history_len);
	}
	else if(sub_mode == CLS_PROMPT)
	{
		complete_prev(&cfg.prompt_hist, cfg.history_len);
	}
	else if(sub_mode == CLS_FILTER)
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
		{
			return;
		}
		++input_stat.cmd_pos;
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

	if(input_stat.cmd_pos > (int)len - 1)
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
	int index;

	stop_completion();

	index = input_stat.index;
	if(index == 0 || input_stat.len == 1)
	{
		return;
	}

	if(index == input_stat.len)
	{
		--index;
	}
	else
	{
		input_stat.curs_pos += vifm_wcwidth(input_stat.line[input_stat.index]);
		++input_stat.index;
	}

	char_before_last = input_stat.line[index - 1];
	input_stat.line[index - 1] = input_stat.line[index];
	input_stat.line[index] = char_before_last;

	update_cmdline_text(&input_stat);
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
	int required_height;
	input_stat.curs_pos = input_stat.prompt_wid +
			vifm_wcswidth(input_stat.line, input_stat.len);
	input_stat.index = input_stat.len;

	required_height = get_required_height();
	if(required_height >= getmaxy(status_bar))
	{
		update_cmdline_size();
	}

	update_cmdline_text(&input_stat);
}

/* Gets status bar height required to display all its content.  Returns the
 * height. */
static int
get_required_height(void)
{
	return DIV_ROUND_UP(input_stat.prompt_wid + input_stat.len + 1, line_width);
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
	wchar_t *wide_completed;

	const size_t new_len = (p - line_mb) + wide_len(completed) +
		(stat->len - stat->index) + 1;

	line_ending = vifm_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
	{
		return -1;
	}

	t = reallocarray(stat->line, new_len, sizeof(wchar_t));
	if(t == NULL)
	{
		free(line_ending);
		return -1;
	}
	stat->line = t;

	wide_completed = to_wide(completed);
	vifm_swprintf(stat->line + (p - line_mb), new_len,
			L"%" WPRINTF_WSTR L"%" WPRINTF_WSTR, wide_completed, line_ending);
	free(wide_completed);
	free(line_ending);

	update_line_stat(stat, new_len);
	update_cmdline_size();
	update_cmdline_text(stat);
	return 0;
}

/* Returns non-zero on error */
TSTATIC int
line_completion(line_stats_t *stat)
{
	static int offset;
	static char *line_mb;
	static const char *line_mb_cmd;

	char *completion;
	int result;

	if(!stat->complete_continue)
	{
		wchar_t t;
		CompletionPreProcessing compl_func_arg;

		/* Only complete the part before the cursor so just copy that part to
		 * line_mb. */
		t = stat->line[stat->index];
		stat->line[stat->index] = L'\0';

		free(line_mb);
		line_mb = to_multibyte(stat->line);
		if(line_mb == NULL)
		{
			return -1;
		}

		line_mb_cmd = find_last_command(line_mb);

		stat->line[stat->index] = t;

		vle_compl_reset();

		compl_func_arg = CPP_NONE;
		if(sub_mode == CLS_COMMAND)
		{
			const CmdLineLocation ipt = get_cmdline_location(line_mb,
					line_mb + strlen(line_mb));
			switch(ipt)
			{
				case CLL_OUT_OF_ARG:
				case CLL_NO_QUOTING:
					vle_compl_set_add_path_hook(&escaped_arg_hook);
					compl_func_arg = CPP_PERCENT_UNESCAPE;
					break;

				case CLL_S_QUOTING:
					vle_compl_set_add_path_hook(&squoted_arg_hook);
					compl_func_arg = CPP_SQUOTES_UNESCAPE;
					break;

				case CLL_D_QUOTING:
					vle_compl_set_add_path_hook(&dquoted_arg_hook);
					compl_func_arg = CPP_DQUOTES_UNESCAPE;
					break;

				case CLL_R_QUOTING:
					assert(0 && "Unexpected completion inside regexp argument.");
					break;
			}
		}

		offset = stat->complete(line_mb_cmd, (void *)compl_func_arg);

		vle_compl_set_add_path_hook(NULL);
	}

	vle_compl_set_order(input_stat.reverse_completion);

	if(vle_compl_get_count() == 0)
		return 0;

	completion = vle_compl_next();
	result = line_part_complete(stat, line_mb, line_mb_cmd + offset, completion);
	free(completion);

	if(vle_compl_get_count() >= 2)
		stat->complete_continue = 1;

	return result;
}

/* Processes completion match for insertion into command-line as escaped value.
 * Returns newly allocated string. */
static char *
escaped_arg_hook(const char match[])
{
#ifndef _WIN32
	return shell_like_escape(match, 1);
#else
	return strdup(escape_for_cd(match));
#endif
}

/* Processes completion match for insertion into command-line as a value in
 * single quotes.  Returns newly allocated string. */
static char *
squoted_arg_hook(const char match[])
{
	return escape_for_squotes(match, 1);
}

/* Processes completion match for insertion into command-line as a value in
 * double quotes.  Returns newly allocated string. */
static char *
dquoted_arg_hook(const char match[])
{
	return escape_for_dquotes(match, 1);
}

static void
update_line_stat(line_stats_t *stat, int new_len)
{
	stat->index += (new_len - 1) - stat->len;
	stat->curs_pos = stat->prompt_wid + vifm_wcswidth(stat->line, stat->index);
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
	vle_compl_reset();
	if(cfg.wild_menu &&
			(sub_mode != CLS_MENU_COMMAND && input_stat.complete != NULL))
	{
		if(cfg.display_statusline)
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
/* vim: set cinoptions+=t0 filetype=c : */
