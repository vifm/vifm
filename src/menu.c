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

static void init_extended_keys(void);
static void leave_menu_mode(void);
static void cmd_ctrl_b(struct key_info, struct keys_info *);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_f(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_slash(struct key_info, struct keys_info *);
static void cmd_colon(struct key_info, struct keys_info *);
static void cmd_question(struct key_info, struct keys_info *);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_N(struct key_info, struct keys_info *);
static void cmd_dd(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void cmd_n(struct key_info, struct keys_info *);

void
init_menu_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_cmd(L"\x02", MENU_MODE);
	curr->data.handler = cmd_ctrl_b;

	curr = add_cmd(L"\x03", MENU_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"\x06", MENU_MODE);
	curr->data.handler = cmd_ctrl_f;

	/* return */
	curr = add_cmd(L"\x0d", MENU_MODE);
	curr->data.handler = cmd_ctrl_m;

	/* escape */
	curr = add_cmd(L"\x1b", MENU_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L"/", MENU_MODE);
	curr->data.handler = cmd_slash;

	curr = add_cmd(L":", MENU_MODE);
	curr->data.handler = cmd_colon;

	curr = add_cmd(L"?", MENU_MODE);
	curr->data.handler = cmd_question;

	curr = add_cmd(L"G", MENU_MODE);
	curr->data.handler = cmd_G;

	curr = add_cmd(L"N", MENU_MODE);
	curr->data.handler = cmd_N;

	curr = add_cmd(L"dd", MENU_MODE);
	curr->data.handler = cmd_dd;

	curr = add_cmd(L"gg", MENU_MODE);
	curr->data.handler = cmd_gg;

	curr = add_cmd(L"j", MENU_MODE);
	curr->data.handler = cmd_j;

	curr = add_cmd(L"k", MENU_MODE);
	curr->data.handler = cmd_k;

	curr = add_cmd(L"l", MENU_MODE);
	curr->data.handler = cmd_ctrl_m;

	curr = add_cmd(L"n", MENU_MODE);
	curr->data.handler = cmd_n;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_PPAGE;
	curr = add_cmd(buf, MENU_MODE);
	curr->data.handler = cmd_ctrl_b;

	buf[0] = KEY_NPAGE;
	curr = add_cmd(buf, MENU_MODE);
	curr->data.handler = cmd_ctrl_f;

	buf[0] = KEY_UP;
	curr = add_cmd(buf, MENU_MODE);
	curr->data.handler = cmd_k;

	buf[0] = KEY_DOWN;
	curr = add_cmd(buf, MENU_MODE);
	curr->data.handler = cmd_j;
#endif /* ENABLE_EXTENDED_KEYS */
}

void
enter_menu_mode(menu_info *m, FileView *active_view)
{
	werase(status_bar);

	view = active_view;
	menu = m;
	*mode = MENU_MODE;
	curr_stats.need_redraw = 1;
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
	redraw_menu(view, menu);
}

void
execute_menu_command(FileView *view, char *command, menu_info *m)
{
	if(strncmp("quit", command, strlen(command)) == 0)
		leave_menu_mode();
	else if(strcmp("x", command) == 0)
		leave_menu_mode();
	else if(isdigit(*command))
	{
		clean_menu_position(m);
		moveto_menu_pos(view, atoi(command) - 1, m);
		wrefresh(menu_win);
	}
}

static void
leave_menu_mode(void)
{
	reset_popup_menu(menu);
	*mode = NORMAL_MODE;
}

static void
cmd_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->pos -= menu->win_rows - 3;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_menu_mode();
}

static void
cmd_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->pos += menu->win_rows - 3;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	execute_menu_cb(curr_view, menu);
	leave_menu_mode();
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
	clean_menu_position(menu);
	moveto_menu_pos(curr_view, menu->len - 1, menu);
	wrefresh(menu_win);
}

static void
cmd_N(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->regexp != NULL)
	{
		menu->match_dir = last_search_backward ? DOWN : UP;
		menu->matching_entries = 0;
		search_menu_list(view, NULL, menu);
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
	if(menu->type == COMMAND)
	{
		clean_menu_position(menu);
		remove_command(command_list[menu->pos].name);

		reload_command_menu_list(menu);
		draw_menu(view, menu);

		if(menu->pos -1 >= 0)
			moveto_menu_pos(view, menu->pos -1, menu);
		else
			moveto_menu_pos(view, 0, menu);
	}
	else if(menu->type == BOOKMARK)
	{
		clean_menu_position(menu);
		remove_bookmark(active_bookmarks[menu->pos]);

		reload_bookmarks_menu_list(menu);
		draw_menu(view, menu);

		if(menu->pos -1 >= 0)
			moveto_menu_pos(view, menu->pos -1, menu);
		else
			moveto_menu_pos(view, 0, menu);
	}
	wrefresh(menu_win);
}

static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	moveto_menu_pos(curr_view, 0, menu);
	wrefresh(menu_win);
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos += key_info.count;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos -= key_info.count;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
cmd_n(struct key_info key_info, struct keys_info *keys_info)
{
	if(menu->regexp != NULL)
	{
		menu->match_dir = last_search_backward ? UP : DOWN;
		menu->matching_entries = 0;
		search_menu_list(view, NULL, menu);
		wrefresh(menu_win);
	}
	else
	{
		status_bar_message("No search pattern set.");
		wrefresh(status_bar);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
