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

#include "../config.h"

#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "keys.h"
#include "menu.h"
#include "menus.h"
#include "modes.h"
#include "options.h"
#include "permissions_dialog.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

#include "cmdline.h"

struct line_stats
{
	wchar_t *line;         /* the line reading */
	int index;             /* index of the current character */
	int curs_pos;          /* position of the cursor */
	int len;               /* length of the string */
	int cmd_pos;           /* position in the history */
	wchar_t prompt[320];   /* prompt */
	int prompt_wid;        /* width of prompt */
	int complete_continue; /* If non-zero, continue the previous completion */
};

static int *mode;
static int prev_mode;
static enum CmdLineSubModes sub_mode;
static struct line_stats input_stat;
static int line_width;
static void *sub_mode_ptr;
static char **paths;
static int paths_count;

static void split_path(void);
static int def_handler(wchar_t key);
static void update_cmdline_size(void);
static void update_cmdline_text(void);
static wchar_t * wcsins(wchar_t *src, wchar_t *ins, int pos);
static void prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd);
static void leave_cmdline_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_h(struct key_info, struct keys_info *);
static void cmd_ctrl_i(struct key_info, struct keys_info *);
static void cmd_ctrl_k(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_ctrl_n(struct key_info, struct keys_info *);
static void cmd_ctrl_u(struct key_info, struct keys_info *);
static void cmd_ctrl_w(struct key_info, struct keys_info *);
static void cmd_meta_b(struct key_info, struct keys_info *);
static void find_prev_word(void);
static void cmd_meta_d(struct key_info, struct keys_info *);
static void cmd_meta_f(struct key_info, struct keys_info *);
static void find_next_word(void);
static void cmd_left(struct key_info, struct keys_info *);
static void cmd_right(struct key_info, struct keys_info *);
static void cmd_home(struct key_info, struct keys_info *);
static void cmd_end(struct key_info, struct keys_info *);
static void cmd_delete(struct key_info, struct keys_info *);
static void complete_cmd_next(void);
static void complete_search_next(void);
static void cmd_ctrl_p(struct key_info, struct keys_info *);
static void complete_cmd_prev(void);
static void complete_search_prev(void);
static int line_completion(struct line_stats *stat);
static int colorschemes_completion(char *line_mb, char *last_word,
		struct line_stats *stat);
static int option_completion(char *line_mb, struct line_stats *stat);
static int file_completion(char *filename, char *line_mb,
		struct line_stats *stat);
static void update_line_stat(struct line_stats *stat, int new_len);
static void redraw_status_bar(struct line_stats *stat);
static size_t get_words_count(const char * string);
static char * get_last_word(char * string);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
static char * filename_completion(char *str, int type);
static char * check_for_executable(char *string);

static struct keys_add_info builtin_cmds[] = {
	{L"\x03",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* backspace */
	{L"\x08",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{L"\x09",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0b",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_k}}},
	{L"\x0d",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_n}}},
	{L"\x10",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_p}}},
	/* escape */
	{L"\x1b",         {BUILDIN_WAIT_POINT, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* escape escape */
	{L"\x1b\x1b",     {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* ascii Delete */
	{L"\x7f",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_BACKSPACE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{{KEY_DOWN},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_n}}},
	{{KEY_UP},        {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_p}}},
	{{KEY_LEFT},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	{{KEY_RIGHT},     {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	{{KEY_HOME},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	{{KEY_END},       {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
	{{KEY_DC},        {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
#endif /* ENABLE_EXTENDED_KEYS */
	/* ctrl b */
	{L"\x02", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	/* ctrl f */
	{L"\x06", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	/* ctrl a */
	{L"\x01", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	/* ctrl e */
	{L"\x05", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
	/* ctrl d */
	{L"\x04", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
	{L"\x15", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_w}}},
	{L"\x1b"L"b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_b}}},
	{L"\x1b"L"d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_d}}},
	{L"\x1b"L"f", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_f}}},
};

void
init_cmdline_mode(int *key_mode)
{
	assert(key_mode != NULL);

	mode = key_mode;
	set_def_handler(CMDLINE_MODE, def_handler);

	assert(add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), CMDLINE_MODE) == 0);

	split_path();
}

static void
split_path(void)
{
	char *path, *p, *q;
	int i;

	path = getenv("PATH");

	paths_count = 1;
	p = path;
	while((p = strchr(p, ':')) != NULL)
	{
		paths_count++;
		p++;
	}

	paths = malloc(paths_count*sizeof(paths[0]));
	if(paths == NULL)
		return;

	i = 0;
	p = path - 1;
	do
	{
		int j;
		char *s;

		p++;
		q = strchr(p, ':');
		if(q == NULL)
		{
			q = p + strlen(p);
		}

		s = malloc((q - p + 1)*sizeof(s[0]));
		if(s == NULL)
		{
			for(j = 0; j < i - 1; j++)
				free(paths[j]);
			paths_count = 0;
			return;
		}
		snprintf(s, q - p + 1, "%s", p);

		if(access(s, F_OK) != 0)
		{
			free(s);
			continue;
		}

		p = q;
		paths[i++] = s;

		for(j = 0; j < i - 1; j++)
		{
			if(strcmp(paths[j], s) == 0)
			{
				free(s);
				i--;
				break;
			}
		}
	} while (q[0] != '\0');
	paths_count = i;
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
		update_cmdline_size();

	input_stat.curs_pos += wcwidth(key);
	update_cmdline_text();

	return 0;
}

static void
update_cmdline_size(void)
{
	int d;
	d = (input_stat.prompt_wid + input_stat.len + 1 + line_width - 1)/line_width;
	mvwin(status_bar, getmaxy(stdscr) - d, 0);
	wresize(status_bar, d, line_width);
	werase(status_bar);
}

static void
update_cmdline_text(void)
{
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
	wmove(status_bar, 0, input_stat.curs_pos);
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

	sub_mode_ptr = cb;
	sub_mode = PROMPT_SUBMODE;

	buf = to_wide(cmd);
	if(buf == NULL)
		return;

	prepare_cmdline_mode(prompt, buf);
	free(buf);
}

void
redraw_cmdline(void)
{
	if(prev_mode == MENU_MODE)
		menu_redraw();
	else
	{
		redraw_window();
		if(prev_mode == SORT_MODE)
			redraw_sort_dialog();
		else if(prev_mode == PERMISSIONS_MODE)
			redraw_permissions_dialog();
	}

	line_width = getmaxx(stdscr);
	curs_set(TRUE);
	update_cmdline_size();
	update_cmdline_text();
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
	clean_status_bar();

	if(*mode == CMDLINE_MODE)
		*mode = prev_mode;
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	werase(status_bar);
	wnoutrefresh(status_bar);

	leave_cmdline_mode();
}

static void
cmd_ctrl_h(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index == 0)
	{
		if(input_stat.len == 0 && sub_mode != PROMPT_SUBMODE)
			leave_cmdline_mode();
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
cmd_ctrl_i(struct key_info key_info, struct keys_info *keys_info)
{
	int len;
	if(sub_mode != CMD_SUBMODE)
		return;

	if(input_stat.line == NULL)
	{
		input_stat.line = my_wcsdup(L"");
		if(input_stat.line == NULL)
			return;
	}

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
cmd_ctrl_k(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.complete_continue = 0;

	if(input_stat.index == input_stat.len)
		return;

	wcsdel(input_stat.line, input_stat.index + 1,
			input_stat.len - input_stat.index);
	input_stat.len = input_stat.index;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char* p;
	int save_hist = !keys_info->mapped;
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
		char* s = p;
		while(*s == ' ' || *s == ':')
			s++;
		curr_stats.save_msg = exec_commands(s, curr_view, GET_COMMAND, save_hist);
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_commands(p, curr_view, GET_FSEARCH_PATTERN,
				save_hist);
	}
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_commands(p, curr_view, GET_BSEARCH_PATTERN,
				save_hist);
	}
	else if(sub_mode == MENU_CMD_SUBMODE)
	{
		execute_menu_command(p, sub_mode_ptr);
	}
	else if(sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.need_redraw = 1;
		search_menu_list(p, sub_mode_ptr);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		prompt_cb cb;

		cb = (prompt_cb)sub_mode_ptr;
		cb(p);
	}

	if(prev_mode == VISUAL_MODE)
		leave_visual_mode(curr_stats.save_msg);

	free(p);
}

static void
cmd_ctrl_n(struct key_info key_info, struct keys_info *keys_info)
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
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
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
cmd_ctrl_w(struct key_info key_info, struct keys_info *keys_info)
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
cmd_meta_b(struct key_info key_info, struct keys_info *keys_info)
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
cmd_meta_d(struct key_info key_info, struct keys_info *keys_info)
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

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
cmd_meta_f(struct key_info key_info, struct keys_info *keys_info)
{
	find_next_word();
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
find_next_word(void)
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
}

static void
cmd_left(struct key_info key_info, struct keys_info *keys_info)
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
cmd_right(struct key_info key_info, struct keys_info *keys_info)
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
cmd_home(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = wcslen(input_stat.prompt);
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
cmd_end(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid + input_stat.len;
	wmove(status_bar, 0, input_stat.curs_pos);
}

static void
cmd_delete(struct key_info key_info, struct keys_info *keys_info)
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

/* Returns 0 on success */
static int
replace_input_line(const char *new)
{
	size_t len;
	wchar_t *p;

	len = mbstowcs(NULL, new, 0);
	p = realloc(input_stat.line, (len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return -1;

	input_stat.line = p;
	input_stat.len = len;
	mbstowcs(input_stat.line, new, len + 1);
	return 0;
}

static void
complete_cmd_prev(void)
{
	if(cfg.cmd_history_num < 0)
		return;

	if(++input_stat.cmd_pos > cfg.cmd_history_num)
		input_stat.cmd_pos = 0;

	replace_input_line(cfg.cmd_history[input_stat.cmd_pos]);

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
	if(cfg.search_history_num < 0)
		return;

	if(++input_stat.cmd_pos > cfg.search_history_num)
		input_stat.cmd_pos = 0;

	replace_input_line(cfg.search_history[input_stat.cmd_pos]);

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
cmd_ctrl_p(struct key_info key_info, struct keys_info *keys_info)
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
	if(cfg.cmd_history_num < 0)
		return;

	if(--input_stat.cmd_pos < 0)
		input_stat.cmd_pos = cfg.cmd_history_num;

	replace_input_line(cfg.cmd_history[input_stat.cmd_pos]);

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
	if(cfg.search_history_num < 0)
		return;

	if(--input_stat.cmd_pos < 0)
		input_stat.cmd_pos = cfg.search_history_num;

	replace_input_line(cfg.search_history[input_stat.cmd_pos]);

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
insert_completed_command(struct line_stats *stat, const char *complete_command)
{
	wchar_t *q;
	if(stat->line == NULL)
	{
		return 0;
	}

	if(stat->index < stat->len && (q = wcschr(stat->line, L' ')) != NULL)
	{	/* If the cursor is not at the end of the string... */
		int i;
		void *p;
		wchar_t *buf;

		if((buf = to_wide(complete_command)) == NULL)
			return -1;

		i = wcslen(buf) + wcslen(q) + 1;

		wcsdel(stat->line, 1, q - stat->line);

		if((p = realloc(stat->line, i*sizeof(wchar_t))) == NULL)
		{
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
		int i;
		wchar_t *p;

		if((p = to_wide(complete_command)) == NULL)
			return -1;

		i = wcslen(p) + 1;
		stat->line = p;

		q = wcschr(stat->line, L' ');
		if(q == NULL)
			q = stat->line;
		else
			q++;

		mbstowcs(q, complete_command, i);
		stat->index = stat->len = q - stat->line + i - 1;
		stat->curs_pos = wcswidth(stat->line, stat->len) + stat->prompt_wid;
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, stat->prompt);
	mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
	wmove(status_bar, 0, stat->curs_pos);

	return 0;
}

static int
line_completion(struct line_stats *stat)
{
	static char *line_mb = (char *)NULL;

	size_t words_count;
	int id;
	int usercmd_completion;
	char *last_word = (char *)NULL;

	if(stat->line[stat->index] != L' ' && stat->index != stat->len)
	{
		stat->complete_continue = 0;
		return -1;
	}

	if(stat->complete_continue == 0)
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

		line_mb = (char *) p;
		wcstombs(line_mb, stat->line, i);

		stat->line[stat->index] = t;
	}

	words_count = get_words_count(line_mb);
	last_word = get_last_word(line_mb);
	id = get_buildin_id(line_mb);

	usercmd_completion = (id == COM_DELCOMMAND || id == COM_COMMAND)
			&& words_count <= 2;
	if(!usercmd_completion && (last_word != NULL || id == COM_EXECUTE ||
			id == COM_SET || (id == COM_COLORSCHEME && words_count == 2)))
	{
		char *filename = (char *)NULL;
		char *raw_name = (char *)NULL;

		if(last_word == NULL)
			last_word = strdup("");
		if(last_word == NULL)
			return -1;

		if(id == COM_SET)
		{
			int ret = option_completion(line_mb, stat);
			free(last_word);
			return ret;
		}
		else if(id == COM_COLORSCHEME)
		{
			int ret = colorschemes_completion(line_mb, stat->complete_continue
					? NULL : last_word, stat);
			free(last_word);
			return ret;
		}
		else if(id == COM_EXECUTE && words_count == 1 && last_word[0] != '.')
			raw_name = exec_completion(stat->complete_continue ? NULL : last_word);
		else
			raw_name = filename_completion(stat->complete_continue ? NULL : last_word,
					(id == COM_CD || id == COM_PUSHD) ? 1 : (id == COM_EXECUTE ? 3 : 0));

		stat->complete_continue = 1;

		if(raw_name)
			filename = escape_filename(raw_name, 0, 1);

		if(filename != NULL)
		{
			int ret = file_completion(filename, line_mb, stat);
			free(raw_name);
			free(filename);
			if(ret != 0)
			{
				free(last_word);
				return ret;
			}
		}
	}
	/* :partial_command */
	else
	{
		int users_only = ((id == COM_DELCOMMAND || id == COM_COMMAND)
				&& last_word != NULL);
		char *complete_command = command_completion(
				stat->complete_continue ? NULL : (users_only ? last_word : line_mb),
				users_only);

		if(complete_command != NULL)
		{
			int ret = insert_completed_command(stat, complete_command);
			free(complete_command);
      if(ret != 0)
			{
				free(last_word);
				return ret;
			}
		}
	}

	stat->complete_continue = 1;
	free(last_word);
	return 0;
}

/*
 * p - begin of part that being completed
 * completed - new part of command line
 */
static int
line_part_complete(struct line_stats *stat, const char *line_mb, const char *p,
		const char *completed)
{
	void *t;
	wchar_t *line_ending;
	int new_len;

	new_len = (p - line_mb) + mbstowcs(NULL, completed, 0)
			+ (stat->len - stat->index) + 1;

	line_ending = my_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
		return -1;

	if((t = realloc(stat->line, new_len * sizeof(wchar_t))) == NULL)
		return -1;
	stat->line = (wchar_t *) t;

	swprintf(stat->line + (p - line_mb), new_len, L"%s%ls", completed,
			line_ending);
	free(line_ending);

	stat->complete_continue = 1;
	update_line_stat(stat, new_len);
	redraw_status_bar(stat);
	return 0;
}

static int
colorschemes_completion(char *line_mb, char *last_word, struct line_stats *stat)
{
	char *completed;
	int result;

	completed = complete_colorschemes(last_word);
	result = line_part_complete(stat, line_mb, strchr(line_mb, ' ') + 1,
			completed);
	free(completed);
	return result;
}

static int
option_completion(char* line_mb, struct line_stats *stat)
{
	static const char *p;
	char *completed;
	int result;

	if(!stat->complete_continue)
	{
		completed = strchr(line_mb, ' ');
		if(completed == NULL)
			completed = "";
		else
			while(isspace(*completed))
				completed++;
	}
	completed = complete_options(stat->complete_continue ? NULL : completed, &p);

	result = line_part_complete(stat, line_mb, p, completed);
	free(completed);
	return result;
}

static int
file_completion(char* filename, char* line_mb, struct line_stats *stat)
{
	char *cur_file_pos = strrchr(line_mb, ' ');
	char *temp = (char *) NULL;

	void *p;
	int i;
	char x;
	wchar_t *temp2;

	if(cur_file_pos != NULL)
	{
		if((temp = strrchr(cur_file_pos, '/')) != NULL)
			/* :command /some/directory/partial_filename anything_else... */
			temp++;
		else
			/* :command partial_filename anything_else... */
			temp = cur_file_pos + 1;
	}
	else if((temp = strrchr(line_mb, '!')) != NULL)
	{
		/* :!partial_filename anything_else...		 or
		 * :!!partial_filename anything_else... */
		char *t;
		temp++;
		t = strrchr(temp, '/');
		if(t != NULL)
			temp = t + 1;
	}
	else
		return 0;

	x = *temp;
	*temp = '\0';

	temp2 = my_wcsdup(stat->line + stat->index);
	if(temp2 == NULL)
		return -1;

	i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
			+ (stat->len - stat->index) + 1;

	if((p = realloc(stat->line, i * sizeof(wchar_t))) == NULL)
	{
		*temp = x;
		free(temp2);
		return -1;
	}
	stat->line = (wchar_t *) p;

	swprintf(stat->line, i, L"%s%s%ls", line_mb, filename, temp2);

	update_line_stat(stat, i);

	*temp = x;
	free(temp2);

	redraw_status_bar(stat);
	return 0;
}

static void update_line_stat(struct line_stats *stat, int new_len)
{
	stat->index += (new_len - 1) - stat->len;
	stat->curs_pos = stat->prompt_wid + wcswidth(stat->line, stat->index);
	stat->len = new_len - 1;
}

static void redraw_status_bar(struct line_stats *stat)
{
	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, stat->prompt);
	mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
	wmove(status_bar, 0, stat->curs_pos);
}

static size_t
get_words_count(const char * string)
{
	size_t result;

	result = 1;
	string--;
	while((string = strchr(string + 1, ' ')) != NULL)
	{
		while(*string == ' ')
			string++;
		string--;
		result++;
	}
	return result;
}

/* String returned by this function should be freed by caller */
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
		return strdup(temp);
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
char *
exec_completion(char *str)
{
	static char *string;
	static int last_dir;
	static int dir;

	char *result;
	int cur_dir;

	if(str != NULL)
	{
		free(string);
		string = strdup(str);
		last_dir = -1;
		dir = 0;
	}
	cur_dir = dir;
	do
	{
		if(chdir(paths[dir]) != 0)
			continue;
		result = filename_completion((last_dir != dir) ? string : NULL, 2);
		if(result == NULL)
		{
			last_dir = dir;
			dir = (dir + 1)%paths_count;
			if(dir == 0)
			{
				last_dir = -1;
				if(chdir(curr_view->curr_dir) != 0)
					return NULL;
				return strdup(string);
			}
		}
	}while(result == NULL && cur_dir != dir);
	if(result == NULL)
	{
		dir = 0;
		last_dir = -1;
	}
	else
		last_dir = dir;
	if(chdir(curr_view->curr_dir) != 0)
	{
		free(result);
		return NULL;
	}
	return result;
}

static int
is_entry_dir(const struct dirent *d)
{
	if(d->d_type != DT_DIR && d->d_type != DT_LNK)
		return 0;
	if(d->d_type == DT_LNK && check_link_is_dir(d->d_name))
		return 0;
	return 1;
}

static int
is_entry_exec(const struct dirent *d)
{
	if(d->d_type == DT_DIR)
		return 0;
	if(d->d_type == DT_LNK && check_link_is_dir(d->d_name))
		return 0;
	if(access(d->d_name, X_OK) != 0)
		return 0;
	return 1;
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 *
 * type:
 *  - 0 - all files and directories
 *  - 1 - only directories
 *  - 2 - only executable files
 *  - 3 - directories and executable files
 */
static char *
filename_completion(char *str, int type)
{
	/* TODO refactor filename_completion(...) function */
	static char *string;
	static int offset;

	DIR *dir;
	struct dirent *d;
	char * dirname;
	char * filename;
	char * temp;
	int i;
	int filename_len;

	if(str != NULL)
	{
		free(string);
		string = strdup(str);
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

		snprintf(dirname, strlen(homedir) + strlen(string) + 1, "%s/%s", homedir,
				string + 2);

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

	while((d = readdir(dir)) != NULL)
	{
		if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;
		if(strncmp(d->d_name, filename, filename_len) != 0)
			continue;

		if(type == 1 && !is_entry_dir(d))
			continue;
		else if(type == 2 && !is_entry_exec(d))
			continue;
		else if(type == 3 && !is_entry_dir(d) && !is_entry_exec(d))
			continue;

		break;
	}

	if(d == NULL)
	{
		closedir(dir);
		free(filename);
		free(dirname);
		return NULL;
	}

	i = 0;
	while(i < offset)
	{
		if((d = readdir(dir)) == NULL)
		{
			offset = -1;

			closedir(dir);
			free(dirname);

			return (type == 2) ? NULL : strdup(filename);
		}

		if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;
		if(strncmp(d->d_name, filename, filename_len) != 0)
			continue;

		if(type == 1 && !is_entry_dir(d))
			continue;
		else if(type == 2 && !is_entry_exec(d))
			continue;
		else if(type == 3 && !is_entry_dir(d) && !is_entry_exec(d))
			continue;

		i++;
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
		{
			closedir(dir);
			return NULL;
		}
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
		{
			closedir(dir);
			return NULL;
		}
		snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
		temp = strdup(tempfile);

		free(tempfile);
	}

	free(filename);
	free(dirname);
	closedir(dir);
	return temp;
}

/* String returned by this function should be freed by caller */
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
