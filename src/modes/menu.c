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

#include <curses.h>

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL wchar_t */
#include <stdio.h> /* pclose() popen() */
#include <stdlib.h> /* free() */
#include <string.h> /* strerror() */

#include "../cfg/config.h"
#include "../compat/reallocarray.h"
#include "../engine/cmds.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../menus/menus.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../cmd_completion.h"
#include "../filelist.h"
#include "../status.h"
#include "cmdline.h"
#include "modes.h"
#include "wk.h"

static const int SCROLL_GAP = 2;

static int swap_range(void);
static int resolve_mark(char mark);
static char * menu_expand_macros(const char str[], int for_shell, int *usr1,
		int *usr2);
static char * menu_expand_envvars(const char *str);
static void post(int id);
static void menu_select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

static int key_handler(wchar_t key);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static int can_scroll_menu_up(const menu_info *menu);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static int can_scroll_menu_down(const menu_info *menu);
static void change_menu_top(menu_info *const menu, int delta);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void update_ui_on_leaving(void);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static int get_effective_menu_scroll_offset(const menu_info *menu);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_qmark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_B(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_H(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_b(key_info_t key_info, keys_info_t *keys_info);
static void dump_into_custom_view(int very);
static void cmd_dd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gf(key_info_t key_info, keys_info_t *keys_info);
static int pass_combination_to_khandler(const wchar_t keys[]);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void cmd_v(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zL(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zh(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zt(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zz(key_info_t key_info, keys_info_t *keys_info);
static int all_lines_visible(const menu_info *const menu);
static int goto_cmd(const cmd_info_t *cmd_info);
static int nohlsearch_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);
static int write_cmd(const cmd_info_t *cmd_info);
static void leave_menu_mode(int reset_selection);

static FileView *view;
static menu_info *menu;
static int last_search_backward;
static int was_redraw;
static int saved_top, saved_pos;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_b,    {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{WK_C_c,    {{&cmd_ctrl_c}, .descr = "leave menu mode"}},
	{WK_C_d,    {{&cmd_ctrl_d}, .descr = "scroll half-page down"}},
	{WK_C_e,    {{&cmd_ctrl_e}, .descr = "scroll one line down"}},
	{WK_C_f,    {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{WK_C_l,    {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_CR,     {{&cmd_return}, .descr = "pick current item"}},
	{WK_C_n,    {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_p,    {{&cmd_k},      .descr = "go to item above"}},
	{WK_C_u,    {{&cmd_ctrl_u}, .descr = "scroll half-page up"}},
	{WK_C_y,    {{&cmd_ctrl_y}, .descr = "scroll one line up"}},
	{WK_ESC,    {{&cmd_ctrl_c}, .descr = "leave menu mode"}},
	{WK_SLASH,  {{&cmd_slash},  .descr = "search forward"}},
	{WK_COLON,  {{&cmd_colon},  .descr = "go to cmdline mode"}},
	{WK_QM,     {{&cmd_qmark},  .descr = "search backward"}},
	{WK_B,      {{&cmd_B},      .descr = "make unsorted custom view"}},
	{WK_G,      {{&cmd_G},      .descr = "go to the last item"}},
	{WK_H,      {{&cmd_H},      .descr = "go to top of viewport"}},
	{WK_L,      {{&cmd_L},      .descr = "go to bottom of viewport"}},
	{WK_M,      {{&cmd_M},      .descr = "go to middle of viewport"}},
	{WK_N,      {{&cmd_N},      .descr = "go to previous search match"}},
	{WK_Z WK_Z, {{&cmd_ctrl_c}, .descr = "leave menu mode"}},
	{WK_Z WK_Q, {{&cmd_ctrl_c}, .descr = "leave menu mode"}},
	{WK_b,      {{&cmd_b},      .descr = "make custom view"}},
	{WK_d WK_d, {{&cmd_dd},     .descr = "remove files"}},
	{WK_g WK_f, {{&cmd_gf},     .descr = "navigate to file location"}},
	{WK_g WK_g, {{&cmd_gg},     .descr = "go to the first item"}},
	{WK_j,      {{&cmd_j},      .descr = "go to item below"}},
	{WK_k,      {{&cmd_k},      .descr = "go to item above"}},
	{WK_l,      {{&cmd_return}, .descr = "pick current item"}},
	{WK_n,      {{&cmd_n},      .descr = "go to next search match"}},
	{WK_q,      {{&cmd_ctrl_c}, .descr = "leave menu mode"}},
	{WK_v,      {{&cmd_v},      .descr = "use items as Vim quickfix list"}},
	{WK_z WK_b, {{&cmd_zb},     .descr = "push cursor to the bottom"}},
	{WK_z WK_H, {{&cmd_zH},     .descr = "scroll page left"}},
	{WK_z WK_L, {{&cmd_zL},     .descr = "scroll page right"}},
	{WK_z WK_h, {{&cmd_zh},     .descr = "scroll one column left"}},
	{WK_z WK_l, {{&cmd_zl},     .descr = "scroll one column right"}},
	{WK_z WK_t, {{&cmd_zt},     .descr = "push cursor to the top"}},
	{WK_z WK_z, {{&cmd_zz},     .descr = "center cursor position"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_PPAGE},       {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{{KEY_NPAGE},       {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{{KEY_UP},          {{&cmd_k},      .descr = "go to item above"}},
	{{KEY_DOWN},        {{&cmd_j},      .descr = "go to item below"}},
	{{KEY_RIGHT},       {{&cmd_return}, .descr = "pick current item"}},
	{{KEY_HOME},        {{&cmd_gg},     .descr = "go to the first item"}},
	{{KEY_END},         {{&cmd_G},      .descr = "go to the last item"}},
	{{WC_z, KEY_LEFT},  {{&cmd_zh},     .descr = "scroll one column left"}},
	{{WC_z, KEY_RIGHT}, {{&cmd_zl},     .descr = "scroll one column right"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

/* Specification of builtin commands. */
static const cmd_add_t commands[] = {
	{ .name = "",                  .abbr = NULL,    .id = -1,
	  .descr = "navigate to specific line",
	  .flags = HAS_RANGE,
	  .handler = &goto_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "exit",              .abbr = "exi",   .id = -1,
	  .descr = "exit the application",
	  .flags = 0,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "nohlsearch",        .abbr = "noh",   .id = -1,
	  .descr = "reset highlighting of search matches",
	  .flags = 0,
	  .handler = &nohlsearch_cmd,  .min_args = 0,   .max_args = 0, },
	{ .name = "quit",              .abbr = "q",     .id = -1,
	  .descr = "exit the application",
	  .flags = 0,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "write",             .abbr = "w",     .id = COM_MENU_WRITE,
	  .descr = "write all menu lines into a file",
	  .flags = HAS_QUOTED_ARGS,
	  .handler = &write_cmd,       .min_args = 1,   .max_args = 1, },
	{ .name = "xit",               .abbr = "x",     .id = -1,
	  .descr = "exit the application",
	  .flags = 0,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
};

/* Settings for the cmds unit. */
static cmds_conf_t cmds_conf = {
	.complete_args = &complete_args,
	.swap_range = &swap_range,
	.resolve_mark = &resolve_mark,
	.expand_macros = &menu_expand_macros,
	.expand_envvars = &menu_expand_envvars,
	.post = &post,
	.select_range = &menu_select_range,
	.skip_at_beginning = &skip_at_beginning,
};

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

/* Implementation of macros expansion callback for cmds unit.  Returns newly
 * allocated memory. */
static char *
menu_expand_macros(const char str[], int for_shell, int *usr1, int *usr2)
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
init_menu_mode(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), MENU_MODE);
	assert(ret_code == 0);

	(void)ret_code;

	vle_keys_set_def_handler(MENU_MODE, key_handler);

	init_cmds(0, &cmds_conf);
	add_builtin_commands((const cmd_add_t *)&commands, ARRAY_LEN(commands));
}

static int
key_handler(wchar_t key)
{
	const wchar_t shortcut[] = {key, L'\0'};

	if(pass_combination_to_khandler(shortcut) && menu->len == 0)
	{
		show_error_msg("No more items in the menu", "Menu will be closed");
		leave_menu_mode(1);
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
	vle_mode_set(MENU_MODE, VMT_PRIMARY);
	curr_stats.need_update = UT_FULL;
	was_redraw = 0;

	init_cmds(0, &cmds_conf);
}

void
menu_pre(void)
{
	touchwin(ruler_win);
	wrefresh(ruler_win);
}

void
menu_post(void)
{
	if(curr_stats.need_update != UT_NONE)
	{
		menu_redraw();
		curr_stats.need_update = UT_NONE;
	}
	status_bar_message(curr_stats.save_msg ? NULL : "");
}

void
menu_redraw(void)
{
	was_redraw = 1;
	redraw_menu(menu);
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
	leave_menu_mode(1);
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

/* Redraw TUI. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	menu_redraw();
}

static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	static menu_info *saved_menu;

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	saved_menu = menu;
	if(menu->execute_handler != NULL && menu->execute_handler(curr_view, menu))
	{
		vle_mode_set(MENU_MODE, VMT_PRIMARY);
		menu_redraw();
		return;
	}

	if(!vle_mode_is(MENU_MODE))
	{
		reset_popup_menu(saved_menu);
	}
	else if(menu != saved_menu)
	{
		reset_popup_menu(saved_menu);
		update_menu();
	}

	update_ui_on_leaving();
}

/* Updates UI on leaving the mode trying to minimize efforts to do this. */
static void
update_ui_on_leaving(void)
{
	if(was_redraw)
	{
		update_screen(UT_FULL);
	}
	else
	{
		update_all_windows();
	}
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
	menu->search_repeat = def_count(key_info.count);
	last_search_backward = 0;
	menu->backward_search = 0;
	free(menu->regexp);
	menu->regexp = NULL;
	enter_cmdline_mode(CLS_MENU_FSEARCH, "", menu);
}

static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	cmds_conf.begin = 1;
	cmds_conf.current = menu->pos;
	cmds_conf.end = menu->len;
	enter_cmdline_mode(CLS_MENU_COMMAND, "", menu);
}

static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
	menu->search_repeat = def_count(key_info.count);
	last_search_backward = 1;
	menu->backward_search = 1;
	free(menu->regexp);
	enter_cmdline_mode(CLS_MENU_BSEARCH, "", menu);
}

/* Populates very custom (unsorted) view with list of files. */
static void
cmd_B(key_info_t key_info, keys_info_t *keys_info)
{
	dump_into_custom_view(1);
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
		if(pass_combination_to_khandler(L"L"))
		{
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

/* Moves cursor to the middle of the window. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	int new_pos;
	if(menu->len < menu->win_rows)
		new_pos = DIV_ROUND_UP(menu->len, 2);
	else
		new_pos = menu->top + DIV_ROUND_UP(menu->win_rows - 3, 2);

	clean_menu_position(menu);
	move_to_menu_pos(MAX(0, new_pos - 1), menu);
	wrefresh(menu_win);
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	key_info.count = def_count(key_info.count);
	while(key_info.count-- > 0)
	{
		menus_search(menu, !last_search_backward);
	}
}

/* Populates custom view with list of files. */
static void
cmd_b(key_info_t key_info, keys_info_t *keys_info)
{
	dump_into_custom_view(0);
}

/* Makes custom view of specified type out of menu items. */
static void
dump_into_custom_view(int very)
{
	if(menu_to_custom_view(menu, view, very) != 0)
	{
		show_error_msg("Menu transformation",
				"No valid paths discovered in menu content");
		return;
	}

	leave_menu_mode(1);
}

static void
cmd_dd(key_info_t key_info, keys_info_t *keys_info)
{
	if(pass_combination_to_khandler(L"dd") && menu->len == 0)
	{
		show_error_msg("Menu is closing", "No more items in the menu");
		leave_menu_mode(1);
	}
}

/* Passes "gf" shortcut to menu as otherwise the shortcut is not available. */
static void
cmd_gf(key_info_t key_info, keys_info_t *keys_info)
{
	(void)pass_combination_to_khandler(L"gf");
}

/* Gives menu-specific keyboard routine to process the shortcut.  Returns zero
 * if the shortcut wasn't processed, otherwise non-zero is returned. */
static int
pass_combination_to_khandler(const wchar_t keys[])
{
	KHandlerResponse handler_response;

	if(menu->key_handler == NULL)
	{
		return 0;
	}

	handler_response = menu->key_handler(menu, keys);

	switch(handler_response)
	{
		case KHR_REFRESH_WINDOW:
			wrefresh(menu_win);
			return 1;
		case KHR_CLOSE_MENU:
			leave_menu_mode(1);
			return 1;
		case KHR_MORPHED_MENU:
			assert(!vle_mode_is(MENU_MODE) && "Wrong use of KHR_MORPHED_MENU.");
			return 1;
		case KHR_UNHANDLED:
			return 0;

		default:
			assert(0 && "Unknown menu-specific keyboard handler response.");
			return 0;
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
	key_info.count = def_count(key_info.count);
	while(key_info.count-- > 0)
	{
		menus_search(menu, last_search_backward);
	}
}

/* Handles current content of the menu to Vim as quickfix list. */
static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	int bg;
	const char *vi_cmd;
	FILE *vim_stdin;
	char *cmd;
	int i;
	int qf = 1;

	/* If both first and last lines do not contain colons, treat lines as list of
	 * file names. */
	if(strchr(menu->items[0], ':') == NULL &&
			strchr(menu->items[menu->len - 1], ':') == NULL)
	{
		qf = 0;
	}

	endwin();
	curr_stats.need_update = UT_FULL;

	vi_cmd = cfg_get_vicmd(&bg);
	if(!qf)
	{
		char *const arg = shell_like_escape("+exe 'bd!|args' "
				"join(map(getline('1','$'),'fnameescape(v:val)'))", 0);
		cmd = format_str("%s %s +argument%d -", vi_cmd, arg, menu->pos + 1);
		free(arg);
	}
	else if(menu->pos == 0)
	{
		/* For some reason +cc1 causes noisy messages on status line, so handle this
		 * case separately. */
		cmd = format_str("%s +cgetbuffer +bd! +cfirst -", vi_cmd);
	}
	else
	{
		cmd = format_str("%s +cgetbuffer +bd! +cfirst +cc%d -", vi_cmd,
				menu->pos + 1);
	}

	vim_stdin = popen(cmd, "w");
	free(cmd);

	if(vim_stdin == NULL)
	{
		recover_after_shellout();
		show_error_msg("Vim QuickFix", "Failed to send list of files to editor.");
		return;
	}

	for(i = 0; i < menu->len; ++i)
	{
		fputs(menu->items[i], vim_stdin);
		putc('\n', vim_stdin);
	}

	pclose(vim_stdin);
	recover_after_shellout();
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

/* Disables highlight of search result matches. */
static int
nohlsearch_cmd(const cmd_info_t *cmd_info)
{
	menus_reset_search_highlight(menu);
	return 0;
}

static int
quit_cmd(const cmd_info_t *cmd_info)
{
	leave_menu_mode(1);
	return 0;
}

/* Writes all menu lines into file specified as argument. */
static int
write_cmd(const cmd_info_t *cmd_info)
{
	char *const no_tilde = expand_tilde(cmd_info->argv[0]);
	if(write_file_of_lines(no_tilde, menu->items, menu->len) != 0)
	{
		show_error_msg("Failed to open output file", strerror(errno));
	}
	free(no_tilde);
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

void
menu_morph_into_cmdline(CmdLineSubmode submode, const char input[],
		int external)
{
	/* input might point to part of menu data. */
	char *input_copy;

	if(input[0] == '\0')
	{
		show_error_msg("Command insertion", "Ignoring empty command");
		return;
	}

	input_copy = external ? format_str("!%s", input) : strdup(input);
	if(input_copy == NULL)
	{
		show_error_msg("Error", "Not enough memory");
		return;
	}

	leave_menu_mode(0);
	enter_cmdline_mode(submode, input_copy, NULL);

	free(input_copy);
}

/* Leaves menu mode, possibly resetting selection.  Does nothing if current mode
 * isn't menu mode. */
static void
leave_menu_mode(int reset_selection)
{
	/* Some menu implementation could have switched mode from one of handlers. */
	if(!vle_mode_is(MENU_MODE))
	{
		return;
	}

	reset_popup_menu(menu);

	if(reset_selection)
	{
		clean_selected_files(view);
		redraw_view(view);
	}

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	update_ui_on_leaving();
}

void
execute_cmdline_command(const char cmd[])
{
	if(exec_command(cmd, curr_view, CIT_COMMAND) < 0)
	{
		status_bar_error("An error occurred while trying to execute command");
	}
	init_cmds(0, &cmds_conf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
