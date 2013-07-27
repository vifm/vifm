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

#include "menu.h"

#include <regex.h>

#include <curses.h>

#include <assert.h> /* assert() */
#include <ctype.h>
#include <string.h>

#include "../cfg/config.h"
#include "../engine/cmds.h"
#include "../engine/keys.h"
#include "../menus/menus.h"
#include "../utils/macros.h"
#include "../utils/utils.h"
#include "../bookmarks.h"
#include "../commands.h"
#include "../filelist.h"
#include "../fileops.h"
#include "../status.h"
#include "../ui.h"
#include "cmdline.h"
#include "modes.h"

static const int SCROLL_GAP = 2;

static int complete_args(int id, const char *args, int argc, char **argv,
		int arg_pos);
static int swap_range(void);
static int resolve_mark(char mark);
static char * menu_expand_macros(const char *str, int for_shell, int *usr1,
		int *usr2);
static char * menu_expand_envvars(const char *str);
static void post(int id);
static void menu_select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

static int key_handler(wchar_t key);
static void leave_menu_mode(void);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static int can_scroll_menu_up(const menu_info *menu);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static int can_scroll_menu_down(const menu_info *menu);
static void change_menu_top(menu_info *const menu, int delta);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static int get_effective_menu_scroll_offset(const menu_info *menu);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_question(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_H(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_dd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(int backward);
static void cmd_zb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zL(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zh(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zz(key_info_t key_info, keys_info_t *keys_info);
static int all_lines_visible(const menu_info *const menu);

static int goto_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);

static int search_menu(menu_info *m, int start_pos);
static int search_menu_forwards(menu_info *m, int start_pos);
static int search_menu_backwards(menu_info *m, int start_pos);

static int *mode;
static FileView *view;
static menu_info *menu;
static int last_search_backward;
static int was_redraw;
static int saved_top, saved_pos;
static int search_repeat;

static keys_add_info_t builtin_cmds[] = {
	{L"\x02", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	/* return */
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"\x15", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x19", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"/", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L":", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L"?", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_question}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"N", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"dd", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dd}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"n", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"zb", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zb}}},
	{L"zH", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zH}}},
	{L"zL", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zL}}},
	{L"zh", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zh}}},
	{L"zl", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zl}}},
	{L"zt", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zt}}},
	{L"zz", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zz}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{{L'z', KEY_LEFT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zh}}},
	{{L'z', KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zl}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

static const cmd_add_t commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "quit",             .abbr = "q",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "xit",              .abbr = "x",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
};

static cmds_conf_t cmds_conf = {
	.complete_args = complete_args,
	.swap_range = swap_range,
	.resolve_mark = resolve_mark,
	.expand_macros = menu_expand_macros,
	.expand_envvars = menu_expand_envvars,
	.post = post,
	.select_range = menu_select_range,
	.skip_at_beginning = skip_at_beginning,
};

static int
complete_args(int id, const char *args, int argc, char **argv, int arg_pos)
{
	return 0;
}

static int
swap_range(void)
{
	return 0;
}

static int
resolve_mark(char mark)
{
	return -1;
}

static char *
menu_expand_macros(const char *str, int for_shell, int *usr1, int *usr2)
{
	return strdup(str);
}

static char *
menu_expand_envvars(const char *str)
{
	return strdup(str);
}

static void
post(int id)
{
}

static void
menu_select_range(int id, const cmd_info_t *cmd_info)
{
}

static int
skip_at_beginning(int id, const char *args)
{
	return -1;
}

void
init_menu_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), MENU_MODE);
	assert(ret_code == 0);

	set_def_handler(MENU_MODE, key_handler);

	init_cmds(0, &cmds_conf);
	add_builtin_commands((const cmd_add_t *)&commands, ARRAY_LEN(commands));
}

static int
key_handler(wchar_t key)
{
	wchar_t buf[] = {key, L'\0'};

	if(menu->key_handler == NULL)
		return 0;

	if(menu->key_handler(menu, buf) > 0)
		wrefresh(menu_win);

	if(menu->len == 0)
	{
		show_error_msg("No more items in the menu", "Menu will be closed");
		leave_menu_mode();
	}

	return 0;
}

void
enter_menu_mode(menu_info *m, FileView *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	assert(m->len > 0 && "Menu cannot be empty.");

	werase(status_bar);

	view = active_view;
	menu = m;
	*mode = MENU_MODE;
	curr_stats.need_update = UT_FULL;
	was_redraw = 0;

	init_cmds(0, &cmds_conf);
}

void
menu_pre(void)
{
	touchwin(pos_win);
	wrefresh(pos_win);
}

void
menu_post(void)
{
	if(curr_stats.need_update != UT_NONE)
	{
		touchwin(menu_win);
		wrefresh(menu_win);
		curr_stats.need_update = UT_NONE;
	}
}

void
menu_redraw(void)
{
	was_redraw = 1;
	redraw_menu(menu);

	if(curr_stats.errmsg_shown)
	{
		redraw_error_msg_window();
		redrawwin(error_win);
		wnoutrefresh(error_win);
		doupdate();
	}
}

static void
leave_menu_mode(void)
{
	reset_popup_menu(menu);

	clean_selected_files(view);
	redraw_view(view);

	*mode = NORMAL_MODE;
	if(was_redraw)
		update_screen(UT_FULL);
	else
		update_all_windows();
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_up(menu))
	{
		const int s = get_effective_menu_scroll_offset(menu);
		const int off = (menu->win_rows - 2) - SCROLL_GAP;
		menu->pos = get_last_visible_line(menu) - off;
		change_menu_top(menu, -off);
		if(cfg.scroll_off > 0 && menu->top + (menu->win_rows - 3) - menu->pos < s)
			menu->pos -= s - (menu->top + (menu->win_rows - 3) - menu->pos);

		update_menu();
	}
}

/* Returns non-zero if menu can be scrolled up. */
static int
can_scroll_menu_up(const menu_info *menu)
{
	return menu->top > 0;
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_menu_mode();
}

static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
{
	const int s = get_effective_menu_scroll_offset(menu);
	clean_menu_position(menu);
	menu->top += DIV_ROUND_UP(menu->win_rows - 3, 2);
	menu->pos += DIV_ROUND_UP(menu->win_rows - 3, 2);
	if(cfg.scroll_off > 0 && menu->pos - menu->top < s)
		menu->pos += s - (menu->pos - menu->top);

	update_menu();
}

static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		int off = MAX(cfg.scroll_off, 0);
		if(menu->pos <= menu->top + off)
			menu->pos = menu->top + 1 + off;

		menu->top++;
		update_menu();
	}
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		const int s = get_effective_menu_scroll_offset(menu);
		const int off = (menu->win_rows - 2) - SCROLL_GAP;
		menu->pos = menu->top + off;
		change_menu_top(menu, off);
		if(cfg.scroll_off > 0 && menu->pos - menu->top < s)
			menu->pos += s - (menu->pos - menu->top);

		update_menu();
	}
}

/* Returns non-zero if menu can be scrolled down. */
static int
can_scroll_menu_down(const menu_info *menu)
{
	return get_last_visible_line(menu) < menu->len - 1;
}

/* Moves top line of the menu ensuring that its value is correct. */
static void
change_menu_top(menu_info *const menu, int delta)
{
	menu->top = MAX(MIN(menu->top + delta, menu->len - (menu->win_rows - 2)), 0);
}

int
get_last_visible_line(const menu_info *menu)
{
	return menu->top + (menu->win_rows - 2) - 1;
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	redraw_menu(menu);
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	static menu_info *saved_menu;

	*mode = NORMAL_MODE;
	saved_menu = menu;
	if(execute_menu_cb(curr_view, menu) != 0)
	{
		*mode = MENU_MODE;
		menu_redraw();
		return;
	}

	if(*mode != MENU_MODE)
	{
		reset_popup_menu(saved_menu);
	}
	else if(menu != saved_menu)
	{
		reset_popup_menu(saved_menu);
		update_menu();
	}

	if(was_redraw)
		update_screen(UT_FULL);
	else
		update_all_windows();
}

static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	const int s = get_effective_menu_scroll_offset(menu);
	clean_menu_position(menu);

	if(cfg.scroll_off > 0 && menu->top + menu->win_rows - menu->pos < s)
		menu->pos -= s - (menu->top + (menu->win_rows - 3) - menu->pos);

	menu->top -= DIV_ROUND_UP(menu->win_rows - 3, 2);
	if(menu->top < 0)
		menu->top = 0;
	menu->pos -= DIV_ROUND_UP(menu->win_rows - 3, 2);

	update_menu();
}

/* Returns scroll offset value for the menu taking menu height into account. */
static int
get_effective_menu_scroll_offset(const menu_info *menu)
{
	return MIN(DIV_ROUND_UP(menu->win_rows - 3, 2) - 1, cfg.scroll_off);
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_up(menu))
	{
		int off = MAX(cfg.scroll_off, 0);
		if(menu->pos >= menu->top + menu->win_rows - 3 - off)
			menu->pos = menu->top - 1 + menu->win_rows - 3 - off;

		menu->top--;
		update_menu();
	}
}

static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	last_search_backward = 0;
	menu->match_dir = NONE;
	free(menu->regexp);
	menu->regexp = NULL;
	enter_cmdline_mode(MENU_SEARCH_FORWARD_SUBMODE, L"", menu);
}

static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	cmds_conf.begin = 1;
	cmds_conf.current = menu->pos;
	cmds_conf.end = menu->len;
	enter_cmdline_mode(MENU_CMD_SUBMODE, L"", menu);
}

static void
cmd_question(key_info_t key_info, keys_info_t *keys_info)
{
	search_repeat = (key_info.count == NO_COUNT_GIVEN) ? 1 : key_info.count;
	last_search_backward = 1;
	menu->match_dir = NONE;
	free(menu->regexp);
	enter_cmdline_mode(MENU_SEARCH_BACKWARD_SUBMODE, L"", menu);
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = menu->len;

	clean_menu_position(menu);
	move_to_menu_pos(key_info.count - 1, menu);
	wrefresh(menu_win);
}

static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	int top;
	int off = MAX(cfg.scroll_off, 0);
	if(off > menu->win_rows/2)
		return;

	if(menu->top == 0)
		top = 0;
	else
		top = menu->top + off;

	clean_menu_position(menu);
	move_to_menu_pos(top, menu);
	wrefresh(menu_win);
}

static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	int top;
	int off;
	if(menu->key_handler != NULL)
	{
		int ret = menu->key_handler(menu, L"L");
		if(ret == 0)
			return;
		else if(ret > 0)
		{
			wrefresh(menu_win);
			return;
		}
	}

	off = MAX(cfg.scroll_off, 0);
	if(off > menu->win_rows/2)
		return;

	if(menu->top + menu->win_rows < menu->len - 1)
		top = menu->top + menu->win_rows - off;
	else
		top = menu->top + menu->win_rows;

	clean_menu_position(menu);
	move_to_menu_pos(top - 3, menu);
	wrefresh(menu_win);
}

static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	int new_pos;
	if(menu->len < menu->win_rows)
		new_pos = menu->len/2;
	else
		new_pos = menu->top + (menu->win_rows - 3)/2;

	clean_menu_position(menu);
	move_to_menu_pos(new_pos, menu);
	wrefresh(menu_win);
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		search(!last_search_backward);
}

static void
cmd_dd(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->key_handler == NULL)
		return;

	if(menu->key_handler(menu, L"dd") > 0)
		wrefresh(menu_win);

	if(menu->len == 0)
	{
		show_error_msg("No more items in the menu", "Menu will be closed");
		leave_menu_mode();
	}
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	move_to_menu_pos(key_info.count - 1, menu);
	wrefresh(menu_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->pos == menu->len - 1)
		return;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos += key_info.count;
	move_to_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->pos == 0)
		return;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos -= key_info.count;
	move_to_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	while(key_info.count-- > 0)
		search(last_search_backward);
}

static void
search(int backward)
{
	if(menu->regexp != NULL)
	{
		int was_msg;
		menu->match_dir = backward ? UP : DOWN;
		was_msg = search_menu_list(NULL, menu);
		wrefresh(menu_win);

		if(!was_msg)
			status_bar_messagef("%c%s", backward ? '?' : '/', menu->regexp);
	}
	else
	{
		status_bar_error("No search pattern set");
	}
	curr_stats.save_msg = 1;
}

static void
cmd_zb(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_up(menu))
	{
		if(menu->pos < menu->win_rows)
			menu->top = 0;
		else
			menu->top = menu->pos - (menu->win_rows - 3);
		update_menu();
	}
}

static void
cmd_zH(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->hor_pos == 0)
		return;
	menu->hor_pos = MAX(0, menu->hor_pos - (getmaxx(menu_win) - 4));
	update_menu();
}

static void
cmd_zL(key_info_t key_info, keys_info_t *keys_info)
{
	menu->hor_pos += getmaxx(menu_win) - 4;
	update_menu();
}

static void
cmd_zh(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->hor_pos == 0)
		return;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	menu->hor_pos = MAX(0, menu->hor_pos - key_info.count);
	update_menu();
}

static void
cmd_zl(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	menu->hor_pos += key_info.count;
	update_menu();
}

static void
cmd_zt(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		if(menu->len - menu->pos >= menu->win_rows - 3 + 1)
			menu->top = menu->pos;
		else
			menu->top = menu->len - (menu->win_rows - 3 + 1);
		update_menu();
	}
}

static void
cmd_zz(key_info_t key_info, keys_info_t *keys_info)
{
	if(!all_lines_visible(menu))
	{
		if(menu->pos <= (menu->win_rows - 3)/2)
			menu->top = 0;
		else if(menu->pos > menu->len - DIV_ROUND_UP(menu->win_rows - 3, 2))
			menu->top = menu->len - (menu->win_rows - 3 + 1);
		else
			menu->top = menu->pos - DIV_ROUND_UP(menu->win_rows - 3, 2);

		update_menu();
	}
}

/* Returns non-zero if all menu lines are visible, so no scrolling is needed. */
static int
all_lines_visible(const menu_info *const menu)
{
	return menu->len <= menu->win_rows - 2;
}

void
update_menu(void)
{
	draw_menu(menu);
	move_to_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static int
goto_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->end == NOT_DEF)
		return 0;
	clean_menu_position(menu);
	move_to_menu_pos(cmd_info->end, menu);
	wrefresh(menu_win);
	return 0;
}

static int
quit_cmd(const cmd_info_t *cmd_info)
{
	leave_menu_mode();
	return 0;
}

void
save_menu_pos(void)
{
	saved_top = menu->top;
	saved_pos = menu->pos;
}

void
load_menu_pos(void)
{
	menu->top = saved_top;
	menu->pos = saved_pos;
}

int
search_menu_list(const char *pattern, menu_info *m)
{
	int save = 0;
	int i;

	if(pattern != NULL)
	{
		m->regexp = strdup(pattern);
		if(search_menu(m, m->pos) != 0)
		{
			draw_menu(m);
			move_to_menu_pos(m->pos, m);
			return 1;
		}
		draw_menu(m);
	}

	for(i = 0; i < search_repeat; i++)
		switch(m->match_dir)
		{
			case NONE:
				save = search_menu_forwards(m, m->pos + 1);
				break;
			case UP:
				save = search_menu_backwards(m, m->pos - 1);
				break;
			case DOWN:
				save = search_menu_forwards(m, m->pos + 1);
				break;
			default:
				break;
		}
	return save;
}

/* Returns non-zero on error */
static int
search_menu(menu_info *m, int start_pos)
{
	int cflags;
	regex_t re;
	int err;

	if(m->matches == NULL)
		m->matches = malloc(sizeof(int)*m->len);

	memset(m->matches, 0, sizeof(int)*m->len);

	if(m->regexp[0] == '\0')
		return 0;

	cflags = get_regexp_cflags(m->regexp);
	if((err = regcomp(&re, m->regexp, cflags)) == 0)
	{
		int x;
		m->matching_entries = 0;
		for(x = 0; x < m->len; x++)
		{
			if(regexec(&re, m->items[x], 0, NULL, 0) != 0)
				continue;
			m->matches[x] = 1;

			m->matching_entries++;
		}
		regfree(&re);
		return 0;
	}
	else
	{
		status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return -1;
	}
}

static int
search_menu_forwards(menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int x;

	for(x = 0; x < m->len; x++)
	{
		if(!m->matches[x])
			continue;

		if(match_up < 0)
		{
			if(x < start_pos)
				match_up = x;
		}
		if(match_down < 0)
		{
			if(x >= start_pos)
				match_down = x;
		}
	}

	if(match_up > -1 || match_down > -1)
	{
		int pos;

		if(!cfg.wrap_scan && match_down <= -1)
		{
			status_bar_errorf("Search hit BOTTOM without match for: %s", m->regexp);
			return 1;
		}

		pos = (match_down > -1) ? match_down : match_up;

		clean_menu_position(m);
		move_to_menu_pos(pos, m);
		status_bar_messagef("%d %s", m->matching_entries,
				(m->matching_entries == 1) ? "match" : "matches");
	}
	else
	{
		move_to_menu_pos(m->pos, m);
		if(!cfg.wrap_scan)
			status_bar_errorf("Search hit BOTTOM without match for: %s", m->regexp);
		else
			status_bar_errorf("No matches for: %s", m->regexp);
		return 1;
	}
	return 0;
}

static int
search_menu_backwards(menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int x;

	for(x = m->len - 1; x > -1; x--)
	{
		if(!m->matches[x])
			continue;

		if(match_up < 0)
		{
			if(x <= start_pos)
				match_up = x;
		}
		if(match_down < 0)
		{
			if(x > start_pos)
				match_down = x;
		}
	}

	if(match_up > -1 || match_down > -1)
	{
		int pos;

		if(!cfg.wrap_scan && match_up <= -1)
		{
			status_bar_errorf("Search hit TOP without match for: %s", m->regexp);
			return 1;
		}

		pos = (match_up > -1) ? match_up : match_down;

		clean_menu_position(m);
		move_to_menu_pos(pos, m);
		status_bar_messagef("%d %s", m->matching_entries,
				(m->matching_entries == 1) ? "match" : "matches");
	}
	else
	{
		move_to_menu_pos(m->pos, m);
		if(!cfg.wrap_scan)
			status_bar_errorf("Search hit TOP without match for: %s", m->regexp);
		else
			status_bar_errorf("No matches for: %s", m->regexp);
		return 1;
	}
	return 0;
}

void
execute_cmdline_command(const char cmd[])
{
	if(exec_command(cmd, curr_view, GET_COMMAND) < 0)
	{
		status_bar_error("An error occuried while trying to execute command");
	}
	init_cmds(0, &cmds_conf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
