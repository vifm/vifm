#include <curses.h>

#include <assert.h>
#include <string.h>

#include "bookmarks.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "keys_buildin_c.h"
#include "modes.h"
#include "status.h"
#include "ui.h"

#include "keys_buildin_m.h"

static int *mode;
static FileView *view;
static menu_info *menu;
static int last_search_backward;

static void init_extended_keys(void);
static void leave_menu_mode(int save_msg);
static void keys_ctrl_b(struct key_info, struct keys_info *);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_f(struct key_info, struct keys_info *);
static void keys_ctrl_m(struct key_info, struct keys_info *);
static void keys_slash(struct key_info, struct keys_info *);
static void keys_colon(struct key_info, struct keys_info *);
static void keys_question(struct key_info, struct keys_info *);
static void keys_G(struct key_info, struct keys_info *);
static void keys_N(struct key_info, struct keys_info *);
static void keys_dd(struct key_info, struct keys_info *);
static void keys_gg(struct key_info, struct keys_info *);
static void keys_j(struct key_info, struct keys_info *);
static void keys_k(struct key_info, struct keys_info *);
static void keys_n(struct key_info, struct keys_info *);

void
init_buildin_m_keys(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_keys(L"\x02", MENU_MODE);
	curr->data.handler = keys_ctrl_b;

	curr = add_keys(L"\x03", MENU_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L"\x06", MENU_MODE);
	curr->data.handler = keys_ctrl_f;

	/* return */
	curr = add_keys(L"\x0d", MENU_MODE);
	curr->data.handler = keys_ctrl_m;

	/* escape */
	curr = add_keys(L"\x1b", MENU_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L"/", MENU_MODE);
	curr->data.handler = keys_slash;

	curr = add_keys(L":", MENU_MODE);
	curr->data.handler = keys_colon;

	curr = add_keys(L"?", MENU_MODE);
	curr->data.handler = keys_question;

	curr = add_keys(L"G", MENU_MODE);
	curr->data.handler = keys_G;

	curr = add_keys(L"N", MENU_MODE);
	curr->data.handler = keys_N;

	curr = add_keys(L"dd", MENU_MODE);
	curr->data.handler = keys_dd;

	curr = add_keys(L"gg", MENU_MODE);
	curr->data.handler = keys_gg;

	curr = add_keys(L"j", MENU_MODE);
	curr->data.handler = keys_j;

	curr = add_keys(L"k", MENU_MODE);
	curr->data.handler = keys_k;

	curr = add_keys(L"l", MENU_MODE);
	curr->data.handler = keys_ctrl_m;

	curr = add_keys(L"n", MENU_MODE);
	curr->data.handler = keys_n;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_PPAGE;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_ctrl_b;

	buf[0] = KEY_NPAGE;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_ctrl_f;

	buf[0] = KEY_UP;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_k;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_j;
}

void
enter_menu_mode(menu_info *m, FileView *active_view)
{
	int y, len;
	getmaxyx(menu_win, y, len);
	keypad(menu_win, TRUE);
	werase(status_bar);
	wtimeout(menu_win, KEYPRESS_TIMEOUT);

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
execute_menu_command(FileView *view, char *command, menu_info *m)
{
	if(strncmp("quit", command, strlen(command)) == 0)
		leave_menu_mode(0);
	else if(strcmp("x", command) == 0)
		leave_menu_mode(0);
	else if(isdigit(*command))
	{
		clean_menu_position(m);
		moveto_menu_pos(view, atoi(command) - 1, m);
		wrefresh(menu_win);
	}
}

static void
leave_menu_mode(int save_msg)
{
	reset_popup_menu(menu);
	*mode = NORMAL_MODE;
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_menu_mode(0);
}

static void
keys_ctrl_b(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	moveto_menu_pos(view, menu->top - menu->win_rows + 2, menu);
	wrefresh(menu_win);
}

static void
keys_ctrl_f(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	menu->pos += menu->win_rows;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
keys_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	execute_menu_cb(curr_view, menu);
	leave_menu_mode(0);
}

static void
keys_slash(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 0;
	menu->match_dir = NONE;
	free(menu->regexp);
	enter_cmdline_mode(MENU_SEARCH_FORWARD_SUBMODE, L"", menu);
}

static void
keys_colon(struct key_info key_info, struct keys_info *keys_info)
{
	enter_cmdline_mode(MENU_CMD_SUBMODE, L"", menu);
}

static void
keys_question(struct key_info key_info, struct keys_info *keys_info)
{
	last_search_backward = 1;
	menu->match_dir = NONE;
	free(menu->regexp);
	enter_cmdline_mode(MENU_SEARCH_BACKWARD_SUBMODE, L"", menu);
}

static void
keys_G(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	moveto_menu_pos(curr_view, menu->len - 1, menu);
	wrefresh(menu_win);
}

static void
keys_N(struct key_info key_info, struct keys_info *keys_info)
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
keys_dd(struct key_info key_info, struct keys_info *keys_info)
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
keys_gg(struct key_info key_info, struct keys_info *keys_info)
{
	clean_menu_position(menu);
	moveto_menu_pos(curr_view, 0, menu);
	wrefresh(menu_win);
}

static void
keys_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos += key_info.count;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
keys_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	clean_menu_position(menu);
	menu->pos -= key_info.count;
	moveto_menu_pos(view, menu->pos, menu);
	wrefresh(menu_win);
}

static void
keys_n(struct key_info key_info, struct keys_info *keys_info)
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
