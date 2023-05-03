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

#define CMDLINE_IMPL
#include "cmdline.h"

#include <curses.h>

#include <limits.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcslen() wcsncpy() */
#include <wctype.h> /* iswprint() */

#include "../cfg/config.h"
#include "../compat/curses.h"
#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"
#include "../engine/abbrevs.h"
#include "../engine/cmds.h"
#include "../engine/completion.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../engine/parsing.h"
#include "../engine/var.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/color_scheme.h"
#include "../ui/colors.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/statusline.h"
#include "../ui/quickview.h"
#include "../ui/ui.h"
#include "../utils/hist.h"
#include "../utils/macros.h"
#include "../utils/matcher.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../cmd_completion.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../filtering.h"
#include "../flist_pos.h"
#include "../flist_sel.h"
#include "../marks.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "dialogs/attr_dialog.h"
#include "dialogs/sort_dialog.h"
#include "menu.h"
#include "modes.h"
#include "normal.h"
#include "visual.h"
#include "wk.h"

/* Prompt prefix when navigation is enabled. */
#define NAV_PREFIX L"(nav)"

/* Stashed store of the state to support limited recursion. */
static line_stats_t prev_input_stat;
/* State of the command-line mode. */
static line_stats_t input_stat;

/* Width of the status bar. */
static int line_width = 1;

static int def_handler(wchar_t key);
static void update_cmdline_text(line_stats_t *stat);
static void draw_cmdline_text(line_stats_t *stat);
static void input_line_changed(void);
static void handle_empty_input(void);
static void handle_nonempty_input(void);
static void update_state(int result, int nmatches);
static void set_local_filter(const char value[]);
static wchar_t * wcsins(wchar_t src[], const wchar_t ins[], int pos);
static int enter_submode(CmdLineSubmode sub_mode, const char initial[],
		int reenter);
static void prepare_cmdline_mode(const wchar_t prompt[], const wchar_t cmd[],
		complete_cmd_func complete, CmdLineSubmode sub_mode, int allow_ee);
static void init_line_stats(line_stats_t *stat, const wchar_t prompt[],
		const wchar_t initial[], complete_cmd_func complete,
		CmdLineSubmode sub_mode, int allow_ee, int prev_mode);
static void save_view_port(line_stats_t *stat, view_t *view);
static void set_view_port(void);
static int is_line_edited(void);
static void leave_cmdline_mode(int cancelled);
static void free_line_stats(line_stats_t *stat);
static void cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static CmdInputType cls_to_editable_cit(CmdLineSubmode sub_mode);
static void extedit_prompt(const char input[], int cursor_col, int is_expr_reg,
		prompt_cb cb, void *cb_arg);
static void cmd_ctrl_rb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_h(key_info_t key_info, keys_info_t *keys_info);
static int should_quit_on_backspace(void);
static int no_initial_line(void);
static void cmd_ctrl_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_shift_tab(key_info_t key_info, keys_info_t *keys_info);
static void do_completion(void);
static void draw_wild_menu(int op);
static int draw_wild_bar(int *last_pos, int *pos, int *len);
static int draw_wild_popup(int *last_pos, int *pos, int *len);
static int compute_wild_menu_height(void);
static void cmd_ctrl_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void nav_open(void);
static int is_input_line_empty(void);
static void expand_abbrev(void);
TSTATIC const wchar_t * extract_abbrev(line_stats_t *stat, int *pos,
		int *no_remap);
static void exec_abbrev(const wchar_t abbrev_rhs[], int no_remap, int pos);
static void save_input_to_history(const keys_info_t *keys_info,
		const char input[]);
static void save_prompt_to_history(const char input[], int is_expr_reg);
static void finish_prompt_submode(const char input[], prompt_cb cb,
		void *cb_arg);
static CmdInputType search_cls_to_cit(CmdLineSubmode sub_mode);
static int is_forward_search(CmdLineSubmode sub_mode);
static int is_backward_search(CmdLineSubmode sub_mode);
static int replace_wstring(wchar_t **str, const wchar_t with[]);
static void cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info);
static void hist_next(line_stats_t *stat, const hist_t *hist, size_t len);
static void cmd_ctrl_requals(key_info_t key_info, keys_info_t *keys_info);
static void expr_reg_prompt_cb(const char expr[], void *arg);
static int expr_reg_prompt_completion(const char cmd[], void *arg);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xslash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xa(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xc(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxc(key_info_t key_info, keys_info_t *keys_info);
static void paste_short_path(view_t *view);
static void cmd_ctrl_xd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xe(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxe(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xm(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xr(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_xxr(key_info_t key_info, keys_info_t *keys_info);
static void paste_short_path_root(view_t *view);
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
static void cmd_delete(key_info_t key_info, keys_info_t *keys_info);
static void update_cursor(void);
TSTATIC void hist_prev(line_stats_t *stat, const hist_t *hist, size_t len);
static int replace_input_line(line_stats_t *stat, const char new[]);
static const hist_t * pick_hist(void);
static int is_cmdmode(int mode);
static void update_cmdline(line_stats_t *stat);
static int get_required_height(void);
static void cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info);
static void move_view_cursor(line_stats_t *stat, view_t *view, int new_pos);
static void cmd_ctrl_t(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void nav_start(line_stats_t *stat);
static void nav_stop(line_stats_t *stat);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_down(key_info_t key_info, keys_info_t *keys_info);
static void cmd_up(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right(key_info_t key_info, keys_info_t *keys_info);
static void cmd_home(key_info_t key_info, keys_info_t *keys_info);
static void cmd_end(key_info_t key_info, keys_info_t *keys_info);
static void cmd_page_up(key_info_t key_info, keys_info_t *keys_info);
static void cmd_page_down(key_info_t key_info, keys_info_t *keys_info);
#endif /* ENABLE_EXTENDED_KEYS */
static void update_cmdline_size(void);
TSTATIC int line_completion(line_stats_t *stat);
static char * escaped_arg_hook(const char match[]);
static char * squoted_arg_hook(const char match[]);
static char * dquoted_arg_hook(const char match[]);
static void update_line_stat(line_stats_t *stat, int new_len);
static int esc_wcswidth(const wchar_t str[], size_t n);
static int esc_wcwidth(wchar_t wc);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
static void stop_completion(void);
static void stop_dot_completion(void);
static void stop_regular_completion(void);
TSTATIC line_stats_t * get_line_stats(void);
static void handle_mouse_event(key_info_t key_info, keys_info_t *keys_info);

static keys_add_info_t builtin_cmds[] = {
	{WK_C_c,             {{&cmd_ctrl_c}, .descr = "leave cmdline mode"}},
	{WK_C_g,             {{&cmd_ctrl_g}, .descr = "edit cmdline in editor"}},
	{WK_C_h,             {{&cmd_ctrl_h}, .descr = "remove char to the left"}},
	{WK_C_i,             {{&cmd_ctrl_i}, .descr = "start/continue completion"}},
	{WK_C_j,             {{&cmd_ctrl_j}, .descr = "nav: leave without moving the cursor"}},
	{WK_C_k,             {{&cmd_ctrl_k}, .descr = "remove line part to the right"}},
	{WK_CR,              {{&cmd_return}, .descr = "execute/accept input"}},
	{WK_C_n,             {{&cmd_ctrl_n}, .descr = "recall next history item"}},
	{WK_C_o,             {{&cmd_ctrl_o}, .descr = "nav: go to parent directory"}},
	{WK_C_p,             {{&cmd_ctrl_p}, .descr = "recall previous history item"}},
	{WK_C_t,             {{&cmd_ctrl_t}, .descr = "swap adjacent characters"}},
	{WK_ESC,             {{&cmd_ctrl_c}, .descr = "leave cmdline mode"}},
	{WK_ESC WK_ESC,      {{&cmd_ctrl_c}, .descr = "leave cmdline mode"}},
	{WK_C_RB,            {{&cmd_ctrl_rb}, .descr = "expand abbreviation"}},
	{WK_C_USCORE,        {{&cmd_ctrl_underscore}, .descr = "reset completion"}},
	{WK_DELETE,          {{&cmd_ctrl_h}, .descr = "remove char to the left"}},
	{WK_ESC L"[Z",       {{&cmd_shift_tab}, .descr = "complete in reverse order"}},
	{WK_C_b,             {{&cmd_ctrl_b}, .descr = "move cursor to the left"}},
	{WK_C_f,             {{&cmd_ctrl_f}, .descr = "move cursor to the right"}},
	{WK_C_a,             {{&cmd_ctrl_a}, .descr = "move cursor to the beginning"}},
	{WK_C_e,             {{&cmd_ctrl_e}, .descr = "move cursor to the end"}},
	{WK_C_d,             {{&cmd_delete}, .descr = "delete current character"}},
	{WK_C_r WK_EQUALS,   {{&cmd_ctrl_requals}, .descr = "invoke expression register prompt"}},
	{WK_C_u,             {{&cmd_ctrl_u}, .descr = "remove line part to the left"}},
	{WK_C_w,             {{&cmd_ctrl_w}, .descr = "remove word to the left"}},
	{WK_C_y,             {{&cmd_ctrl_y}, .descr = "toggle navigation"}},
	{WK_C_x WK_SLASH,    {{&cmd_ctrl_xslash}, .descr = "insert last search pattern"}},
	{WK_C_x WK_a,        {{&cmd_ctrl_xa}, .descr = "insert implicit permanent filter value"}},
	{WK_C_x WK_c,        {{&cmd_ctrl_xc}, .descr = "insert current file name"}},
	{WK_C_x WK_d,        {{&cmd_ctrl_xd}, .descr = "insert current directory path"}},
	{WK_C_x WK_e,        {{&cmd_ctrl_xe}, .descr = "insert current file extension"}},
	{WK_C_x WK_m,        {{&cmd_ctrl_xm}, .descr = "insert explicit permanent filter value"}},
	{WK_C_x WK_r,        {{&cmd_ctrl_xr}, .descr = "insert root of current file name"}},
	{WK_C_x WK_t,        {{&cmd_ctrl_xt}, .descr = "insert name of current directory"}},
	{WK_C_x WK_C_x WK_c, {{&cmd_ctrl_xxc}, .descr = "insert other file name"}},
	{WK_C_x WK_C_x WK_d, {{&cmd_ctrl_xxd}, .descr = "insert other directory path"}},
	{WK_C_x WK_C_x WK_e, {{&cmd_ctrl_xxe}, .descr = "insert other file extension"}},
	{WK_C_x WK_C_x WK_r, {{&cmd_ctrl_xxr}, .descr = "insert root of other file name"}},
	{WK_C_x WK_C_x WK_t, {{&cmd_ctrl_xxt}, .descr = "insert name of other directory"}},
	{WK_C_x WK_EQUALS,   {{&cmd_ctrl_xequals}, .descr = "insert local filter value"}},
#ifndef __PDCURSES__
	{WK_ESC WK_b,     {{&cmd_meta_b}, .descr = "move cursor to previous word"}},
	{WK_ESC WK_d,     {{&cmd_meta_d}, .descr = "remove next word"}},
	{WK_ESC WK_f,     {{&cmd_meta_f}, .descr = "move cursor to next word"}},
	{WK_ESC WK_DOT,   {{&cmd_meta_dot}, .descr = "start/continue last arg completion"}},
#else
	{{K(ALT_B)},      {{&cmd_meta_b}, .descr = "move cursor to previous word"}},
	{{K(ALT_D)},      {{&cmd_meta_d}, .descr = "remove next word"}},
	{{K(ALT_F)},      {{&cmd_meta_f}, .descr = "move cursor to next word"}},
	{{K(ALT_PERIOD)}, {{&cmd_meta_dot}, .descr = "start/continue last arg completion"}},
#endif
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_BACKSPACE)}, {{&cmd_ctrl_h}, .descr = "remove char to the left"}},
	{{K(KEY_DOWN)},      {{&cmd_down},   .descr = "prefix-complete next history item/next item"}},
	{{K(KEY_UP)},        {{&cmd_up},     .descr = "prefix-complete previous history item/prev item"}},
	{{K(KEY_LEFT)},      {{&cmd_left},   .descr = "move cursor to the left/parent dir"}},
	{{K(KEY_RIGHT)},     {{&cmd_right},  .descr = "move cursor to the right/enter entry"}},
	{{K(KEY_HOME)},      {{&cmd_home},   .descr = "move cursor to the beginning/first item"}},
	{{K(KEY_END)},       {{&cmd_end},    .descr = "move cursor to the end/last item"}},
	{{K(KEY_DC)},        {{&cmd_delete}, .descr = "delete current character"}},
	{{K(KEY_BTAB)},      {{&cmd_shift_tab}, .descr = "complete in reverse order"}},
	{{K(KEY_PPAGE)},     {{&cmd_page_up},   .descr = "nav: scroll page up"}},
	{{K(KEY_NPAGE)},     {{&cmd_page_down}, .descr = "nav: scroll page down"}},
#endif /* ENABLE_EXTENDED_KEYS */
	{{K(KEY_MOUSE)},     {{&handle_mouse_event}, FOLLOWED_BY_NONE}},
};

void
modcline_init(void)
{
	int ret_code;

	vle_keys_set_def_handler(CMDLINE_MODE, &def_handler);

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), CMDLINE_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
modnav_init(void)
{
	vle_keys_set_def_handler(NAV_MODE, &def_handler);

	int ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), NAV_MODE);
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

	/* There is no need for resetting completion because of terminal dimensions
	 * change (note also that iswprint(KEY_RESIZE) might return false). */
	if(key == K(KEY_RESIZE))
	{
		return 0;
	}

	stop_completion();

	if(key != L'\r' && !iswprint(key))
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
		leave_cmdline_mode(/*cancelled=*/1);
		return 0;
	}

	input_stat.line = p;

	input_stat.index++;
	wcsins(input_stat.line, buf, input_stat.index);
	input_stat.len++;

	input_stat.curs_pos += esc_wcwidth(key);

	update_cmdline_size();
	update_cmdline_text(&input_stat);

	return 0;
}

/* Processes input change and redraw of resulting command-line. */
static void
update_cmdline_text(line_stats_t *stat)
{
	input_line_changed();
	draw_cmdline_text(stat);
}

/* Updates text displayed on the command line and cursor within it. */
static void
draw_cmdline_text(line_stats_t *stat)
{
	if(stats_silenced_ui())
	{
		/* XXX: erasing status_bar causes blinking if <silent> mapping that uses
		 *      command-line is pressed and held down.  Ideally wouldn't need this
		 *      return. */
		return;
	}

	int group = -1;
	col_attr_t prompt_col = {};

	werase(status_bar);

	switch(stat->state)
	{
		case PS_NORMAL:        group = CMD_LINE_COLOR; break;
		case PS_WRONG_PATTERN: group = ERROR_MSG_COLOR; break;
		case PS_NO_MATCH:      group = CMD_LINE_COLOR;
		                       prompt_col.attr = A_REVERSE;
		                       prompt_col.gui_attr = A_REVERSE;
		                       break;
	}
	prompt_col.attr ^= cfg.cs.color[group].attr;
	prompt_col.gui_attr ^= cfg.cs.color[group].gui_attr;

	wchar_t *escaped;

	escaped = escape_unreadablew(stat->prompt);
	ui_set_attr(status_bar, &prompt_col, cfg.cs.pair[group]);
	compat_mvwaddwstr(status_bar, 0, 0, escaped);
	free(escaped);

	escaped = escape_unreadablew(stat->line);
	ui_set_attr(status_bar, &cfg.cs.color[CMD_LINE_COLOR],
			cfg.cs.pair[CMD_LINE_COLOR]);
	compat_mvwaddwstr(status_bar, stat->prompt_wid/line_width,
			stat->prompt_wid%line_width, escaped);
	free(escaped);

	update_cursor();
	ui_refresh_win(status_bar);
}

/* Callback-like function, which is called every time input line is changed. */
static void
input_line_changed(void)
{
	if(!cfg.inc_search ||
			(!input_stat.search_mode && input_stat.sub_mode != CLS_FILTER))
	{
		return;
	}

	/* Hide cursor during view update, otherwise user might notice it blinking in
	 * wrong places. */
	ui_set_cursor(/*visibility=*/0);

	/* set_view_port() should not be called if none of the conditions are true. */

	input_stat.state = PS_NORMAL;
	if(is_input_line_empty())
	{
		free(input_stat.last_line);
		input_stat.last_line = NULL;

		set_view_port();
		handle_empty_input();
	}
	else if(input_stat.last_line == NULL ||
			wcscmp(input_stat.last_line, input_stat.line) != 0)
	{
		(void)replace_wstring(&input_stat.last_line, input_stat.line);

		set_view_port();
		handle_nonempty_input();
	}

	if(input_stat.prev_mode != MENU_MODE && input_stat.prev_mode != VISUAL_MODE)
	{
		fpos_set_pos(curr_view, curr_view->list_pos);
		qv_ui_updated();
		redraw_current_view();
	}
	else if(input_stat.prev_mode == MENU_MODE)
	{
		modmenu_partial_redraw();
	}

	/* Hardware cursor is moved on the screen only on refresh, so refresh status
	 * bar to force cursor moving there before it becomes visible again. */
	ui_refresh_win(status_bar);
	ui_set_cursor(/*visibility=*/1);
}

/* Provides reaction for empty input during interactive search/filtering. */
static void
handle_empty_input(void)
{
	/* Clear selection/highlight. */
	if(input_stat.prev_mode == MENU_MODE)
	{
		(void)menus_search("", input_stat.menu, 0);
	}
	else if(cfg.hl_search && input_stat.prev_mode != VISUAL_MODE)
	{
		flist_sel_stash(curr_view);
	}

	if(input_stat.prev_mode != MENU_MODE)
	{
		ui_view_reset_search_highlight(curr_view);
	}

	if(input_stat.prev_mode == VISUAL_MODE)
	{
		modvis_update();
	}

	if(input_stat.sub_mode == CLS_FILTER)
	{
		set_local_filter("");
	}
}

/* Provides reaction for non-empty input during interactive search/filtering. */
static void
handle_nonempty_input(void)
{
	char *const mbinput = to_multibyte(input_stat.line);
	int backward = 0;

	switch(input_stat.sub_mode)
	{
		int result;

		case CLS_BSEARCH: backward = 1; /* Fall through. */
		case CLS_FSEARCH:
			result = modnorm_find(curr_view, mbinput, backward, /*print_msg=*/0,
					&input_stat.search_match_found);
			update_state(result, curr_view->matches);
			break;
		case CLS_VBSEARCH: backward = 1; /* Fall through. */
		case CLS_VFSEARCH:
			result = modvis_find_interactive(curr_view, mbinput, backward,
					&input_stat.search_match_found);
			update_state(result, curr_view->matches);
			break;
		case CLS_MENU_FSEARCH:
		case CLS_MENU_BSEARCH:
			result = menus_search(mbinput, input_stat.menu, /*print_erros=*/0);
			update_state(result, menus_search_matched(input_stat.menu));
			break;
		case CLS_FILTER:
			set_local_filter(mbinput);
			break;

		default:
			assert("Unexpected command-line submode.");
			break;
	}

	free(mbinput);
}

/* Computes and sets new prompt state from result of a search and number of
 * matched items.  It is assumed that current state is PS_NORMAL. */
static void
update_state(int result, int nmatches)
{
	if(result < 0)
	{
		input_stat.state = PS_WRONG_PATTERN;
	}
	else if(nmatches == 0)
	{
		input_stat.state = PS_NO_MATCH;
	}
}

/* Updates value of the local filter of the current view. */
static void
set_local_filter(const char value[])
{
	const int rel_pos = input_stat.old_pos - input_stat.old_top;
	const int result = local_filter_set(curr_view, value);
	if(result != 0)
	{
		input_stat.state = (result < 0) ? PS_WRONG_PATTERN : PS_NO_MATCH;
	}
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
modcline_enter(CmdLineSubmode sub_mode, const char initial[])
{
	assert(sub_mode != CLS_MENU_COMMAND && sub_mode != CLS_MENU_FSEARCH &&
			sub_mode != CLS_MENU_BSEARCH &&
			"Use modcline_in_menu() for CLS_MENU_* submodes.");
	assert(sub_mode != CLS_PROMPT &&
			"Use modcline_prompt() for CLS_PROMPT submode.");
	if(enter_submode(sub_mode, initial, /*reenter=*/0) == 0 &&
			!is_null_or_empty(initial))
	{
		/* Trigger handling of input if its initial value isn't empty. */
		update_cmdline_text(&input_stat);
	}
}

void
modcline_in_menu(CmdLineSubmode sub_mode, const char initial[],
		struct menu_data_t *m)
{
	assert((sub_mode == CLS_MENU_COMMAND || sub_mode == CLS_MENU_FSEARCH ||
			sub_mode == CLS_MENU_BSEARCH) &&
			"modcline_in_menu() is only for CLS_MENU_* submodes.");

	if(enter_submode(sub_mode, initial, /*reenter=*/0) == 0)
	{
		input_stat.menu = m;
	}
}

/* Enters command-line editing submode.  Returns zero on success. */
static int
enter_submode(CmdLineSubmode sub_mode, const char initial[], int reenter)
{
	wchar_t *winitial;
	const wchar_t *wprompt;
	complete_cmd_func complete_func = NULL;

	if(sub_mode == CLS_FILTER && curr_view->custom.type == CV_DIFF)
	{
		show_error_msg("Filtering", "No local filter for diff views");
		return 1;
	}

	winitial = to_wide_force(initial);
	if(winitial == NULL)
	{
		show_error_msg("Error", "Not enough memory");
		return 1;
	}

	if(sub_mode == CLS_COMMAND || sub_mode == CLS_MENU_COMMAND)
	{
		wprompt = L":";
		complete_func = &vle_cmds_complete;
	}
	else if(sub_mode == CLS_FILTER)
	{
		wprompt = L"=";
	}
	else if(is_forward_search(sub_mode))
	{
		wprompt = L"/";
	}
	else if(is_backward_search(sub_mode))
	{
		wprompt = L"?";
	}
	else
	{
		assert(0 && "Unknown command line submode.");
		wprompt = L"E";
	}

	if(reenter)
	{
		int prev_mode = input_stat.prev_mode;
		free_line_stats(&input_stat);
		init_line_stats(&input_stat, wprompt, winitial, complete_func, sub_mode,
				/*allow_ee=*/0, prev_mode);

		/* Just in case. */
		curr_stats.save_msg = 1;

		stats_redraw_later();
	}
	else
	{
		prepare_cmdline_mode(wprompt, winitial, complete_func, sub_mode,
				/*allow_ee=*/0);
	}

	free(winitial);
	return 0;
}

void
modcline_prompt(const char prompt[], const char initial[], prompt_cb cb,
		void *cb_arg, complete_cmd_func complete, int allow_ee)
{
	wchar_t *wprompt = to_wide_force(prompt);
	wchar_t *winitial = to_wide_force(initial);

	if(wprompt == NULL || winitial == NULL)
	{
		show_error_msg("Error", "Not enough memory");
	}
	else
	{
		prepare_cmdline_mode(wprompt, winitial, complete, CLS_PROMPT, allow_ee);
		input_stat.prompt_callback = cb;
		input_stat.prompt_callback_arg = cb_arg;
	}

	free(wprompt);
	free(winitial);
}

void
modcline_redraw(void)
{
	/* Hide cursor during redraw, otherwise user might notice it blinking in wrong
	 * places. */
	ui_set_cursor(/*visibility=*/0);

	if(input_stat.prev_mode == MENU_MODE)
	{
		modmenu_full_redraw();
	}
	else
	{
		ui_redraw_as_background();
		if(input_stat.prev_mode == SORT_MODE)
		{
			redraw_sort_dialog();
		}
		else if(input_stat.prev_mode == ATTR_MODE)
		{
			redraw_attr_dialog();
		}
	}

	line_width = getmaxx(stdscr);
	update_cmdline_size();
	draw_cmdline_text(&input_stat);

	if(input_stat.complete_continue && cfg.wild_menu)
	{
		draw_wild_menu(-1);
	}

	if(input_stat.prev_mode != MENU_MODE)
	{
		update_all_windows();
	}

	/* Make cursor visible after all redraws. */
	ui_set_cursor(/*visibility=*/1);
}

/* Performs all necessary preparations for command-line mode to start
 * operating. */
static void
prepare_cmdline_mode(const wchar_t prompt[], const wchar_t initial[],
		complete_cmd_func complete, CmdLineSubmode sub_mode, int allow_ee)
{
	if(is_cmdmode(vle_mode_get()))
	{
		/* We're recursing into command-line mode. */
		ui_sb_unlock();
		prev_input_stat = input_stat;
	}

	line_width = getmaxx(stdscr);
	ui_sb_lock();

	init_line_stats(&input_stat, prompt, initial, complete, sub_mode, allow_ee,
			vle_mode_get());

	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);

	update_cmdline_size();
	draw_cmdline_text(&input_stat);

	curr_stats.save_msg = 1;

	if(input_stat.prev_mode == NORMAL_MODE)
		cmds_init();

	/* Make cursor visible only after all initial draws. */
	ui_set_cursor(/*visibility=*/1);
}

/* Initializes command-line status. */
static void
init_line_stats(line_stats_t *stat, const wchar_t prompt[],
		const wchar_t initial[], complete_cmd_func complete,
		CmdLineSubmode sub_mode, int allow_ee, int prev_mode)
{
	stat->sub_mode = sub_mode;
	stat->navigating = 0;
	stat->sub_mode_allows_ee = allow_ee;
	stat->menu = NULL;
	stat->prompt_callback = NULL;
	stat->prompt_callback_arg = NULL;

	stat->prev_mode = prev_mode;

	stat->line = vifm_wcsdup(initial);
	stat->last_line = NULL;
	stat->initial_line = vifm_wcsdup(stat->line);
	stat->index = wcslen(initial);
	stat->curs_pos = esc_wcswidth(stat->line, (size_t)-1);
	stat->len = stat->index;
	stat->cmd_pos = -1;
	stat->complete_continue = 0;
	stat->history_search = HIST_NONE;
	stat->line_buf = NULL;
	stat->reverse_completion = 0;
	stat->complete = complete;
	stat->search_mode = 0;
	stat->search_match_found = 0;
	stat->dot_pos = -1;
	stat->line_edited = 0;
	stat->enter_mapping_state = vle_keys_mapping_state();
	stat->expanding_abbrev = 0;
	stat->state = PS_NORMAL;

	if((is_forward_search(sub_mode) || is_backward_search(sub_mode)) &&
			sub_mode != CLS_VWFSEARCH && sub_mode != CLS_VWBSEARCH)
	{
		stat->search_mode = 1;
	}

	if(stat->search_mode || sub_mode == CLS_FILTER)
	{
		save_view_port(stat, curr_view);
	}

	wcsncpy(stat->prompt, prompt, ARRAY_LEN(stat->prompt));
	stat->prompt_wid = esc_wcswidth(stat->prompt, (size_t)-1);
	stat->curs_pos += stat->prompt_wid;
}

/* Stores view port parameters (top line, current position). */
static void
save_view_port(line_stats_t *stat, view_t *view)
{
	if(stat->prev_mode != MENU_MODE)
	{
		stat->old_top = view->top_line;
		stat->old_pos = view->list_pos;
	}
	else
	{
		modmenu_save_pos();
	}
}

/* Sets view port parameters to appropriate for current submode state. */
static void
set_view_port(void)
{
	if(input_stat.prev_mode == MENU_MODE)
	{
		modmenu_restore_pos();
		return;
	}

	if(input_stat.sub_mode != CLS_FILTER || !is_line_edited())
	{
		curr_view->top_line = input_stat.old_top;
		curr_view->list_pos = input_stat.old_pos;
	}
	else if(input_stat.sub_mode == CLS_FILTER)
	{
		/* Filtering itself doesn't update status line. */
		ui_stat_update(curr_view, 1);
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
leave_cmdline_mode(int cancelled)
{
	free_line_stats(&input_stat);

	if(is_cmdmode(vle_mode_get()))
	{
		if(is_cmdmode(input_stat.prev_mode))
		{
			/* We're restoring from a recursive command-line mode. */
			input_stat = prev_input_stat;
			/* Update is needed primarily on cancellation as callback isn't called
			 * then. */
			update_cmdline_text(&input_stat);
			return;
		}

		if(cancelled && input_stat.sub_mode == CLS_PROMPT)
		{
			/* Invoke callback with NULL for a result to inform about cancellation. */
			finish_prompt_submode(/*input=*/NULL, input_stat.prompt_callback,
					input_stat.prompt_callback_arg);
		}

		vle_mode_set(input_stat.prev_mode, VMT_PRIMARY);
	}

	/* Hide the cursor first. */
	ui_set_cursor(/*visibility=*/0);

	const int multiline_status_bar = (getmaxy(status_bar) > 1);

	wresize(status_bar, 1, getmaxx(stdscr) - FIELDS_WIDTH());
	if(multiline_status_bar)
	{
		stats_redraw_later();
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		if(input_stat.prev_mode == MENU_MODE)
		{
			wresize(menu_win, getmaxy(stdscr) - 1, getmaxx(stdscr));
			modmenu_partial_redraw();
		}
	}

	curr_stats.save_msg = 0;
	ui_sb_unlock();
	ui_sb_clear();

	if(!vle_mode_is(MENU_MODE))
	{
		ui_ruler_update(curr_view, 1);
	}

	if(input_stat.prev_mode != MENU_MODE && input_stat.prev_mode != VIEW_MODE)
	{
		redraw_current_view();
	}
}

/* Frees resources allocated for command-line status. */
static void
free_line_stats(line_stats_t *stat)
{
	free(input_stat.line);
	free(input_stat.last_line);
	free(input_stat.initial_line);
	free(input_stat.line_buf);
	input_stat.line = NULL;
	input_stat.last_line = NULL;
	input_stat.initial_line = NULL;
	input_stat.line_buf = NULL;
}

/* Moves command-line cursor to the beginning of command-line. */
static void
cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = input_stat.prompt_wid;
	update_cursor();
}

/* Moves command-line cursor to the left. */
static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index > 0)
	{
		input_stat.index--;
		input_stat.curs_pos -= esc_wcwidth(input_stat.line[input_stat.index]);
		update_cursor();
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

	if(input_stat.sub_mode != CLS_FILTER)
	{
		input_stat.line[0] = L'\0';
		input_line_changed();
	}

	const line_stats_t old_input_stat = input_stat;
	leave_cmdline_mode(/*cancelled=*/1);

	if(old_input_stat.prev_mode == VISUAL_MODE)
	{
		if(!old_input_stat.search_mode)
		{
			modvis_leave(curr_stats.save_msg, 1, 1);
			fpos_set_pos(curr_view, marks_find_in_view(curr_view, '<'));
		}
	}

	if(old_input_stat.sub_mode == CLS_COMMAND)
	{
		curr_stats.save_msg = cmds_dispatch("", curr_view, CIT_COMMAND);
	}
	else if(old_input_stat.sub_mode == CLS_FILTER)
	{
		local_filter_cancel(curr_view);
		curr_view->top_line = old_input_stat.old_top;
		curr_view->list_pos = old_input_stat.old_pos;
		redraw_current_view();
	}
}

/* Moves command-line cursor to the end of command-line. */
static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.index == input_stat.len)
		return;

	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid +
			esc_wcswidth(input_stat.line, (size_t)-1);
	update_cursor();
}

/* Moves command-line cursor to the right. */
static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(input_stat.index < input_stat.len)
	{
		input_stat.curs_pos += esc_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
		update_cursor();
	}
}

/* Opens the editor with already typed in characters, gets entered line and
 * executes it as if it was typed. */
static void
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	const CmdInputType type = cls_to_editable_cit(input_stat.sub_mode);
	const int prompt_ee = (input_stat.sub_mode == CLS_PROMPT)
	                   && input_stat.sub_mode_allows_ee;
	if(type != (CmdInputType)-1 || prompt_ee)
	{
		char *const mbstr = to_multibyte(input_stat.line);
		const int prev_mode = input_stat.prev_mode;
		const CmdLineSubmode sub_mode = input_stat.sub_mode;
		const prompt_cb prompt_callback = input_stat.prompt_callback;
		void *const prompt_callback_arg = input_stat.prompt_callback_arg;
		const int index = input_stat.index;

		leave_cmdline_mode(/*cancelled=*/0);

		if(sub_mode == CLS_FILTER)
		{
			local_filter_cancel(curr_view);
		}

		if(prompt_ee)
		{
			int is_expr_reg = is_cmdmode(prev_mode);
			extedit_prompt(mbstr, index + 1, is_expr_reg, prompt_callback,
					prompt_callback_arg);
		}
		else
		{
			cmds_run_ext(mbstr, index + 1, type);
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
extedit_prompt(const char input[], int cursor_col, int is_expr_reg,
		prompt_cb cb, void *cb_arg)
{
	CmdInputType type = (is_expr_reg ? CIT_EXPRREG_INPUT : CIT_PROMPT_INPUT);
	char *const ext_cmd = cmds_get_ext(input, cursor_col, type);

	if(ext_cmd != NULL)
	{
		save_prompt_to_history(ext_cmd, is_expr_reg);
	}
	else
	{
		save_prompt_to_history(input, is_expr_reg);

		ui_sb_err("Error querying data from external source.");
		curr_stats.save_msg = 1;
	}

	/* Invoking this will NULL in case of error to report it. */
	finish_prompt_submode(ext_cmd, cb, cb_arg);
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

	input_stat.curs_pos -= esc_wcwidth(input_stat.line[input_stat.index]);

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
	    && input_stat.sub_mode != CLS_PROMPT
	    && (input_stat.sub_mode != CLS_FILTER || no_initial_line());
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

	/* Indicate that status line is being reused for wild menu and there is a
	 * potential usage conflict due to size differences. */
	if(cfg.display_statusline && !curr_stats.reusing_statusline &&
			cfg.wild_menu && vle_compl_get_count() > 2 && getmaxy(stat_win) > 1)
	{
		curr_stats.reusing_statusline = 1;
		wresize(stat_win, compute_wild_menu_height(), getmaxx(stdscr));
		modcline_redraw();
	}
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

	int pos = vle_compl_get_pos();
	const int count = vle_compl_get_count() - 1;
	int i;
	int len = getmaxx(stdscr);

	/* This check should go first to ensure that resetting of wild menu is
	 * processed and no returns will break the expected behaviour. */
	if(op > 0)
	{
		last_pos = 0;
		return;
	}

	if(input_stat.complete == NULL || count < 2)
	{
		return;
	}

	if(pos == 0 || pos == count)
		last_pos = 0;
	if(last_pos == 0 && pos == count - 1)
		last_pos = count;

	i = cfg.wild_popup
	  ? draw_wild_popup(&last_pos, &pos, &len)
	  : draw_wild_bar(&last_pos, &pos, &len);

	if(pos > 0 && pos != count)
	{
		last_pos = pos;
		draw_wild_menu(op);
		return;
	}
	if(op == 0 && len < 2 && i - 1 == pos)
		last_pos = i;
	ui_refresh_win(stat_win);

	update_cursor();
}

/* Draws wild menu as a bar.  Returns index of the last displayed item. */
static int
draw_wild_bar(int *last_pos, int *pos, int *len)
{
	int i;

	const vle_compl_t *const items = vle_compl_get_items();
	const int count = vle_compl_get_count() - 1;

	wresize(stat_win, compute_wild_menu_height(), getmaxx(stdscr));
	checked_wmove(stat_win, 0, 0);
	werase(stat_win);
	ui_stat_reposition(getmaxy(status_bar), 1);

	if(*pos < *last_pos)
	{
		int l = *len;
		while(*last_pos > 0 && l > 2)
		{
			--*last_pos;

			char *escaped = escape_unreadable(items[*last_pos].text);
			l -= strlen(escaped);
			free(escaped);

			if(*last_pos != 0)
				l -= 2;
		}
		if(l < 2)
			++*last_pos;
	}

	for(i = *last_pos; i < count && *len > 0; ++i)
	{
		char *escaped = escape_unreadable(items[i].text);

		*len -= strlen(escaped);
		if(i != 0)
			*len -= 2;

		if(i == *last_pos && *last_pos > 0)
		{
			wprintw(stat_win, "< ");
		}
		else if(i > *last_pos)
		{
			if(*len < 2)
			{
				free(escaped);
				wprintw(stat_win, " >");
				break;
			}
			wprintw(stat_win, "  ");
		}

		if(i == *pos)
		{
			col_attr_t col = cfg.cs.color[STATUS_LINE_COLOR];
			cs_mix_colors(&col, &cfg.cs.color[WILD_MENU_COLOR]);
			ui_set_attr(stat_win, &col, -1);
		}
		wprint(stat_win, escaped);
		if(i == *pos)
		{
			ui_set_attr(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
					cfg.cs.pair[STATUS_LINE_COLOR]);
			*pos = -*pos;
		}

		free(escaped);
	}

	return i;
}

/* Draws wild menu as a popup.  Returns index of the last displayed item. */
static int
draw_wild_popup(int *last_pos, int *pos, int *len)
{
	const vle_compl_t *const items = vle_compl_get_items();
	const int count = vle_compl_get_count() - 1;
	const int height = compute_wild_menu_height();
	size_t max_title_width;
	int i, j;

	if(*pos < *last_pos)
	{
		*last_pos = MAX(0, *last_pos - height);
	}

	ui_stat_reposition(getmaxy(status_bar), height);
	wresize(stat_win, height, getmaxx(stdscr));
	ui_set_attr(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
			cfg.cs.pair[STATUS_LINE_COLOR]);
	werase(stat_win);

	max_title_width = 0U;
	for(i = *last_pos, j = 0; i < count && j < height; ++i, ++j)
	{
		char *escaped = escape_unreadable(items[i].text);
		const size_t width = utf8_strsw(escaped);
		free(escaped);

		if(width > max_title_width)
		{
			max_title_width = width;
		}
	}

	for(i = *last_pos, j = 0; i < count && j < height; ++i, ++j)
	{
		if(i == *pos)
		{
			col_attr_t col = cfg.cs.color[STATUS_LINE_COLOR];
			cs_mix_colors(&col, &cfg.cs.color[WILD_MENU_COLOR]);
			ui_set_attr(stat_win, &col, -1);
		}

		checked_wmove(stat_win, j, 0);
		char *escaped = escape_unreadable(items[i].text);
		ui_stat_draw_popup_line(stat_win, escaped, items[i].descr, max_title_width);
		free(escaped);

		if(i == *pos)
		{
			ui_set_attr(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
					cfg.cs.pair[STATUS_LINE_COLOR]);
			*pos = -*pos;
		}
	}

	return i;
}

/* Computes height needed for wild menu (bar or popup).  Returns the height. */
static int
compute_wild_menu_height(void)
{
	if(!cfg.wild_popup)
	{
		return 1;
	}

	const int term_height = getmaxy(stdscr);
	const int count = vle_compl_get_count() - 1;

	const int height_limit = MAX(10, term_height*2/5);
	const int max_height = term_height - getmaxy(status_bar)
	                     - ui_stat_job_bar_height() - 1;
	return MIN(count, MIN(height_limit, max_height));
}

/* Leaves navigation mode without undoing cursor position or filter state. */
static void
cmd_ctrl_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		if(input_stat.sub_mode == CLS_FILTER)
		{
			local_filter_accept(curr_view, /*update_history=*/0);
		}
		leave_cmdline_mode(/*cancelled=*/0);
	}
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
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	/* TODO: refactor this cmd_return() function. */
	if(input_stat.navigating)
	{
		nav_open();
		return;
	}

	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	CmdLineSubmode sub_mode = input_stat.sub_mode;

	if(is_input_line_empty() && sub_mode == CLS_MENU_COMMAND)
	{
		leave_cmdline_mode(/*cancelled=*/0);
		return;
	}

	expand_abbrev();

	char *input = to_multibyte(input_stat.line);
	save_input_to_history(keys_info, input);

	const int prev_mode = input_stat.prev_mode;
	struct menu_data_t *const menu = input_stat.menu;
	const prompt_cb prompt_callback = input_stat.prompt_callback;
	void *const prompt_callback_arg = input_stat.prompt_callback_arg;
	const int search_mode = input_stat.search_mode;
	leave_cmdline_mode(/*cancelled=*/0);

	if(prev_mode == VISUAL_MODE && sub_mode != CLS_VFSEARCH &&
			sub_mode != CLS_VBSEARCH)
	{
		modvis_leave(curr_stats.save_msg, 1, 0);
	}

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

		if(sub_mode == CLS_COMMAND)
		{
			cmds_scope_start();
		}
		curr_stats.save_msg = cmds_dispatch(real_start, curr_view, cmd_type);
		if(sub_mode == CLS_COMMAND)
		{
			if(cmds_scope_finish() != 0)
			{
				curr_stats.save_msg = 1;
			}
		}
	}
	else if(sub_mode == CLS_PROMPT)
	{
		finish_prompt_submode(input, prompt_callback, prompt_callback_arg);
	}
	else if(sub_mode == CLS_FILTER)
	{
		if(cfg.inc_search)
		{
			local_filter_accept(curr_view, /*update_history=*/1);
		}
		else
		{
			local_filter_apply(curr_view, input);
			ui_view_schedule_reload(curr_view);
		}
	}
	else if(!cfg.inc_search || prev_mode == VIEW_MODE || input[0] == '\0')
	{
		const char *const pattern = (input[0] == '\0') ? hists_search_last()
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
					curr_stats.save_msg = cmds_dispatch1(pattern, curr_view, cit);
					break;
				}
			case CLS_MENU_FSEARCH:
			case CLS_MENU_BSEARCH:
				stats_refresh_later();
				curr_stats.save_msg = menus_search(pattern, menu, 1);
				break;

			default:
				assert(0 && "Unknown command line submode.");
				break;
		}
	}
	else if(cfg.inc_search && search_mode)
	{
		if(prev_mode == MENU_MODE)
		{
			modmenu_partial_redraw();
			menus_search_print_msg(menu);
			curr_stats.save_msg = 1;
		}
		else
		{
			curr_stats.save_msg = print_search_result(curr_view,
					input_stat.search_match_found, is_backward_search(sub_mode),
					&print_search_msg);
		}
	}

	free(input);
}

/* Opens current entry while navigating. */
static void
nav_open(void)
{
	dir_entry_t *curr = get_current_entry(curr_view);
	if(fentry_is_fake(curr))
	{
		return;
	}

	int is_dir_like = (fentry_is_dir(curr) || is_unc_root(curr_view->curr_dir));

	CmdLineSubmode sub_mode = input_stat.sub_mode;
	if(sub_mode == CLS_FILTER)
	{
		/* Accepting filter changes list of files, but in case of error it might be
		 * better. */
		local_filter_accept(curr_view, /*update_history=*/0);
	}

	if(is_dir_like)
	{
		char *initial = to_multibyte(input_stat.line);
		if(rn_enter_dir(curr_view) == 0)
		{
			replace_string(&initial, "");
		}

		/* Auto-commands on entering a directory can do something that will require
		 * a postponed view update.  Do it here, so that we don't work with file
		 * list that's about to be changed.  In particular re-entering submode saves
		 * top and cursor positions. */
		switch(ui_view_query_scheduled_event(curr_view))
		{
			case UUE_NONE:
				/* Nothing to do. */
				break;
			case UUE_REDRAW:
				ui_view_schedule_redraw(curr_view);
				break;
			case UUE_RELOAD:
				load_saving_pos(curr_view);
				break;
		}

		if(initial != NULL)
		{
			enter_submode(sub_mode, initial, /*reenter=*/1);
			nav_start(&input_stat);

			free(initial);
		}
	}
	else
	{
		leave_cmdline_mode(/*cancelled=*/0);
		if(cfg.nav_open_files)
		{
			rn_open(curr_view, FHE_RUN);
		}
	}
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

	/* Don't expand command-line abbreviations in navigation and avoid recursion
	 * on expanding abbreviations. */
	if(input_stat.navigating || input_stat.expanding_abbrev)
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
	input_stat.curs_pos -= esc_wcswidth(&input_stat.line[pos], lhs_len);
	(void)wcsdel(input_stat.line, pos + 1, lhs_len);

	if(no_remap)
	{
		(void)vle_keys_exec_timed_out_no_remap(abbrev_rhs);
	}
	else
	{
		(void)vle_keys_exec_timed_out(abbrev_rhs);
	}
}

/* Saves command-line input into appropriate history.  input can be NULL, in
 * which case it's ignored. */
static void
save_input_to_history(const keys_info_t *keys_info, const char input[])
{
	if(input_stat.search_mode)
	{
		hists_search_save(input);
	}
	else if(ONE_OF(input_stat.sub_mode, CLS_COMMAND, CLS_MENU_COMMAND))
	{
		/* XXX: skipping should probably work for all histories. */
		const int mapped_input = input_stat.enter_mapping_state != 0 &&
			vle_keys_mapping_state() == input_stat.enter_mapping_state;
		const int ignore_input = mapped_input || keys_info->recursive;
		if(!ignore_input)
		{
			if(input_stat.sub_mode == CLS_COMMAND)
			{
				hists_commands_save(input);
			}
			else
			{
				hists_menucmd_save(input);
			}
		}
	}
	else if(input_stat.sub_mode == CLS_PROMPT)
	{
		const int is_expr_reg = is_cmdmode(input_stat.prev_mode);
		save_prompt_to_history(input, is_expr_reg);
	}
}

/* Save prompt input to history. */
static void
save_prompt_to_history(const char input[], int is_expr_reg)
{
	if(is_expr_reg)
	{
		hists_exprreg_save(input);
	}
	else
	{
		hists_prompt_save(input);
	}
}

/* Performs final actions on successful querying of prompt input. */
static void
finish_prompt_submode(const char input[], prompt_cb cb, void *cb_arg)
{
	modes_post();
	modes_pre();

	cb(input, cb_arg);
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
	update_cmdline(&input_stat);
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

/* Recalls a newer historical entry or moves view cursor if navigating. */
static void
cmd_ctrl_n(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		if(curr_view->list_pos < curr_view->list_rows - 1)
		{
			move_view_cursor(&input_stat, curr_view, curr_view->list_pos + 1);
		}
		return;
	}

	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	input_stat.history_search = HIST_GO;

	hist_next(&input_stat, pick_hist(), cfg.history_len);
}

/* Puts next element in the history or restores user input if end of history has
 * been reached.  hist can be NULL, in which case nothing happens. */
static void
hist_next(line_stats_t *stat, const hist_t *hist, size_t len)
{
	if(hist == NULL || hist_is_empty(hist))
	{
		return;
	}

	if(stat->history_search != HIST_SEARCH)
	{
		if(stat->cmd_pos <= 0)
		{
			restore_user_input();
			return;
		}
		--stat->cmd_pos;
	}
	else
	{
		int pos = stat->cmd_pos;
		int len = stat->hist_search_len;
		while(--pos >= 0)
		{
			wchar_t *const buf = to_wide(hist->items[pos].text);
			if(wcsncmp(stat->line, buf, len) == 0)
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
		stat->cmd_pos = pos;
	}

	(void)replace_input_line(stat, hist->items[stat->cmd_pos].text);

	update_cmdline(stat);

	if(stat->cmd_pos > (int)len - 1)
	{
		stat->cmd_pos = len - 1;
	}
}

/* Invokes expression register prompt. */
static void
cmd_ctrl_requals(key_info_t key_info, keys_info_t *keys_info)
{
	input_stat.history_search = HIST_NONE;
	stop_completion();

	if(!is_cmdmode(input_stat.prev_mode))
	{
		modcline_prompt("(=)", "", &expr_reg_prompt_cb, /*cb_arg=*/NULL,
				&expr_reg_prompt_completion, /*allow_ee=*/1);
	}
}

/* Handles result of expression register prompt. */
static void
expr_reg_prompt_cb(const char expr[], void *arg)
{
	/* Try to parse expr and convert the result to string on success. */
	parsing_result_t result = vle_parser_eval(expr, /*interactive=*/0);
	if(result.error != PE_NO_ERROR)
	{
		/* TODO: maybe print error message on status bar. */
		var_free(result.value);
		return;
	}

	char *res_str = var_to_str(result.value);
	var_free(result.value);

	wchar_t *wide = to_wide_force(res_str);
	free(res_str);

	if(insert_str(wide) == 0)
	{
		update_cmdline_text(&input_stat);
	}

	free(wide);
}

/* Completion for expression register prompt.  Returns completion start
 * offset. */
static int
expr_reg_prompt_completion(const char cmd[], void *arg)
{
	const char *start;
	complete_expr(cmd, &start);
	return (start - cmd);
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
		input_stat.curs_pos -= esc_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
	}
	if(iswalnum(input_stat.line[input_stat.index - 1]))
	{
		while(input_stat.index > 0 &&
				iswalnum(input_stat.line[input_stat.index - 1]))
		{
			const wchar_t curr_wchar = input_stat.line[input_stat.index - 1];
			input_stat.curs_pos -= esc_wcwidth(curr_wchar);
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
			input_stat.curs_pos -= esc_wcwidth(curr_wchar);
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
	paste_str(hists_search_last(), 0);
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
paste_short_path(view_t *view)
{
	if(flist_custom_active(view))
	{
		char short_path[PATH_MAX + 1];
		get_short_path_of(view, get_current_entry(view), NF_NONE, 0,
				sizeof(short_path), short_path);
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
	paste_str(matcher_get_undec(curr_view->manual_filter), 0);
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
paste_short_path_root(view_t *view)
{
	char short_path[PATH_MAX + 1];
	get_short_path_of(view, get_current_entry(view), NF_NONE, 0,
			sizeof(short_path), short_path);
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

	if(input_stat.prev_mode == MENU_MODE)
	{
		return;
	}

	stop_completion();

	escaped = (allow_escaping && input_stat.sub_mode == CLS_COMMAND)
	        ? escape_cmd_for_pasting(str)
	        : NULL;
	if(escaped != NULL)
	{
		str = escaped;
	}

	wide = to_wide_force(str);

	if(insert_str((str[0] != '\0' && wide[0] == L'\0') ? L"?" : wide) == 0)
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

	escaped = cmds_insertion_escape(mb_input, strlen(mb_input), str);

	free(mb_input);
	free(wide_input);

	return escaped;
}

static void
cmd_ctrl_underscore(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.complete_continue)
	{
		return;
	}

	/* Restore initial user input (before completion). */
	vle_compl_rewind();
	input_stat.reverse_completion = 0;
	if(input_stat.complete != NULL)
	{
		line_completion(&input_stat);
	}

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
		input_stat.curs_pos -= esc_wcwidth(input_stat.line[input_stat.index - 1]);
		input_stat.index--;
	}
	while(input_stat.index > 0 &&
			cfg_is_word_wchar(input_stat.line[input_stat.index - 1]))
	{
		input_stat.curs_pos -= esc_wcwidth(input_stat.line[input_stat.index - 1]);
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

	if(input_stat.sub_mode != CLS_COMMAND)
	{
		return;
	}

	stop_regular_completion();

	if(hist_is_empty(&curr_stats.cmd_hist))
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
	if(input_stat.dot_pos >= curr_stats.cmd_hist.size)
	{
		return vifm_wcsdup(L"");
	}

	size_t len;
	const char *last_entry = curr_stats.cmd_hist.items[input_stat.dot_pos++].text;
	const char *last_arg_pos = vle_cmds_last_arg(last_entry, 1, &len);

	char *last_arg = format_str("%.*s", (int)len, last_arg_pos);
	wchar_t *wide = to_wide(last_arg);
	free(last_arg);

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
	input_stat.curs_pos += esc_wcswidth(str, (size_t)-1);
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
		input_stat.curs_pos += esc_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
	}
	while(input_stat.index < input_stat.len
			&& cfg_is_word_wchar(input_stat.line[input_stat.index]))
	{
		input_stat.curs_pos += esc_wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
	}
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

/* Replaces current input with specified string.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
replace_input_line(line_stats_t *stat, const char new[])
{
	wchar_t *const wide_new = to_wide(new);
	if(wide_new == NULL)
	{
		return 1;
	}

	free(stat->line);
	stat->line = wide_new;
	stat->len = wcslen(wide_new);
	return 0;
}

/* Goes to parent directory when in navigation. */
static void
cmd_ctrl_o(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.navigating)
	{
		return;
	}

	CmdLineSubmode sub_mode = input_stat.sub_mode;
	if(sub_mode == CLS_FILTER)
	{
		local_filter_accept(curr_view, /*update_history=*/0);
	}

	rn_leave(curr_view, /*levels=*/1);
	enter_submode(sub_mode, /*initial=*/"", /*reenter=*/1);
	nav_start(&input_stat);
}

/* Recalls an older historical entry or moves view cursor if navigating. */
static void
cmd_ctrl_p(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		if(curr_view->list_pos > 0)
		{
			move_view_cursor(&input_stat, curr_view, curr_view->list_pos - 1);
		}
		return;
	}

	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	input_stat.history_search = HIST_GO;

	hist_prev(&input_stat, pick_hist(), cfg.history_len);
}

/* Moves cursor in the view and updates information related to its position. */
static void
move_view_cursor(line_stats_t *stat, view_t *view, int new_pos)
{
	fpos_set_pos(view, new_pos);

	if(stat->sub_mode == CLS_FILTER)
	{
		local_filter_update_pos(view);
	}
	else
	{
		save_view_port(stat, view);
	}
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
		input_stat.curs_pos += esc_wcwidth(input_stat.line[input_stat.index]);
		++input_stat.index;
	}

	char_before_last = input_stat.line[index - 1];
	input_stat.line[index - 1] = input_stat.line[index];
	input_stat.line[index] = char_before_last;

	update_cmdline_text(&input_stat);
}

/* Toggles navigation for search/filtering. */
static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(!input_stat.search_mode && input_stat.sub_mode != CLS_FILTER)
	{
		return;
	}
	if(!cfg.inc_search || input_stat.prev_mode != NORMAL_MODE)
	{
		return;
	}

	if(!input_stat.navigating)
	{
		nav_start(&input_stat);
	}
	else
	{
		nav_stop(&input_stat);
	}
	draw_cmdline_text(&input_stat);
}

/* Enables navigation. */
static void
nav_start(line_stats_t *stat)
{
	stat->navigating = 1;

	size_t nav_prefix_len = wcslen(NAV_PREFIX);

	wchar_t new_prompt[NAME_MAX + 1] = NAV_PREFIX;
	wcsncpy(new_prompt + nav_prefix_len, stat->prompt,
			ARRAY_LEN(new_prompt) - nav_prefix_len - 1);

	wcscpy(stat->prompt, new_prompt);
	stat->prompt_wid += nav_prefix_len;
	stat->curs_pos += nav_prefix_len;

	vle_mode_set(NAV_MODE, VMT_SECONDARY);
}

/* Disables navigation. */
static void
nav_stop(line_stats_t *stat)
{
	stat->navigating = 0;

	size_t nav_prefix_len = wcslen(NAV_PREFIX);

	memmove(stat->prompt, &stat->prompt[nav_prefix_len],
			sizeof(stat->prompt) - sizeof(wchar_t)*nav_prefix_len);
	stat->prompt_wid -= nav_prefix_len;
	stat->curs_pos -= nav_prefix_len;

	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);
}

#ifdef ENABLE_EXTENDED_KEYS

/* Fetches a matching future historical entry or moves view cursor down in
 * navigation. */
static void
cmd_down(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		cmd_ctrl_n(key_info, keys_info);
		return;
	}

	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	if(input_stat.history_search != HIST_SEARCH)
	{
		input_stat.history_search = HIST_SEARCH;
		input_stat.hist_search_len = input_stat.len;
	}

	hist_next(&input_stat, pick_hist(), cfg.history_len);
}

/* Fetches a matching past historical entry or moves view cursor up in
 * navigation. */
static void
cmd_up(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		cmd_ctrl_p(key_info, keys_info);
		return;
	}

	stop_completion();

	if(input_stat.history_search == HIST_NONE)
		save_users_input();

	if(input_stat.history_search != HIST_SEARCH)
	{
		input_stat.history_search = HIST_SEARCH;
		input_stat.hist_search_len = input_stat.len;
	}

	hist_prev(&input_stat, pick_hist(), cfg.history_len);
}

/* Moves command-line cursor to the left or goes to parent directory in
 * navigation. */
static void
cmd_left(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		cmd_ctrl_o(key_info, keys_info);
	}
	else
	{
		cmd_ctrl_b(key_info, keys_info);
	}
}

/* Moves command-line cursor to the right or enter active entry in
 * navigation. */
static void
cmd_right(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		cmd_return(key_info, keys_info);
	}
	else
	{
		cmd_ctrl_f(key_info, keys_info);
	}
}

/* Moves command-line cursor to the beginning of command-line or moves
 * view cursor to the first file in navigation. */
static void
cmd_home(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		fpos_set_pos(curr_view, 0);
	}
	else
	{
		cmd_ctrl_a(key_info, keys_info);
	}
}

/* Moves command-line cursor to the end of command-line or moves view cursor to
 * the last file in navigation. */
static void
cmd_end(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		fpos_set_pos(curr_view, curr_view->list_rows - 1);
	}
	else
	{
		cmd_ctrl_e(key_info, keys_info);
	}
}

/* Scrolls view up in navigation. */
static void
cmd_page_up(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		fview_scroll_page_up(curr_view);
	}
}

/* Scrolls view down in navigation. */
static void
cmd_page_down(key_info_t key_info, keys_info_t *keys_info)
{
	if(input_stat.navigating)
	{
		fview_scroll_page_down(curr_view);
	}
}

#endif /* ENABLE_EXTENDED_KEYS */

/* Puts previous element in the history.  hist can be NULL, in which case
 * nothing happens. */
TSTATIC void
hist_prev(line_stats_t *stat, const hist_t *hist, size_t len)
{
	if(hist == NULL || hist_is_empty(hist))
	{
		return;
	}

	if(stat->history_search != HIST_SEARCH)
	{
		if(stat->cmd_pos == hist->size - 1)
		{
			return;
		}

		++stat->cmd_pos;

		/* Handle the case when the most recent history item equals current input.
		 * This can happen if command-line mode was given some initial input
		 * string.  Initially cmd_pos is -1, no need to check anything if history
		 * contains only one element as even if it's equal input line won't be
		 * changed. */
		if(stat->cmd_pos == 0 && hist->size != 1)
		{
			wchar_t *const wide_item = to_wide(hist->items[0].text);
			if(wcscmp(stat->line, wide_item) == 0)
			{
				++stat->cmd_pos;
			}
			free(wide_item);
		}
	}
	else
	{
		int pos = stat->cmd_pos;
		int len = stat->hist_search_len;
		while(++pos < hist->size)
		{
			wchar_t *const wide_item = to_wide(hist->items[pos].text);
			if(wcsncmp(stat->line, wide_item, len) == 0)
			{
				free(wide_item);
				break;
			}
			free(wide_item);
		}
		if(pos >= hist->size)
			return;
		stat->cmd_pos = pos;
	}

	(void)replace_input_line(stat, hist->items[stat->cmd_pos].text);

	update_cmdline(stat);

	if(stat->cmd_pos > (int)len - 1)
	{
		stat->cmd_pos = len - 1;
	}
}

/* Picks history depending on sub_mode.  Returns picked history or NULL. */
static const hist_t *
pick_hist(void)
{
	if(input_stat.sub_mode == CLS_COMMAND)
	{
		return &curr_stats.cmd_hist;
	}
	if(input_stat.sub_mode == CLS_MENU_COMMAND)
	{
		return &curr_stats.menucmd_hist;
	}
	if(input_stat.search_mode)
	{
		return &curr_stats.search_hist;
	}
	if(input_stat.sub_mode == CLS_PROMPT)
	{
		if(is_cmdmode(input_stat.prev_mode))
		{
			return &curr_stats.exprreg_hist;
		}
		return &curr_stats.prompt_hist;
	}
	if(input_stat.sub_mode == CLS_FILTER)
	{
		return &curr_stats.filter_hist;
	}
	return NULL;
}

/* Checks for a command-line-like mode. */
static int
is_cmdmode(int mode)
{
  return (mode == CMDLINE_MODE || mode == NAV_MODE);
}

/* Updates command-line properties and redraws it. */
static void
update_cmdline(line_stats_t *stat)
{
	int required_height;
	stat->curs_pos = stat->prompt_wid + esc_wcswidth(stat->line, stat->len);
	stat->index = stat->len;

	required_height = get_required_height();
	if(required_height >= getmaxy(status_bar))
	{
		update_cmdline_size();
	}

	update_cmdline_text(stat);
}

/* Gets status bar height required to display all its content.  Returns the
 * height. */
static int
get_required_height(void)
{
	return DIV_ROUND_UP(input_stat.prompt_wid + input_stat.len + 1, line_width);
}

/* Replaces [ stat->prefix_len : stat->index ) range of stat->line with wide
 * version of the completed parameter.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
line_part_complete(line_stats_t *stat, const char completed[])
{
	void *t;
	wchar_t *line_ending;
	wchar_t *wide_completed = to_wide_force(completed);

	const size_t new_len = stat->prefix_len + wcslen(wide_completed) +
		(stat->len - stat->index) + 1;

	line_ending = vifm_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
	{
		free(wide_completed);
		return -1;
	}

	t = reallocarray(stat->line, new_len, sizeof(wchar_t));
	if(t == NULL)
	{
		free(wide_completed);
		free(line_ending);
		return -1;
	}
	stat->line = t;

	vifm_swprintf(stat->line + stat->prefix_len, new_len,
			L"%" WPRINTF_WSTR L"%" WPRINTF_WSTR, wide_completed, line_ending);
	free(wide_completed);
	free(line_ending);

	update_line_stat(stat, new_len);
	update_cmdline_size();
	update_cmdline_text(stat);
	return 0;
}

/* Make sure status bar size is taken into account in the TUI. */
static void
update_cmdline_size(void)
{
	const int required_height = MIN(getmaxy(stdscr), get_required_height());
	if(required_height < getmaxy(status_bar))
	{
		/* Do not shrink status bar. */
		return;
	}

	/* This handles case when status bar didn't get larger to cover corner case
	 * updates (e.g. first redraw). */

	mvwin(status_bar, getmaxy(stdscr) - required_height, 0);
	wresize(status_bar, required_height, line_width);

	if(input_stat.prev_mode != MENU_MODE)
	{
		int stat_height = cfg.wild_menu
		               && cfg.wild_popup
		               && input_stat.complete_continue;
		if(ui_stat_reposition(required_height, stat_height ? getmaxy(stat_win) : 0))
		{
			ui_stat_refresh();
		}
	}
	else
	{
		wresize(menu_win, getmaxy(stdscr) - required_height, getmaxx(stdscr));
		modmenu_partial_redraw();
	}
}

/* Returns non-zero on error. */
TSTATIC int
line_completion(line_stats_t *stat)
{
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

		char *const line_mb = to_multibyte(stat->line);
		stat->line[stat->index] = t;
		if(line_mb == NULL)
		{
			return -1;
		}

		const char *line_mb_cmd = line_mb;

		vle_compl_reset();

		compl_func_arg = CPP_NONE;
		if(stat->sub_mode == CLS_COMMAND || stat->sub_mode == CLS_MENU_COMMAND)
		{
			line_mb_cmd = cmds_find_last(line_mb);

			const CmdLineLocation ipt = cmds_classify_pos(line_mb,
					line_mb + strlen(line_mb));
			switch(ipt)
			{
				case CLL_OUT_OF_ARG:
				case CLL_NO_QUOTING:
				case CLL_R_QUOTING:
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
			}
		}

		const int offset = stat->complete(line_mb_cmd, (void *)compl_func_arg);
		if(offset >= 0 && offset < (int)strlen(line_mb_cmd))
		{
			line_mb[line_mb_cmd - line_mb + offset] = '\0';
		}
		stat->prefix_len = wide_len(line_mb);
		free(line_mb);

		vle_compl_set_add_path_hook(NULL);
	}

	vle_compl_set_order(stat->reverse_completion);

	if(vle_compl_get_count() == 0)
		return 0;

	completion = vle_compl_next();
	result = line_part_complete(stat, completion);
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
	return posix_like_escape(match, /*type=*/1);
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

/* Recalculates some numeric fields of the stat structure that depend on input
 * string length. */
static void
update_line_stat(line_stats_t *stat, int new_len)
{
	stat->index += (new_len - 1) - stat->len;
	stat->curs_pos = stat->prompt_wid + esc_wcswidth(stat->line, stat->index);
	stat->len = new_len - 1;
}

/* wcswidth()-like function which also accounts for unreadable characters that
 * were escaped. */
static int
esc_wcswidth(const wchar_t str[], size_t n)
{
	int width = 0;
	while(*str != L'\0' && n--)
	{
		width += esc_wcwidth(*str++);
	}
	return width;
}

/* wcwidth()-like function which also accounts for unreadable characters that
 * were escaped. */
static int
esc_wcwidth(wchar_t wc)
{
	/* Escaped unreadable characters have a form of ^X. */
	return (unichar_isprint(wc) ? vifm_wcwidth(wc) : 2);
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

/* Disables all active completions. */
static void
stop_completion(void)
{
	stop_dot_completion();
	stop_regular_completion();
}

/* Disables dot completion if it's active. */
static void
stop_dot_completion(void)
{
	input_stat.dot_pos = -1;
}

/* Disables tab completion if it's active. */
static void
stop_regular_completion(void)
{
	if(!input_stat.complete_continue)
	{
		return;
	}

	if(curr_stats.reusing_statusline)
	{
		curr_stats.reusing_statusline = 0;
	}

	input_stat.complete_continue = 0;
	vle_compl_reset();
	if(cfg.wild_menu && input_stat.complete != NULL)
	{
		modcline_redraw();
	}
}

int
modcline_complete_dirs(const char str[], void *arg)
{
	int name_offset = after_last(str, '/') - str;
	return name_offset
	     + filename_completion(str, CT_DIRONLY, /*skip_canonicalization=*/0);
}

int
modcline_complete_files(const char str[], void *arg)
{
	int name_offset = after_last(str, '/') - str;
	return name_offset
	     + filename_completion(str, CT_ALL, /*skip_canonicalization=*/0);
}

TSTATIC line_stats_t *
get_line_stats(void)
{
	return &input_stat;
}

/* Processes events from the mouse. */
static void
handle_mouse_event(key_info_t key_info, keys_info_t *keys_info)
{
	MEVENT e;
	if(ui_get_mouse(&e) != OK)
	{
		return;
	}

	if(!wenclose(status_bar, e.y, e.x))
	{
		return;
	}

	if(e.bstate & BUTTON1_PRESSED)
	{
		wmouse_trafo(status_bar, &e.y, &e.x, FALSE);

		int idx = e.y*getmaxx(status_bar) + e.x - input_stat.prompt_wid;
		if(idx < 0)
		{
			idx = 0;
		}
		else if(idx > input_stat.len)
		{
			idx = input_stat.len;
		}

		input_stat.index = idx;
		input_stat.curs_pos = input_stat.index + input_stat.prompt_wid;
		update_cmdline_text(&input_stat);
	}
	else if(e.bstate & BUTTON4_PRESSED)
	{
		cmd_ctrl_p(key_info, keys_info);
	}
	else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
	{
		cmd_ctrl_n(key_info, keys_info);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
