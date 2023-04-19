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
#include "../compat/curses.h"
#include "../compat/reallocarray.h"
#include "../engine/cmds.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../lua/vlua.h"
#include "../menus/menus.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../cmd_completion.h"
#include "../flist_sel.h"
#include "../status.h"
#include "cmdline.h"
#include "modes.h"
#include "wk.h"

static const int SCROLL_GAP = 2;

static int complete_line_in_menu(const char cmd_line[], void *extra_arg);
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
static int can_scroll_menu_up(const menu_data_t *menu);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static int can_scroll_menu_down(const menu_data_t *menu);
static void change_menu_top(menu_data_t *menu, int delta);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void update_ui_on_leaving(void);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static int get_effective_menu_scroll_offset(const menu_data_t *menu);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
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
static void handle_mouse_event(key_info_t key_info, keys_info_t *keys_info);
static int all_lines_visible(const menu_data_t *menu);

static int goto_cmd(const cmd_info_t *cmd_info);
static int nohlsearch_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);
static int write_cmd(const cmd_info_t *cmd_info);
static void leave_menu_mode(int reset_selection);

static view_t *view;
static menu_data_t *menu;
static int last_search_backward;
static int was_redraw;
static int saved_top, saved_pos;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_b,     {{&cmd_ctrl_b},  .descr = "scroll page up"}},
	{WK_C_c,     {{&cmd_ctrl_c},  .descr = "leave menu mode"}},
	{WK_C_d,     {{&cmd_ctrl_d},  .descr = "scroll half-page down"}},
	{WK_C_e,     {{&cmd_ctrl_e},  .descr = "scroll one line down"}},
	{WK_C_f,     {{&cmd_ctrl_f},  .descr = "scroll page down"}},
	{WK_C_l,     {{&cmd_ctrl_l},  .descr = "redraw"}},
	{WK_CR,      {{&cmd_return},  .descr = "pick current item"}},
	{WK_C_n,     {{&cmd_j},       .descr = "go to item below"}},
	{WK_C_p,     {{&cmd_k},       .descr = "go to item above"}},
	{WK_C_u,     {{&cmd_ctrl_u},  .descr = "scroll half-page up"}},
	{WK_C_y,     {{&cmd_ctrl_y},  .descr = "scroll one line up"}},
	{WK_ESC,     {{&cmd_ctrl_c},  .descr = "leave menu mode"}},
	{WK_SLASH,   {{&cmd_slash},   .descr = "search forward"}},
	{WK_PERCENT, {{&cmd_percent}, .descr = "go to [count]% position"}},
	{WK_COLON,   {{&cmd_colon},   .descr = "go to cmdline mode"}},
	{WK_QM,      {{&cmd_qmark},   .descr = "search backward"}},
	{WK_B,       {{&cmd_B},       .descr = "make unsorted custom view"}},
	{WK_G,       {{&cmd_G},       .descr = "go to the last item"}},
	{WK_H,       {{&cmd_H},       .descr = "go to top of viewport"}},
	{WK_L,       {{&cmd_L},       .descr = "go to bottom of viewport"}},
	{WK_M,       {{&cmd_M},       .descr = "go to middle of viewport"}},
	{WK_N,       {{&cmd_N},       .descr = "go to previous search match"}},
	{WK_Z WK_Z,  {{&cmd_ctrl_c},  .descr = "leave menu mode"}},
	{WK_Z WK_Q,  {{&cmd_ctrl_c},  .descr = "leave menu mode"}},
	{WK_b,       {{&cmd_b},       .descr = "make custom view"}},
	{WK_d WK_d,  {{&cmd_dd},      .descr = "remove files"}},
	{WK_g WK_f,  {{&cmd_gf},      .descr = "navigate to file location"}},
	{WK_g WK_g,  {{&cmd_gg},      .descr = "go to the first item"}},
	{WK_j,       {{&cmd_j},       .descr = "go to item below"}},
	{WK_k,       {{&cmd_k},       .descr = "go to item above"}},
	{WK_l,       {{&cmd_return},  .descr = "pick current item"}},
	{WK_n,       {{&cmd_n},       .descr = "go to next search match"}},
	{WK_q,       {{&cmd_ctrl_c},  .descr = "leave menu mode"}},
	{WK_v,       {{&cmd_v},       .descr = "use items as Vim quickfix list"}},
	{WK_z WK_b,  {{&cmd_zb},      .descr = "push cursor to the bottom"}},
	{WK_z WK_H,  {{&cmd_zH},      .descr = "scroll page left"}},
	{WK_z WK_L,  {{&cmd_zL},      .descr = "scroll page right"}},
	{WK_z WK_h,  {{&cmd_zh},      .descr = "scroll one column left"}},
	{WK_z WK_l,  {{&cmd_zl},      .descr = "scroll one column right"}},
	{WK_z WK_t,  {{&cmd_zt},      .descr = "push cursor to the top"}},
	{WK_z WK_z,  {{&cmd_zz},      .descr = "center cursor position"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_PPAGE)},       {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{{K(KEY_NPAGE)},       {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{{K(KEY_UP)},          {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_DOWN)},        {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_RIGHT)},       {{&cmd_return}, .descr = "pick current item"}},
	{{K(KEY_HOME)},        {{&cmd_gg},     .descr = "go to the first item"}},
	{{K(KEY_END)},         {{&cmd_G},      .descr = "go to the last item"}},
	{{WC_z, K(KEY_LEFT)},  {{&cmd_zh},     .descr = "scroll one column left"}},
	{{WC_z, K(KEY_RIGHT)}, {{&cmd_zl},     .descr = "scroll one column right"}},
#endif /* ENABLE_EXTENDED_KEYS */
	{{K(KEY_MOUSE)}, {{&handle_mouse_event}, FOLLOWED_BY_NONE}},
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
	.complete_line = &complete_line_in_menu,
	.complete_args = &complete_args,
	.swap_range = &swap_range,
	.resolve_mark = &resolve_mark,
	.expand_macros = &menu_expand_macros,
	.expand_envvars = &menu_expand_envvars,
	.post = &post,
	.select_range = &menu_select_range,
	.skip_at_beginning = &skip_at_beginning,
};

/* Completes whole command-line.  Returns completion offset. */
static int
complete_line_in_menu(const char cmd_line[], void *extra_arg)
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
modmenu_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), MENU_MODE);
	assert(ret_code == 0);

	(void)ret_code;

	vle_keys_set_def_handler(MENU_MODE, &key_handler);

	/* Double initialization can happen in tests. */
	if(cmds_conf.inner == NULL)
	{
		vle_cmds_init(0, &cmds_conf);
		vle_cmds_add(commands, ARRAY_LEN(commands));
	}
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
modmenu_enter(menu_data_t *m, view_t *active_view)
{
	if(curr_stats.load_stage >= 0 && curr_stats.load_stage < 2)
	{
		return;
	}

	assert(m->len > 0 && "Menu cannot be empty.");

	ui_hide_graphics();
	werase(status_bar);

	view = active_view;
	menu = m;
	vle_mode_set(MENU_MODE, VMT_PRIMARY);
	stats_refresh_later();
	was_redraw = 0;

	vle_cmds_init(0, &cmds_conf);
}

void
modmenu_abort(void)
{
	leave_menu_mode(1);
}

void
modmenu_reenter(menu_data_t *m)
{
	assert(vle_mode_is(MENU_MODE) && "Can't reenter if not in menu mode.");
	assert(m->len > 0 && "Menu cannot be empty.");

	menus_replace_data(m);
	menus_full_redraw(m->state);
	menu = m;
}

void
modmenu_pre(void)
{
	touchwin(ruler_win);
	ui_refresh_win(ruler_win);
}

void
modmenu_post(void)
{
	if(stats_update_fetch() != UT_NONE)
	{
		modmenu_full_redraw();
	}
	ui_sb_msg(curr_stats.save_msg ? NULL : "");
}

void
modmenu_full_redraw(void)
{
	was_redraw = 1;
	menus_full_redraw(menu->state);
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_up(menu))
	{
		const int s = get_effective_menu_scroll_offset(menu);
		const int off = (getmaxy(menu_win) - 2) - SCROLL_GAP;
		menu->pos = modmenu_last_line(menu) - off;
		change_menu_top(menu, -off);
		if(cfg.scroll_off > 0 &&
				menu->top + (getmaxy(menu_win) - 3) - menu->pos < s)
		{
			menu->pos -= s - (menu->top + (getmaxy(menu_win) - 3) - menu->pos);
		}

		modmenu_partial_redraw();
	}
}

/* Returns non-zero if menu can be scrolled up. */
static int
can_scroll_menu_up(const menu_data_t *menu)
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
	menus_erase_current(menu->state);
	menu->top += DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);
	menu->pos += DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);
	if(cfg.scroll_off > 0 && menu->pos - menu->top < s)
	{
		menu->pos += s - (menu->pos - menu->top);
	}

	modmenu_partial_redraw();
}

static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		int off = MAX(cfg.scroll_off, 0);
		if(menu->pos <= menu->top + off)
		{
			menu->pos = menu->top + 1 + off;
		}

		++menu->top;
		modmenu_partial_redraw();
	}
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		const int s = get_effective_menu_scroll_offset(menu);
		const int off = (getmaxy(menu_win) - 2) - SCROLL_GAP;
		menu->pos = menu->top + off;
		change_menu_top(menu, off);
		if(cfg.scroll_off > 0 && menu->pos - menu->top < s)
		{
			menu->pos += s - (menu->pos - menu->top);
		}

		modmenu_partial_redraw();
	}
}

/* Returns non-zero if menu can be scrolled down. */
static int
can_scroll_menu_down(const menu_data_t *menu)
{
	return modmenu_last_line(menu) < menu->len - 1;
}

/* Moves top line of the menu ensuring that its value is correct. */
static void
change_menu_top(menu_data_t *menu, int delta)
{
	menu->top =
		MAX(MIN(menu->top + delta, menu->len - (getmaxy(menu_win) - 2)), 0);
}

int
modmenu_last_line(const menu_data_t *menu)
{
	return menu->top + (getmaxy(menu_win) - 2) - 1;
}

/* Redraw TUI. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	modmenu_full_redraw();
}

static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	static menu_data_t *saved_menu;

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	saved_menu = menu;
	if(menu->execute_handler != NULL && menu->execute_handler(view, menu))
	{
		vle_mode_set(MENU_MODE, VMT_PRIMARY);
		modmenu_full_redraw();
		return;
	}

	if(!vle_mode_is(MENU_MODE))
	{
		menus_reset_data(saved_menu);
	}
	else if(menu != saved_menu)
	{
		menus_reset_data(saved_menu);
		modmenu_partial_redraw();
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
		ui_view_title_update(curr_view);
		update_all_windows();
	}
}

static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	const int s = get_effective_menu_scroll_offset(menu);
	menus_erase_current(menu->state);

	if(cfg.scroll_off > 0 && menu->top + getmaxy(menu_win) - menu->pos < s)
	{
		menu->pos -= s - (menu->top + (getmaxy(menu_win) - 3) - menu->pos);
	}

	menu->top -= DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);
	if(menu->top < 0)
	{
		menu->top = 0;
	}
	menu->pos -= DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);

	modmenu_partial_redraw();
}

/* Returns scroll offset value for the menu taking menu height into account. */
static int
get_effective_menu_scroll_offset(const menu_data_t *menu)
{
	return MIN(DIV_ROUND_UP(getmaxy(menu_win) - 3, 2) - 1, cfg.scroll_off);
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_up(menu))
	{
		int off = MAX(cfg.scroll_off, 0);
		if(menu->pos >= menu->top + getmaxy(menu_win) - 3 - off)
		{
			menu->pos = menu->top - 1 + getmaxy(menu_win) - 3 - off;
		}

		--menu->top;
		modmenu_partial_redraw();
	}
}

static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	last_search_backward = 0;
	menus_search_reset(menu->state, last_search_backward,
			def_count(key_info.count));
	modcline_in_menu(CLS_MENU_FSEARCH, menu);
}

/* Jump to percent of list. */
static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
{
	int line;
	if(key_info.count == NO_COUNT_GIVEN || key_info.count > 100)
	{
		return;
	}
	line = (key_info.count*menu->len)/100;
	menus_set_pos(menu->state, line);
	ui_refresh_win(menu_win);
}

static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	cmds_conf.begin = 1;
	cmds_conf.current = menu->pos;
	cmds_conf.end = menu->len;
	modcline_in_menu(CLS_MENU_COMMAND, menu);
}

static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
	last_search_backward = 1;
	menus_search_reset(menu->state, last_search_backward,
			def_count(key_info.count));
	modcline_in_menu(CLS_MENU_BSEARCH, menu);
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
	{
		key_info.count = menu->len;
	}

	menus_erase_current(menu->state);
	menus_set_pos(menu->state, key_info.count - 1);
	ui_refresh_win(menu_win);
}

static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	int top;
	int off = MAX(cfg.scroll_off, 0);
	if(off > getmaxy(menu_win)/2)
	{
		return;
	}

	top = (menu->top == 0) ? 0 : (menu->top + off);

	menus_erase_current(menu->state);
	menus_set_pos(menu->state, top);
	ui_refresh_win(menu_win);
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
	if(off > getmaxy(menu_win)/2)
	{
		return;
	}

	top = menu->top + getmaxy(menu_win);
	if(menu->top + getmaxy(menu_win) < menu->len - 1)
	{
		top -= off;
	}

	menus_erase_current(menu->state);
	menus_set_pos(menu->state, top - 3);
	ui_refresh_win(menu_win);
}

/* Moves cursor to the middle of the window. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	int new_pos;
	if(menu->len < getmaxy(menu_win))
	{
		new_pos = DIV_ROUND_UP(menu->len, 2);
	}
	else
	{
		new_pos = menu->top + DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);
	}

	menus_erase_current(menu->state);
	menus_set_pos(menu->state, MAX(0, new_pos - 1));
	ui_refresh_win(menu_win);
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	key_info.count = def_count(key_info.count);
	while(key_info.count-- > 0)
	{
		menus_search_repeat(menu->state, !last_search_backward);
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
	if(menus_to_custom_view(menu->state, view, very) != 0)
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

	handler_response = menu->key_handler(view, menu, keys);

	switch(handler_response)
	{
		case KHR_REFRESH_WINDOW:
			ui_refresh_win(menu_win);
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
	menus_erase_current(menu->state);
	menus_set_pos(menu->state, def_count(key_info.count) - 1);
	ui_refresh_win(menu_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->pos != menu->len - 1)
	{
		menus_erase_current(menu->state);
		menu->pos += def_count(key_info.count);
		menus_set_pos(menu->state, menu->pos);
		ui_refresh_win(menu_win);
	}
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->pos != 0)
	{
		menus_erase_current(menu->state);
		menu->pos -= def_count(key_info.count);
		menus_set_pos(menu->state, menu->pos);
		ui_refresh_win(menu_win);
	}
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	key_info.count = def_count(key_info.count);
	while(key_info.count-- > 0)
	{
		menus_search_repeat(menu->state, last_search_backward);
	}
}

/* Handles current content of the menu to Vim as quickfix list. */
static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	FILE *vim_stdin;
	char *cmd;
	int i;
	int qf = 1;

	int bg;
	const char *vi_cmd = cfg_get_vicmd(&bg);

	/* If both first and last lines do not contain colons, treat lines as list of
	 * file names. */
	if(strchr(menu->items[0], ':') == NULL &&
			strchr(menu->items[menu->len - 1], ':') == NULL)
	{
		qf = 0;
	}

	if(vlua_handler_cmd(curr_stats.vlua, vi_cmd))
	{
		if(vlua_edit_list(curr_stats.vlua, vi_cmd, menu->items, menu->len,
					menu->pos, qf) != 0)
		{
			show_error_msg("List View", "Failed to view list via handler");
		}
		return;
	}

	ui_shutdown();
	stats_refresh_later();

	if(!qf)
	{
		ShellType shell_type = (get_env_type() == ET_UNIX ? ST_POSIX : ST_CMD);
		char *const arg = shell_arg_escape("+exe 'bd!|args' "
				"join(map(getline('1','$'),'fnameescape(v:val)'))",
				shell_type);
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
		if(menu->pos < getmaxy(menu_win))
		{
			menu->top = 0;
		}
		else
		{
			menu->top = menu->pos - (getmaxy(menu_win) - 3);
		}
		modmenu_partial_redraw();
	}
}

static void
cmd_zH(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->hor_pos != 0)
	{
		menu->hor_pos = MAX(0, menu->hor_pos - (getmaxx(menu_win) - 4));
		modmenu_partial_redraw();
	}
}

static void
cmd_zL(key_info_t key_info, keys_info_t *keys_info)
{
	menu->hor_pos += getmaxx(menu_win) - 4;
	modmenu_partial_redraw();
}

static void
cmd_zh(key_info_t key_info, keys_info_t *keys_info)
{
	if(menu->hor_pos != 0)
	{
		menu->hor_pos = MAX(0, menu->hor_pos - def_count(key_info.count));
		modmenu_partial_redraw();
	}
}

static void
cmd_zl(key_info_t key_info, keys_info_t *keys_info)
{
	menu->hor_pos += def_count(key_info.count);
	modmenu_partial_redraw();
}

static void
cmd_zt(key_info_t key_info, keys_info_t *keys_info)
{
	if(can_scroll_menu_down(menu))
	{
		if(menu->len - menu->pos >= getmaxy(menu_win) - 3 + 1)
			menu->top = menu->pos;
		else
			menu->top = menu->len - (getmaxy(menu_win) - 3 + 1);
		modmenu_partial_redraw();
	}
}

static void
cmd_zz(key_info_t key_info, keys_info_t *keys_info)
{
	if(!all_lines_visible(menu))
	{
		if(menu->pos <= (getmaxy(menu_win) - 3)/2)
			menu->top = 0;
		else if(menu->pos > menu->len - DIV_ROUND_UP(getmaxy(menu_win) - 3, 2))
			menu->top = menu->len - (getmaxy(menu_win) - 3 + 1);
		else
			menu->top = menu->pos - DIV_ROUND_UP(getmaxy(menu_win) - 3, 2);

		modmenu_partial_redraw();
	}
}

/* Returns non-zero if all menu lines are visible, so no scrolling is needed. */
static int
all_lines_visible(const menu_data_t *menu)
{
	return menu->len <= getmaxy(menu_win) - 2;
}

void
modmenu_partial_redraw(void)
{
	menus_partial_redraw(menu->state);
	menus_set_pos(menu->state, menu->pos);
	ui_refresh_win(menu_win);
}

static int
goto_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->end != NOT_DEF)
	{
		menus_erase_current(menu->state);
		menus_set_pos(menu->state, cmd_info->end);
		ui_refresh_win(menu_win);
	}
	return 0;
}

/* Disables highlight of search result matches. */
static int
nohlsearch_cmd(const cmd_info_t *cmd_info)
{
	menus_search_reset_hilight(menu->state);
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
modmenu_save_pos(void)
{
	saved_top = menu->top;
	saved_pos = menu->pos;
}

void
modmenu_restore_pos(void)
{
	menu->top = saved_top;
	menu->pos = saved_pos;
}

void
modmenu_morph_into_cline(CmdLineSubmode submode, const char input[],
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
	modcline_enter(submode, input_copy);

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

	menus_reset_data(menu);

	if(reset_selection)
	{
		flist_sel_stash(view);
		redraw_view(view);
	}

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	update_ui_on_leaving();
}

void
modmenu_run_command(const char cmd[])
{
	if(cmds_dispatch1(cmd, view, CIT_COMMAND) < 0)
	{
		ui_sb_err("An error occurred while trying to execute command");
	}
	vle_cmds_init(0, &cmds_conf);
}

/* Provides access to tests to a local static variable. */
TSTATIC menu_data_t *
menu_get_current(void)
{
	return menu;
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

	if(!wenclose(menu_win, e.y, e.x))
	{
		return;
	}

	if(e.bstate & BUTTON1_PRESSED)
	{
		wmouse_trafo(menu_win, &e.y, &e.x, FALSE);

		int old_pos = menu->pos;

		menus_erase_current(menu->state);
		menus_set_pos(menu->state, menu->top + e.y - 1);
		ui_refresh_win(menu_win);

		if(menu->pos == old_pos)
		{
			cmd_return(key_info, keys_info);
		}
	}
	else if(e.bstate & BUTTON4_PRESSED)
	{
		cmd_ctrl_y(key_info, keys_info);
	}
	else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
	{
		cmd_ctrl_e(key_info, keys_info);
	}
}
/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
