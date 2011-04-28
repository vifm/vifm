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

#define _GNU_SOURCE /* I don't know how portable this is but it is
					   needed in Linux for the ncurses wide char
					   functions
					   */

#include <curses.h>

#include <dirent.h> /* DIR */
#include <limits.h>

#include <wchar.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "commands.h"
#include "config.h"
#include "keys.h"
#include "menu.h"
#include "visual.h"
#include "menus.h"
#include "modes.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

#include "cmdline.h"

/*
 * FIXME status bar looks bad after terminal is resized
 * maybe store somewhere parameters and restore them every time
 */

struct line_stats
{
	wchar_t *line;         /* the line reading */
	int index;             /* index of the current character */
	int curs_pos;          /* position of the cursor */
	int len;               /* length of the string */
	int cmd_pos;           /* position in the history */
	wchar_t prompt[80];    /* prompt */
	int prompt_wid;        /* width of prompt */
	int complete_continue; /* If non-zero, continue the previous completion */
};

static int *mode;
static int prev_mode;
static enum CmdLineSubModes sub_mode;
static struct line_stats input_stat;
static int line_width;
static void *sub_mode_ptr;

static void init_extended_keys(void);
static void init_emacs_keys(void);
static int def_handler(wchar_t keys);
static wchar_t * wcsins(wchar_t *src, wchar_t *ins, int pos);
static void prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd);
static void leave_cmdline_mode(void);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_h(struct key_info, struct keys_info *);
static void keys_ctrl_i(struct key_info, struct keys_info *);
static void keys_ctrl_m(struct key_info, struct keys_info *);
static void keys_ctrl_n(struct key_info, struct keys_info *);
static void keys_ctrl_u(struct key_info, struct keys_info *);
static void keys_ctrl_w(struct key_info, struct keys_info *);
static void keys_meta_b(struct key_info, struct keys_info *);
static void find_prev_word(void);
static void keys_meta_f(struct key_info, struct keys_info *);
static void keys_left(struct key_info, struct keys_info *);
static void keys_right(struct key_info, struct keys_info *);
static void keys_home(struct key_info, struct keys_info *);
static void keys_end(struct key_info, struct keys_info *);
static void keys_delete(struct key_info, struct keys_info *);
static void complete_cmd_next(void);
static void complete_search_next(void);
static void keys_ctrl_p(struct key_info, struct keys_info *);
static void complete_cmd_prev(void);
static void complete_search_prev(void);
static int line_completion(struct line_stats *stat);
static int file_completion(char* filename, char* line_mb, char* raw_name,
		struct line_stats *stat);
static char * get_last_word(char * string);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
static char * filename_completion(char *str);
static char * check_for_executable(char *string);

void
init_cmdline_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;
	set_def_handler(CMDLINE_MODE, def_handler);

	curr = add_keys(L"\x03", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_c;

	/* backspace */
	curr = add_keys(L"\x08", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_h;

	curr = add_keys(L"\x09", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_i;

	curr = add_keys(L"\x0d", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_m;

	curr = add_keys(L"\x0e", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_n;

	curr = add_keys(L"\x10", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_p;

	/* escape */
	curr = add_keys(L"\x1b", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_c;
	curr->type = BUILDIN_WAIT_POINT;

	/* ascii Delete */
	curr = add_keys(L"\x7f", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_h;

	init_extended_keys();
	init_emacs_keys();
}

static void
init_extended_keys(void)
{
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_BACKSPACE;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_ctrl_h;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_ctrl_n;

	buf[0] = KEY_UP;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_ctrl_p;

	buf[0] = KEY_LEFT;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_left;

	buf[0] = KEY_RIGHT;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_right;

	buf[0] = KEY_HOME;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_home;

	buf[0] = KEY_END;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_end;

	buf[0] = KEY_DC;
	curr = add_keys(buf, CMDLINE_MODE);
	curr->data.handler = keys_delete;
}

static void
init_emacs_keys(void)
{
	struct key_t *curr;

	/* ctrl b */
	curr = add_keys(L"\x02", CMDLINE_MODE);
	curr->data.handler = keys_left;

	/* ctrl f */
	curr = add_keys(L"\x06", CMDLINE_MODE);
	curr->data.handler = keys_right;

	/* ctrl a */
	curr = add_keys(L"\x01", CMDLINE_MODE);
	curr->data.handler = keys_home;

	/* ctrl e */
	curr = add_keys(L"\x05", CMDLINE_MODE);
	curr->data.handler = keys_end;

	/* ctrl d */
	curr = add_keys(L"\x04", CMDLINE_MODE);
	curr->data.handler = keys_delete;

	curr = add_keys(L"\x15", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_u;

	curr = add_keys(L"\x17", CMDLINE_MODE);
	curr->data.handler = keys_ctrl_w;

	curr = add_keys(L"\x1b"L"b", CMDLINE_MODE);
	curr->data.handler = keys_meta_b;

	curr = add_keys(L"\x1b"L"f", CMDLINE_MODE);
	curr->data.handler = keys_meta_f;
}

static int
def_handler(wchar_t key)
{
	void *p;
	wchar_t buf[2] = {key, L'\0'};

	if(input_stat.complete_continue == 1
			&& input_stat.line[input_stat.index - 1] == L'/' && key == '/')
	{
		input_stat.complete_continue = 0;
		return 0;
	}

	input_stat.complete_continue = 0;

	if(key != L'\r' && !iswprint(key))
		return 0;

	p = realloc(input_stat.line, (input_stat.len+2) * sizeof(wchar_t));
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

	if((input_stat.len + 1) % getmaxx(status_bar) == 0)
	{
		mvwin(status_bar, getbegy(status_bar) - 1, 0);
		wresize(status_bar, getmaxy(status_bar) + 1, getmaxx(status_bar));
		werase(status_bar);
	}

	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	input_stat.curs_pos += wcwidth(key);
	wmove(status_bar, 0, input_stat.curs_pos);

	return 0;
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
enter_cmdline_mode(enum CmdLineSubModes cl_sub_mode, const wchar_t *cmd,
		void *ptr)
{
	const wchar_t *prompt;

	sub_mode_ptr = ptr;
	sub_mode = cl_sub_mode;

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
		prompt = L":";
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE)
		prompt = L"/";
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
		prompt = L"?";
	else
		prompt = L"E";

	prepare_cmdline_mode(prompt, cmd);
}

void
enter_prompt_mode(const wchar_t *prompt, const char *cmd, prompt_cb cb)
{
	wchar_t *buf;
	size_t len;

	sub_mode_ptr = cb;
	sub_mode = PROMPT_SUBMODE;

	len = mbstowcs(NULL, cmd, 0);
	buf = malloc(sizeof(wchar_t)*(len + 1));
	if(buf == NULL)
		return;
	mbstowcs(buf, cmd, len + 1);
	prepare_cmdline_mode(prompt, buf);
	free(buf);
}

static void
prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd)
{
	line_width = getmaxx(stdscr);
	prev_mode = *mode;
	*mode = CMDLINE_MODE;

	input_stat.line = (wchar_t *) NULL;
	input_stat.index = wcslen(cmd);
	input_stat.curs_pos = 0;
	input_stat.len = input_stat.index;
	input_stat.cmd_pos = -1;
	input_stat.complete_continue = 0;

	wcsncpy(input_stat.prompt, prompt, sizeof(input_stat.prompt)/sizeof(wchar_t));
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

	curs_set(1);
	wresize(status_bar, 1, getmaxx(stdscr));
	werase(status_bar);

	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	if(input_stat.line != NULL)
		waddwstr(status_bar, input_stat.line);
	wrefresh(status_bar);
	curr_stats.save_msg = 1;
}

static void
leave_cmdline_mode(void)
{
	if(getmaxy(status_bar) > 1)
	{
		curr_stats.need_redraw = 1;
		wresize(status_bar, 1, getmaxx(stdscr) - 19);
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
	}
	else
		wresize(status_bar, 1, getmaxx(stdscr) - 19);

	curs_set(0);
	curr_stats.save_msg = 0;
	free(input_stat.line);

	if(*mode == CMDLINE_MODE)
	{
		*mode = prev_mode;
		if(prev_mode == VISUAL_MODE)
			leave_visual_mode(0);
	}
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	werase(status_bar);
	wnoutrefresh(status_bar);

	leave_cmdline_mode();
}

static void
keys_ctrl_h(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index == 0)
	{
		if(input_stat.len == 0)
		{
			leave_cmdline_mode();
		}
		return;
	}

	if(input_stat.index == input_stat.len)
	{ /* If the cursor is at the end of the line, maybe filling
		 * spaces by ourselves would goes faster then repaint
		 * the whole window entirely :-) */

		int w, i;

		input_stat.index--;
		input_stat.len--;

		i = input_stat.curs_pos;
		w = wcwidth(input_stat.line[input_stat.index]);
		while(i - input_stat.curs_pos < w)
		{
			mvwaddch(status_bar, input_stat.curs_pos/line_width,
					input_stat.curs_pos%line_width, ' ');
			input_stat.curs_pos--;
		}
		mvwaddch(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width, ' ');

		input_stat.line[input_stat.index] = L'\0';
	}
	else
	{
		input_stat.index--;
		input_stat.len--;

		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wcsdel(input_stat.line, input_stat.index+1, 1);

		werase(status_bar);
		mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
		mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
	}

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
keys_ctrl_i(struct key_info key_info, struct keys_info *keys_info)
{
	int len;
	if(sub_mode != CMD_SUBMODE || input_stat.line == NULL)
		return;

	line_completion(&input_stat);
	len = (1 + input_stat.len + line_width - 1 + 1)/line_width;
	if(len > getmaxy(status_bar))
	{
		int delta = len - getmaxy(status_bar);
		mvwin(status_bar, getbegy(status_bar) - delta, 0);
		wresize(status_bar, getmaxy(status_bar) + delta, line_width);
		werase(status_bar);
		mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
		mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
	}
}

static void
keys_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char* p;
	int i;

	werase(status_bar);
	wnoutrefresh(status_bar);

	if(!input_stat.line || !input_stat.line[0])
	{
		leave_cmdline_mode();
		return;
	}

	i = wcstombs(NULL, input_stat.line, 0) + 1;
	if((p = (char *) malloc(i * sizeof(char))) == NULL)
	{
		leave_cmdline_mode();
		return;
	}

	wcstombs(p, input_stat.line, i);

	leave_cmdline_mode();

	if(sub_mode == CMD_SUBMODE)
	{
		curr_stats.save_msg = exec_command(p, curr_view, GET_COMMAND, NULL);
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_command(p, curr_view, GET_FSEARCH_PATTERN, NULL);
	}
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_command(p, curr_view, GET_BSEARCH_PATTERN, NULL);
	}
	else if(sub_mode == MENU_CMD_SUBMODE)
	{
		execute_menu_command(curr_view, p, sub_mode_ptr);
	}
	else if(sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.need_redraw = 1;
		search_menu_list(curr_view, p, sub_mode_ptr);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		prompt_cb cb;

		cb = (prompt_cb)sub_mode_ptr;
		cb(p);
	}

	free(p);
}

static void
keys_ctrl_n(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_next();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_next();
	}
}

static void
keys_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index == 0)
		return;

	input_stat.len -= input_stat.index;

	input_stat.curs_pos = input_stat.prompt_wid;
	wcsdel(input_stat.line, 1, input_stat.index);

	input_stat.index = 0;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
keys_ctrl_w(struct key_info key_info, struct keys_info *keys_info)
{
	int old;

	old = input_stat.index;
	find_prev_word();

	if(input_stat.index != old)
	{
		wcsdel(input_stat.line, input_stat.index + 1, old - input_stat.index);
		input_stat.len -= old - input_stat.index;
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
keys_meta_b(struct key_info key_info, struct keys_info *keys_info)
{
	find_prev_word();
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
find_prev_word(void)
{
	while(input_stat.index > 0 && isspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
	while(input_stat.index > 0 && !isspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
}

static void
keys_meta_f(struct key_info key_info, struct keys_info *keys_info)
{
	while(input_stat.index < input_stat.len
			&& isspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
	while(input_stat.index < input_stat.len
			&& !isspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
keys_left(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index > 0)
	{
		input_stat.index--;
		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
keys_right(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index < input_stat.len)
	{
		input_stat.curs_pos += wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
keys_home(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = wcslen(input_stat.prompt);
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
keys_end(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid + input_stat.len;
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
keys_delete(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

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
complete_cmd_prev(void)
{
	char *p;

	if(cfg.cmd_history_num < 0)
		return;

	if(++input_stat.cmd_pos > cfg.cmd_history_num)
		input_stat.cmd_pos = 0;

	input_stat.len = mbstowcs(NULL, cfg.cmd_history[input_stat.cmd_pos], 0);
	p = realloc(input_stat.line, (input_stat.len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return;

	input_stat.line = (wchar_t *) p;
	mbstowcs(input_stat.line, cfg.cmd_history[input_stat.cmd_pos],
			input_stat.len + 1);

	input_stat.curs_pos = input_stat.prompt_wid
			+ wcswidth(input_stat.line, input_stat.len);
	input_stat.index = input_stat.len;

	if(input_stat.len >= line_width - 1)
	{
		int new_height = (1 + input_stat.len + 1 + line_width - 1)/line_width;
		mvwin(status_bar, getbegy(status_bar) - (new_height - 1), 0);
		wresize(status_bar, new_height, line_width);
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	if(input_stat.cmd_pos >= cfg.cmd_history_len - 1)
		input_stat.cmd_pos = cfg.cmd_history_len - 1;
}

static void
complete_search_prev(void)
{
	char *p;

	if(cfg.search_history_num < 0)
		return;

	if(++input_stat.cmd_pos > cfg.search_history_num)
		input_stat.cmd_pos = 0;

	input_stat.len = mbstowcs(NULL, cfg.search_history[input_stat.cmd_pos], 0);

	p = realloc(input_stat.line, (input_stat.len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return;

	input_stat.line = (wchar_t *) p;
	mbstowcs(input_stat.line, cfg.search_history[input_stat.cmd_pos],
			input_stat.len + 1);

	input_stat.curs_pos = wcswidth(input_stat.line, input_stat.len)
			+ input_stat.prompt_wid;
	input_stat.index = input_stat.len;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	if(input_stat.cmd_pos > cfg.search_history_len - 1)
		input_stat.cmd_pos = cfg.search_history_len - 1;
}

static void
keys_ctrl_p(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_prev();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_prev();
	}
}

static void
complete_cmd_next(void)
{
	char *p;

	if(cfg.cmd_history_num < 0)
		return;

	if(--input_stat.cmd_pos < 0)
		input_stat.cmd_pos = cfg.cmd_history_num;

	input_stat.len = mbstowcs(NULL, cfg.cmd_history[input_stat.cmd_pos], 0);

	p = realloc(input_stat.line, (input_stat.len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return;

	input_stat.line = (wchar_t *) p;
	mbstowcs(input_stat.line, cfg.cmd_history[input_stat.cmd_pos],
			input_stat.len + 1);

	input_stat.curs_pos = wcswidth(input_stat.line, input_stat.len)
			+ input_stat.prompt_wid;
	input_stat.index = input_stat.len;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	if(input_stat.cmd_pos > cfg.cmd_history_len - 1)
		input_stat.cmd_pos = cfg.cmd_history_len - 1;
}

static void
complete_search_next(void)
{
	char *p;

	if(cfg.search_history_num < 0)
		return;

	if(--input_stat.cmd_pos < 0)
		input_stat.cmd_pos = cfg.search_history_num;

	input_stat.len = mbstowcs(NULL, cfg.search_history[input_stat.cmd_pos], 0);

	p = realloc(input_stat.line, (input_stat.len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return;

	input_stat.line = (wchar_t *) p;
	mbstowcs(input_stat.line, cfg.search_history[input_stat.cmd_pos],
			input_stat.len + 1);

	input_stat.curs_pos = input_stat.prompt_wid
			+ wcswidth(input_stat.line, input_stat.len);
	input_stat.index = input_stat.len;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	if(input_stat.cmd_pos > cfg.search_history_len - 1)
		input_stat.cmd_pos = cfg.search_history_len - 1;
}

static int
line_completion(struct line_stats *stat)
{
	static char *line_mb = (char *)NULL;
	char *last_word = (char *)NULL;
	char *raw_name = (char *)NULL;
	char *filename = (char *)NULL;
	void *p;
	wchar_t t;
	int i;

	if(stat->line[stat->index] != L' ' && stat->index != stat->len)
	{
		stat->complete_continue = 0;
		return -1;
	}

	if(stat->complete_continue == 0)
	{
		/* only complete the part before the cursor
		 * so just copy that part to line_mb */
		t = stat->line[stat->index];
		stat->line[stat->index] = L'\0';

		i = wcstombs(NULL, stat->line, 0) + 1;

		if(! (p = realloc(line_mb, i * sizeof(char))))
		{
			free(line_mb);
			line_mb = NULL;
			return -1;
		}

		line_mb = (char *) p;
		wcstombs(line_mb, stat->line, i);

		stat->line[stat->index] = t;
	}

	last_word = get_last_word(line_mb);

	if(last_word != NULL)
	{
		raw_name = filename_completion(stat->complete_continue ? NULL : last_word);

		stat->complete_continue = 1;

		if(raw_name)
			filename = escape_filename(raw_name, strlen(raw_name), 1);
	}
	 /* :partial_command */
	else
	{
		char *complete_command = command_completion(
				stat->complete_continue ? NULL : line_mb);
		wchar_t *q;

		if(complete_command != NULL)
		{
			if(stat->line == NULL)
			{
				free(raw_name);
				free(filename);
				free(complete_command);
				return 0;
			}

			if((q = wcschr(stat->line, L' ')) != NULL)
			{	/* If the cursor is not at the end of the string... */
				wchar_t *buf;

				i = mbstowcs(NULL, complete_command, 0) + 1;
				if((buf = (wchar_t *) malloc((i + 1) * sizeof(wchar_t))) == NULL)
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				mbstowcs(buf, complete_command, i);

				i += wcslen(q);

				wcsdel(stat->line, 1, q - stat->line);

				if((p = realloc(stat->line, i*sizeof(wchar_t))) == NULL)
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				stat->line = (wchar_t *)p;

				wcsins(stat->line, buf, 1);

				stat->index = wcschr(stat->line, L' ') - stat->line;
				stat->curs_pos = wcswidth(stat->line, stat->index) + stat->prompt_wid;
				stat->len = wcslen(stat->line);

				free(buf);
			}
			else
			{
				i = mbstowcs(NULL, complete_command, 0) + 1;
				if((p = realloc(stat->line, i * sizeof(wchar_t))) == NULL)
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				stat->line = (wchar_t *) p;

				mbstowcs(stat->line, complete_command, i);
				stat->index = stat->len = i - 1;
				stat->curs_pos = wcswidth(stat->line, stat->len) + stat->prompt_wid;
			}

			werase(status_bar);
			mvwaddwstr(status_bar, 0, 0, stat->prompt);
			mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
			wmove(status_bar, 0, stat->curs_pos);
		}

		free(raw_name);
		free(filename);
		free(complete_command);

		stat->complete_continue = 1;
		return 0;
	}

	if(filename != NULL)
	{
		int ret = file_completion(filename, line_mb, raw_name, stat);
		if(ret != 0)
			return ret;
	}

	stat->complete_continue = 1;
	return 0;
}

static int
file_completion(char* filename, char* line_mb, char* raw_name,
		struct line_stats *stat)
{
	char *cur_file_pos = strrchr(line_mb, ' ');
	char *temp = (char *) NULL;

	if(cur_file_pos != NULL)
	{
		void *p;
		int i;
		/* :command /some/directory/partial_filename anything_else... */
		if((temp = strrchr(cur_file_pos, '/')) != NULL)
		{
			char x;
			wchar_t *temp2;

			temp++;
			x = *temp;
			*temp = '\0';

			/* I'm really worry about the portability... */
//				temp2 = (wchar_t *) wcsdup(stat->line + stat->index)
			temp2 = (wchar_t *) malloc((wcslen(stat->line
					+ stat->index) + 1) * sizeof(wchar_t));
			if(temp2 == NULL)
			{
				free(raw_name);
				free(filename);
				free(temp2);
				return -1;
			}
			wcscpy(temp2, stat->line + stat->index);

			i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
					+ (stat->len - stat->index) + 1;

			if((p = realloc(stat->line, i * sizeof(wchar_t))) == NULL)
			{
				*temp = x;
				free(raw_name);
				free(filename);
				free(temp2);
				return -1;
			}
			stat->line = (wchar_t *) p;

			swprintf(stat->line, i, L"%s%s%ls", line_mb,filename,temp2);

			stat->index = i - (stat->len - stat->index) - 1;
			stat->curs_pos = stat->prompt_wid + wcswidth(stat->line, stat->index);
			stat->len = i - 1;

			*temp = x;
			free(raw_name);
			free(filename);
			free(temp2);
		}
		/* :command partial_filename anything_else... */
		else
		{
			int i;
			char x;
			wchar_t *temp2;

			temp = cur_file_pos + 1;
			x = *temp;
			*temp = '\0';

			temp2 = (wchar_t *) malloc((wcslen(stat->line
					 + stat->index) + 1) * sizeof(wchar_t));
			if(temp2 == NULL)
			{
				free(raw_name);
				free(filename);
				free(temp2);
				return -1;
			}
			wcscpy(temp2, stat->line + stat->index);

			i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
				 + (stat->len - stat->index) + 1;

			if((p = realloc(stat->line, i * sizeof(wchar_t))) == NULL)
			{
				*temp = x;
				free(raw_name);
				free(filename);
				free(temp2);
				return -1;
			}
			stat->line = (wchar_t *) p;

			swprintf(stat->line, i, L"%s%s%ls", line_mb,filename,temp2);

			stat->index = i - (stat->len - stat->index) - 1;
			stat->curs_pos = stat->prompt_wid
								+ wcswidth(stat->line, stat->index);
			stat->len = i - 1;

			*temp = x;
			free(raw_name);
			free(filename);
			free(temp2);
		}
	}
	/* :!partial_filename anthing_else...		 or
	 * :!!partial_filename anthing_else... */
	else if((temp = strrchr(line_mb, '!')) != NULL)
	{
		int i;
		char x;
		wchar_t *temp2;
		void *p;

		temp++;
		x = *temp;
		*temp = '\0';

		temp2 = (wchar_t *) malloc((wcslen(stat->line
				 + stat->index) + 1) * sizeof(wchar_t));
		if(temp2 == NULL)
		{
			free(raw_name);
			free(filename);
			return -1;
		}
		wcscpy(temp2, stat->line + stat->index);

		i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
			 + (stat->len - stat->index) + 1;

		if((p = realloc(stat->line, i * sizeof(wchar_t))) == NULL)
		{
			*temp = x;
			free(raw_name);
			free(filename);
			free(temp2);
			return -1;
		}
		stat->line = (wchar_t *) p;

		swprintf(stat->line, i , L"%s%s%ls", line_mb, filename, temp2);

		stat->index = i - (stat->len - stat->index) - 1;
		stat->curs_pos = stat->prompt_wid
							+ wcswidth(stat->line, stat->index);
		stat->len = i - 1;

		*temp = x;
		free(raw_name);
		free(filename);
		free(temp2);
	}
	/* error */
	else
	{
		show_error_msg(" Debug Error ", "Harmless error in rline.c line 564");
		free(raw_name);
		free(filename);
		return -1;
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, stat->prompt);
	mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
	wmove(status_bar, 0, stat->curs_pos);
	return 0;
}

static char *
get_last_word(char * string)
{
	char * temp = (char *)NULL;

	if(string == NULL)
		return NULL;

	/*:command filename */
	temp = strrchr(string, ' ');

	if(temp != NULL)
	{
		temp++;
		return temp;
	}
 /* :!filename or :!!filename */
	temp = check_for_executable(string);
	if(temp != NULL)
		return temp;

	return NULL;
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

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 */
static char *
filename_completion(char *str)
{
	static char *string;
	static int offset;

	DIR *dir;
	struct dirent *d;
	char * dirname = NULL;
	char * filename = NULL;
	char * temp = NULL;
	int i = 0;
	int found = 0;
	int filename_len = 0;

	if(str != NULL)
	{
		string = str;
		offset = 0;
	}
	else
	{
		offset++;
	}

	if(strncmp(string, "~/", 2) == 0)
	{
		char * homedir = getenv("HOME");

		dirname = (char *)malloc((strlen(homedir) + strlen(string) + 1));

		if(dirname == NULL)
			return NULL;

		snprintf(dirname, strlen(homedir) + strlen(string) + 1, "%s/%s",
				homedir, string + 2);

		filename = strdup(dirname);
	}
	else
	{
		dirname = strdup(string);
		filename = strdup(string);
	}

	temp = strrchr(dirname, '/');

	if(temp)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else
	{
		dirname[0] = '.';
		dirname[1] = '\0';
	}

	dir = opendir(dirname);
	filename_len = strlen(filename);

	if(dir == NULL)
	{
		free(filename);
		free(dirname);
		return NULL;
	}

	while ((d=readdir(dir)))
	{
		if( ! strncmp(d->d_name, filename, filename_len)
			 && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
		{
			i = 0;
			while (i < offset)
			{
				if(!(d=readdir(dir)))
				{
					offset = -1;

					closedir(dir);
					free(dirname);

					return strdup(filename);
				}
				if(!strncmp(d->d_name, filename, filename_len)
					 && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
				{
					i++;
				}
			}
			found = 1;
			break;
		}
	}

	closedir(dir);

	if(!found)
	{
		return NULL;
	}

	int isdir = 0;
	if(is_dir(d->d_name))
	{
		isdir = 1;
	}
	else if(strcmp(dirname, "."))
	{
		char * tempfile = (char *)NULL;
		int len = strlen(dirname) + strlen(d->d_name) + 1;
		tempfile = (char *)malloc((len) * sizeof(char));
		if(!tempfile)
			return NULL;
		snprintf(tempfile, len, "%s%s", dirname, d->d_name);
		if(is_dir(tempfile))
			isdir = 1;
		else
			temp = strdup(d->d_name);

		free(tempfile);
	}
	else
		temp = strdup(d->d_name);

	if(isdir)
	{
		char * tempfile = (char *)NULL;
		tempfile = (char *) malloc((strlen(d->d_name) + 2) * sizeof(char));
		if(!tempfile)
			return NULL;
		snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
		temp = strdup(tempfile);

		free(tempfile);
	}

	free(filename);
	free(dirname);
	return temp;
}

static char *
check_for_executable(char *string)
{
	char *temp = (char *)NULL;

	if(!string)
		return NULL;

	if(string[0] == '!')
	{
		if(strlen(string) > 2)
		{
			if(string[1] == '!')
				temp = strdup(string + 2);
			else
				temp = strdup(string + 1);
		}
		else if(strlen(string) > 1)
			temp = strdup(string + 1);
	}
	return temp;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
