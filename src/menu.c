/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <curses.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "../config.h"

#include "bookmarks.h"
#include "cmdline.h"
#include "cmds.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "menus.h"
#include "modes.h"
#include "status.h"
#include "ui.h"

#include "menu.h"

static int *mode;
static FileView *view;
static menu_info *menu;
static int last_search_backward;
static int was_redraw;

static int complete_args(int id, const char *args, int argc, char **argv,
		int arg_pos);
static int swap_range(void);
static int resolve_mark(char mark);
static char * menu_expand_macros(const char *str, int *usr1, int *usr2);
static void post(int id);
static void menu_select_range(int id, const struct cmd_info *cmd_info);

static int key_handler(wchar_t key);
static void leave_menu_mode(void);
static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_d(struct key_info, struct keys_info *);
static void cmd_ctrl_e(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_ctrl_l(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_ctrl_u(struct key_info, struct keys_info *);
static void cmd_ctrl_y(struct key_info, struct keys_info *);
static void cmd_slash(struct key_info, struct keys_info *);
static void cmd_colon(struct key_info, struct keys_info *);
static void cmd_question(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_H(struct key_info, struct keys_info *);
static void cmd_L(struct key_info, struct keys_info *);
static void cmd_M(struct key_info, struct keys_info *);
static void cmd_N(struct key_info, struct keys_info *);
static void cmd_dd(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_n(struct key_info, struct keys_info *);
static void cmd_zb(struct key_info, struct keys_info *);
static void cmd_zt(struct key_info, struct keys_info *);
static void cmd_zz(struct key_info, struct keys_info *);

static int goto_cmd(const struct cmd_info *cmd_info);
static int quit_cmd(const struct cmd_info *cmd_info);

static struct keys_add_info builtin_cmds[] = {
	{L"\x02", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{L"\x03", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x04", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_d}}},
	{L"\x05", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_e}}},
	{L"\x06", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{L"\x0c", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	/* return */
	{L"\x0d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x15", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x19", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_y}}},
	/* escape */
	{L"\x1b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"/", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_slash}}},
	{L":", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_colon}}},
	{L"?", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_question}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"H", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_H}}},
	{L"L", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_L}}},
	{L"M", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_M}}},
	{L"N", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_N}}},
	{L"dd", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_dd}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"n", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_n}}},
	{L"zb", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zb}}},
	{L"zt", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zt}}},
	{L"zz", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_zz}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_b}}},
	{{KEY_NPAGE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_f}}},
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{{KEY_HOME}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

static const struct cmd_add commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "quit",             .abbr = "q",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "xit",              .abbr = "x",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
};

static struct cmds_conf cmds_conf = {
	.complete_args = complete_args,
	.swap_range = swap_range,
	.resolve_mark = resolve_mark,
	.expand_macros = menu_expand_macros,
	.post = post,
	.select_range = menu_select_range,
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
menu_expand_macros(const char *str, int *usr1, int *usr2)
{
	return strdup(str);
}

static void
post(int id)
{
}

static void
menu_select_range(int id, const struct cmd_info *cmd_info)
{
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
	add_buildin_commands((const struct cmd_add *)&commands, ARRAY_LEN(commands));
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
	if(curr_stats.vifm_started != 2)
		return;

	werase(status_bar);

	view = active_view;
	menu = m;
	*mode = MENU_MODE;
	curr_stats.need_redraw = 1;
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
	if(curr_stats.need_redraw)
	{
		touchwin(menu_win);
		wrefresh(menu_win);
		curr_stats.need_redraw = 0;
	}
}

void
menu_redraw(void)
{
	was_redraw = 1;
	redraw_menu(menu);

	if(curr_stats.errmsg_shown)
	{
		redraw_error_msg(NULL, NULL);
		redrawwin(error_win);
		wnoutrefresh(error_win);
		doupdate();
	}
}

static void
leave_menu_mode(void)
{
	reset_popup_menu(menu);
	*mode = NORMAL_MODE;
	if(was_redraw)
		redraw_window();
}

static void
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->pos -= menu->win_rows - 3;
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_menu_mode();
}

static void
cmd_ctrl_d(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->top += (menu->win_rows - 3 + 1)/2;
	menu->pos += (menu->win_rows - 3 + 1)/2;
	draw_menu(menu);
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_e(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->len <= menu->win_rows - 3)
		return;
	if(menu->top == menu->len - (menu->win_rows - 3) - 1)
		return;
	if(menu->pos == menu->top)
		menu->pos++;

	menu->top++;
	update_menu();
}

static void
cmd_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->pos += menu->win_rows - 3;
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_l(struct key_info key_info, struct keys_info *keys_info)
{
	draw_menu(menu);
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
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
		redraw_window();
}

static void
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->top -= (menu->win_rows - 3 + 1)/2;
	if(menu->top < 0)
		menu->top = 0;
	menu->pos -= (menu->win_rows - 3 + 1)/2;
	draw_menu(menu);
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_y(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->len <= menu->win_rows - 3 || menu->top == 0)
		return;
	if(menu->pos == menu->top + menu->win_rows - 3)
		menu->pos--;

	menu->top--;
	update_menu();
}

static void
cmd_slash(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 0;
	menu->match_dir = NONE;
	free(menu->regexp);
	menu->regexp = NULL;
	enter_cmdline_mode(MENU_SEARCH_FORWARD_SUBMODE, L"", menu);
}

static void
cmd_colon(struct key_info key_info, struct keys_info *keys_info)
{
	cmds_conf.begin = 1;
	cmds_conf.current = menu->pos;
	cmds_conf.end = menu->len;
	enter_cmdline_mode(MENU_CMD_SUBMODE, L"", menu);
}

static void
cmd_question(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 1;
	menu->match_dir = NONE;
	free(menu->regexp);
	enter_cmdline_mode(MENU_SEARCH_BACKWARD_SUBMODE, L"", menu);
}

static void
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = menu->len;

	clean_menu_position(menu);
	moveto_menu_pos(key_info.count - 1, menu);
	wrefresh(menu_win);
}

static void
cmd_H(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	moveto_menu_pos(menu->top, menu);
	wrefresh(menu_win);
}

static void
cmd_L(struct key_info key_info, struct keys_info *keys_info)
{
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

	clean_menu_position(menu);
	moveto_menu_pos(menu->top + menu->win_rows - 3, menu);
	wrefresh(menu_win);
}

static void
cmd_M(struct key_info key_info, struct keys_info *keys_info)
{
	int new_pos;
	if(menu->len < menu->win_rows)
		new_pos = menu->len/2;
	else
		new_pos = menu->top + (menu->win_rows - 3)/2;

	clean_menu_position(menu);
	moveto_menu_pos(new_pos, menu);
	wrefresh(menu_win);
}

static void
cmd_N(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->regexp != NULL)
	{
		menu->match_dir = last_search_backward ? DOWN : UP;
		search_menu_list(NULL, menu);
		wrefresh(menu_win);
	}
	else
	{
		status_bar_message("No search pattern set.");
		wrefresh(status_bar);
	}
}

static void
cmd_dd(struct key_info key_info, struct keys_info *keys_info)
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
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	moveto_menu_pos(key_info.count - 1, menu);
	wrefresh(menu_win);
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->pos == menu->len - 1)
		return;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos += key_info.count;
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->pos == 0)
		return;
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos -= key_info.count;
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_n(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->regexp != NULL)
	{
		menu->match_dir = last_search_backward ? UP : DOWN;
		search_menu_list(NULL, menu);
		wrefresh(menu_win);
	}
	else
	{
		status_bar_message("No search pattern set.");
		wrefresh(status_bar);
	}
}

static void
cmd_zb(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->len <= menu->win_rows - 3)
		return;

	if(menu->pos < menu->win_rows)
		menu->top = 0;
	else
		menu->top = menu->pos - (menu->win_rows - 3);
	update_menu();
}

static void
cmd_zt(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->len <= menu->win_rows - 3)
		return;

	if(menu->len - menu->pos >= menu->win_rows - 3 + 1)
		menu->top = menu->pos;
	else
		menu->top = menu->len - (menu->win_rows - 3 + 1);
	update_menu();
}

static void
cmd_zz(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->len <= menu->win_rows - 3)
		return;

	if(menu->pos <= (menu->win_rows - 3)/2)
		menu->top = 0;
	else if(menu->pos > menu->len - (menu->win_rows - 3 + 1)/2)
		menu->top = menu->len - (menu->win_rows - 3 + 1);
	else
		menu->top = menu->pos - (menu->win_rows - 3 + 1)/2;

	update_menu();
}

void
update_menu(void)
{
	draw_menu(menu);
	moveto_menu_pos(menu->pos, menu);
	wrefresh(menu_win);
}

static int
goto_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->end == NOT_DEF)
		return 0;
	clean_menu_position(menu);
	moveto_menu_pos(cmd_info->end, menu);
	wrefresh(menu_win);
	return 0;
}

static int
quit_cmd(const struct cmd_info *cmd_info)
{
	leave_menu_mode();
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
