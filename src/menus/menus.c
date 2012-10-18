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

#ifdef _WIN32
#include <windows.h>
#endif

#include <curses.h>

#include <dirent.h> /* DIR */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> /* access() */

#include <assert.h>
#include <ctype.h> /* isspace() */
#include <string.h> /* strchr() */
#include <stdarg.h>
#include <signal.h>

#include "../cfg/config.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../modes/modes.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../background.h"
#include "../bookmarks.h"
#include "../commands.h"
#include "../filelist.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "../ui.h"
#include "all.h"

static int prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa);
static int prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip);
static void normalize_top(menu_info *m);
static void redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip);

static void
show_position_in_menu(menu_info *m)
{
	char pos_buf[14];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);
	werase(pos_win);
	mvwaddstr(pos_win, 0, 13 - strlen(pos_buf),  pos_buf);
	wrefresh(pos_win);
}

void
clean_menu_position(menu_info *m)
{
	int x, z;
	char * buf = (char *)NULL;
	col_attr_t col;
	int type = MENU_COLOR;

	x = getmaxx(menu_win);

	x += get_utf8_overhead(m->items[m->pos]);

	buf = malloc(x + 2);

	/* TODO: check if this can be false. */
	if(m->items != NULL && m->items[m->pos] != NULL)
	{
		int off = 0;
		z = m->hor_pos;
		while(z-- > 0 && m->items[m->pos][off] != '\0')
		{
			size_t l = get_char_width(m->items[m->pos] + off);
			off += l;
			x -= l - 1;
		}
		snprintf(buf, x, " %s", m->items[m->pos] + off);
	}
	else
	{
		buf[0] = '\0';
	}

	for(z = 0; buf[z] != '\0'; z++)
		if(buf[z] == '\t')
			buf[z] = ' ';

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	col = cfg.cs.color[WIN_COLOR];

	if(cfg.hl_search && m->matches != NULL && m->matches[m->pos])
	{
		mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
		type = SELECTED_COLOR;
	}

	init_pair(DCOLOR_BASE + type, col.fg, col.bg);
	wattrset(menu_win, COLOR_PAIR(type + DCOLOR_BASE) | col.attr);

	wmove(menu_win, m->current, 1);
	if(strlen(m->items[m->pos]) > x - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		if(strlen(buf) > len)
			buf[len] = '\0';
		wprint(menu_win, buf);
		waddstr(menu_win, "...");
	}
	else
	{
		if(strlen(buf) > x - 4 + 1)
			buf[x - 4 + 1] = '\0';
		wprint(menu_win, buf);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(type + DCOLOR_BASE) | col.attr);

	free(buf);
}

void
show_error_msg(const char title[], const char message[])
{
	(void)prompt_error_msg_internal(title, message, 0);
}

void
show_error_msgf(const char title[], const char format[], ...)
{
	va_list pa;

	va_start(pa, format);
	(void)prompt_error_msg_internalv(title, format, 0, pa);
	va_end(pa);
}

int
prompt_error_msg(const char title[], const char message[])
{
	return prompt_error_msg_internal(title, message, 1);
}

int
prompt_error_msgf(const char title[], const char format[], ...)
{
	int result;
	va_list pa;

	va_start(pa, format);
	result = prompt_error_msg_internalv(title, format, 1, pa);
	va_end(pa);

	return result;
}

/* Just a varargs wrapper over prompt_error_msg_internal. */
static int
prompt_error_msg_internalv(const char title[], const char format[],
		int prompt_skip, va_list pa)
{
	char buf[2048];
	vsnprintf(buf, sizeof(buf), format, pa);
	return prompt_error_msg_internal(title, buf, prompt_skip);
}

/* Internal function for displaying messages to a user.  When the prompt_skip
 * isn't zero, asks user about successive messages.  Returns non-zero if all
 * successive messages should be skipped. */
static int
prompt_error_msg_internal(const char title[], const char message[],
		int prompt_skip)
{
	static int skip_until_started;
	int key;

	if(curr_stats.load_stage == 0)
		return 1;
	if(curr_stats.load_stage < 2 && skip_until_started)
		return 1;

	curr_stats.errmsg_shown = 1;

	redraw_error_msg(title, message, prompt_skip);

	do
		key = wgetch(error_win);
	while(key != 13 && (!prompt_skip || key != 3)); /* ascii Return, Ctrl-c */

	if(curr_stats.load_stage < 2)
		skip_until_started = key == 3;

	werase(error_win);
	wrefresh(error_win);

	curr_stats.errmsg_shown = 0;

	modes_update();
	if(curr_stats.need_update != UT_NONE)
		modes_redraw();

	return key == 3;
}

void
init_menu_info(menu_info *m, int menu_type)
{
	m->top = 0;
	m->current = 1;
	m->len = 0;
	m->pos = 0;
	m->hor_pos = 0;
	m->win_rows = getmaxy(menu_win);
	m->type = menu_type;
	m->match_dir = NONE;
	m->matching_entries = 0;
	m->matches = NULL;
	m->regexp = NULL;
	m->title = NULL;
	m->args = NULL;
	m->items = NULL;
	m->data = NULL;
	m->key_handler = NULL;
	m->extra_data = 0;
	m->execute_handler = NULL;
}

void
reset_popup_menu(menu_info *m)
{
	free(m->args);
	/* Menu elements don't always have data associated with them.  That's why we
	 * need this check. */
	if(m->data != NULL)
	{
		free_string_array(m->data, m->len);
	}
	free_string_array(m->items, m->len);
	free(m->regexp);
	free(m->matches);
	free(m->title);

	werase(menu_win);
}

void
setup_menu(void)
{
	scrollok(menu_win, FALSE);
	curs_set(FALSE);
	werase(menu_win);
	werase(status_bar);
	werase(pos_win);
	wrefresh(status_bar);
	wrefresh(pos_win);
}

void
move_to_menu_pos(int pos, menu_info *m)
{
	/* TODO: refactor this function move_to_menu_pos() */

	int redraw = 0;
	int x, z;
	char *buf = NULL;
	col_attr_t col;

	x = getmaxx(menu_win);

	pos = MIN(m->len - 1, MAX(0, pos));
	if(pos < 0)
		return;

	normalize_top(m);

	x += get_utf8_overhead(m->items[pos]);

	if(pos > get_last_visible_line(m))
	{
		m->top = pos - (m->win_rows - 2 - 1);
		redraw = 1;
	}
	else if(pos < m->top)
	{
		m->top = pos;
		redraw = 1;
	}

	if(cfg.scroll_off > 0)
	{
		int s = MIN(DIV_ROUND_UP(m->win_rows - 2, 2), cfg.scroll_off);
		if(pos - m->top < s && m->top > 0)
		{
			m->top -= s - (pos - m->top);
			normalize_top(m);
			redraw = 1;
		}
		if(pos > get_last_visible_line(m) - s)
		{
			m->top += s - (get_last_visible_line(m) - pos);
			normalize_top(m);
			redraw = 1;
		}
	}

	m->current = 1 + (pos - m->top);

	if(redraw)
		draw_menu(m);

	buf = malloc(x + 2);
	if(buf == NULL)
		return;
	/* TODO: check if this can be false. */
	if(m->items[pos] != NULL)
	{
		int off = 0;
		z = m->hor_pos;
		while(z-- > 0 && m->items[pos][off] != '\0')
		{
			size_t l = get_char_width(m->items[pos] + off);
			off += l;
			x -= l - 1;
		}

		snprintf(buf, x, " %s", m->items[pos] + off);
	}
	else
	{
		buf[0] = '\0';
	}

	for(z = 0; buf[z] != '\0'; z++)
		if(buf[z] == '\t')
			buf[z] = ' ';

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	col = cfg.cs.color[WIN_COLOR];

	if(cfg.hl_search && m->matches != NULL && m->matches[pos])
		mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);

	mix_colors(&col, &cfg.cs.color[CURR_LINE_COLOR]);

	init_pair(DCOLOR_BASE + MENU_CURRENT_COLOR, col.fg, col.bg);
	wattrset(menu_win, COLOR_PAIR(DCOLOR_BASE + MENU_CURRENT_COLOR) | col.attr);

	wmove(menu_win, m->current, 1);
	if(strlen(m->items[pos]) > x - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		if(strlen(buf) > len)
			buf[len] = '\0';
		wprint(menu_win, buf);
		waddstr(menu_win, "...");
	}
	else
	{
		if(strlen(buf) > x - 4 + 1)
			buf[x - 4 + 1] = '\0';
		wprint(menu_win, buf);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(DCOLOR_BASE + MENU_CURRENT_COLOR) | col.attr);

	m->pos = pos;
	free(buf);
	show_position_in_menu(m);
}

void
redraw_menu(menu_info *m)
{
	resize_for_menu_like();
	m->win_rows = getmaxy(menu_win);

	draw_menu(m);
	move_to_menu_pos(m->pos, m);
	wrefresh(menu_win);
}

static void
goto_selected_file(FileView *view, menu_info *m)
{
	char *dir;
	char *file;
	char *free_this;
	char *num = NULL;
	char *p = NULL;

	free_this = file = dir = malloc(2 + strlen(m->items[m->pos]) + 1 + 1);
	if(free_this == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(m->items[m->pos][0] != '/')
		strcpy(dir, "./");
	else
		dir[0] = '\0';
	if(m->type == GREP)
	{
		p = strchr(m->items[m->pos], ':');
		if(p != NULL)
		{
			*p = '\0';
			num = p + 1;
		}
		else
		{
			num = NULL;
		}
	}
	strcat(dir, m->items[m->pos]);
	chomp(file);
	if(m->type == GREP && p != NULL)
	{
		int n = 1;

		*p = ':';

		if(num != NULL)
			n = atoi(num);

		if(access(file, R_OK) == 0)
		{
			curr_stats.auto_redraws = 1;
			view_file(file, n, 1);
			curr_stats.auto_redraws = 0;
		}
		free(free_this);
		return;
	}

	if(access(file, R_OK) == 0)
	{
		int isdir = 0;

		if(is_dir(file))
			isdir = 1;

		file = strrchr(dir, '/');
		*file++ = '\0';

		if(change_directory(view, dir) >= 0)
		{
			status_bar_message("Finding the correct directory...");

			wrefresh(status_bar);
			load_dir_list(view, 0);
			if(isdir)
				strcat(file, "/");

			(void)ensure_file_is_selected(view, file);
		}
		else
		{
			show_error_msgf("Invalid path", "Cannot change dir to \"%s\"", dir);
		}
	}
	else if(m->type == LOCATE || m->type == USER_NAVIGATE)
	{
		show_error_msgf("Missing file", "File \"%s\" doesn't exist", file);
	}

	free(free_this);
}

int
execute_menu_cb(FileView *view, menu_info *m)
{
	switch(m->type)
	{
		case APROPOS:
			execute_apropos_cb(m);
			return 1;
		case BOOKMARK:
			move_to_bookmark(view, index2mark(active_bookmarks[m->pos]));
			break;
		case CMDHISTORY:
			save_command_history(m->items[m->pos]);
			exec_commands(m->items[m->pos], view, GET_COMMAND);
			break;
		case FSEARCHHISTORY:
			save_search_history(m->items[m->pos]);
			exec_commands(m->items[m->pos], view, GET_FSEARCH_PATTERN);
			break;
		case BSEARCHHISTORY:
			save_search_history(m->items[m->pos]);
			exec_commands(m->items[m->pos], view, GET_BSEARCH_PATTERN);
			break;
		case COLORSCHEME:
			load_color_scheme(m->items[m->pos]);
			break;
		case COMMAND:
			*strchr(m->items[m->pos], ' ') = '\0';
			exec_command(m->items[m->pos], view, GET_COMMAND);
			break;
		case FILETYPE:
			execute_filetype_cb(view, m);
			break;
		case DIRHISTORY:
			if(!cfg.auto_ch_pos)
			{
				clean_positions_in_history(curr_view);
				curr_stats.ch_pos = 0;
			}
			if(change_directory(view, m->items[m->pos]) >= 0)
			{
				load_dir_list(view, 0);
				move_to_list_pos(view, view->list_pos);
			}
			if(!cfg.auto_ch_pos)
				curr_stats.ch_pos = 1;
			break;
		case DIRSTACK:
			execute_dirstack_cb(view, m);
			break;
		case JOBS:
			execute_jobs_cb(view, m);
			break;
		case LOCATE:
		case FIND:
		case USER_NAVIGATE:
			goto_selected_file(view, m);
			break;
		case GREP:
			goto_selected_file(view, m);
			return 1;
		case VIFM:
			break;
#ifdef _WIN32
		case VOLUMES:
			execute_volumes_cb(view, m);
			break;
#endif
		default:
			break;
	}
	return 0;
}

void
draw_menu(menu_info *m)
{
	int i;
	int win_len;
	int x, y;
	int len;

	getmaxyx(menu_win, y, x);
	win_len = x;
	werase(menu_win);

	box(menu_win, 0, 0);

	normalize_top(m);

	x = m->top;

	wattron(menu_win, A_BOLD);
	wmove(menu_win, 0, 3);
	wprint(menu_win, m->title);
	wattroff(menu_win, A_BOLD);

	for(i = 1; x < m->len; i++, x++)
	{
		int z, off;
		char *buf;
		char *ptr = NULL;
		col_attr_t col;
		int type = WIN_COLOR;

		chomp(m->items[x]);
		if((ptr = strchr(m->items[x], '\n')) || (ptr = strchr(m->items[x], '\r')))
			*ptr = '\0';
		len = win_len + get_utf8_overhead(m->items[x]);

		col = cfg.cs.color[WIN_COLOR];

		if(cfg.hl_search && m->matches != NULL && m->matches[x])
		{
			mix_colors(&col, &cfg.cs.color[SELECTED_COLOR]);
			type = SELECTED_COLOR;
		}

		init_pair(DCOLOR_BASE + type, col.fg, col.bg);
		wattron(menu_win, COLOR_PAIR(DCOLOR_BASE + type) | col.attr);

		z = m->hor_pos;
		off = 0;
		while(z-- > 0 && m->items[x][off] != '\0')
		{
			size_t l = get_char_width(m->items[x] + off);
			off += l;
			len -= l - 1;
		}

		buf = strdup(m->items[x] + off);
		for(z = 0; buf[z] != '\0'; z++)
			if(buf[z] == '\t')
				buf[z] = ' ';

		wmove(menu_win, i, 2);
		if(strlen(buf) > len - 4)
		{
			size_t len = get_normal_utf8_string_widthn(buf, win_len - 3 - 4);
			if(strlen(buf) > len)
				buf[len] = '\0';
			wprint(menu_win, buf);
			waddstr(menu_win, "...");
		}
		else
		{
			if(strlen(buf) > len - 4)
				buf[len - 4] = '\0';
			wprint(menu_win, buf);
		}
		waddstr(menu_win, " ");

		free(buf);

		wattroff(menu_win, COLOR_PAIR(DCOLOR_BASE + type) | col.attr);

		if(i + 3 > y)
			break;
	}
}

/* Ensures that value of m->top lies in a correct range. */
static void
normalize_top(menu_info *m)
{
	m->top = MAX(0, MIN(m->len - (m->win_rows - 2), m->top));
}

int
capture_output_to_menu(FileView *view, const char cmd[], menu_info *m)
{
	FILE *file, *err;
	char buf[4096];
	int x;
	int were_errors;

	if(background_and_capture((char *)cmd, &file, &err) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return 0;
	}

	curr_stats.search = 1;

	x = 0;
	show_progress("", 0);
	while(fgets(buf, sizeof(buf), file) == buf)
	{
		int i, j;
		size_t len;

		j = 0;
		for(i = 0; buf[i] != '\0'; i++)
			j++;

		show_progress("Loading menu", 1000);
		m->items = realloc(m->items, sizeof(char *)*(x + 1));
		len = strlen(buf) + j*(cfg.tab_stop - 1) + 2;
		m->items[x] = malloc(len);

		j = 0;
		for(i = 0; buf[i] != '\0'; i++)
		{
			if(buf[i] == '\t')
			{
				int k = cfg.tab_stop - j%cfg.tab_stop;
				while(k-- > 0)
					m->items[x][j++] = ' ';
			}
			else
			{
				m->items[x][j++] = buf[i];
			}
		}
		m->items[x][j] = '\0';

		x++;
	}

	fclose(file);
	m->len = x;
	curr_stats.search = 0;

	were_errors = print_errors(err);

	if(m->len < 1)
	{
		reset_popup_menu(m);
		return were_errors;
	}

	setup_menu();
	draw_menu(m);
	move_to_menu_pos(m->pos, m);
	enter_menu_mode(m, view);
	return 0;
}

int
query_user_menu(char *title, char *message)
{
	int key;
	int done = 0;
	char *dup = strdup(message);

	curr_stats.errmsg_shown = 2;

	redraw_error_msg(title, message, 0);

	while(!done)
	{
		key = wgetch(error_win);
		if(key == 'y' || key == 'n' || key == ERR)
			done = 1;
	}

	free(dup);

	curr_stats.errmsg_shown = 0;

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_update != UT_NONE)
		update_screen(UT_FULL);

	if(key == 'y')
		return 1;
	else
		return 0;
}

void
redraw_error_msg_window(void)
{
	redraw_error_msg(NULL, NULL, 0);
}

/* Draws error message on the screen or redraws the last message when both
 * title_arg and message_arg are NULL. */
static void
redraw_error_msg(const char title_arg[], const char message_arg[],
		int prompt_skip)
{
	/* TODO: refactor this function redraw_error_msg() */

	static const char *title;
	static const char *message;
	static int ctrl_c;

	int sx, sy;
	int x, y;
	int z;
	const char *text;

	if(title_arg != NULL && message_arg != NULL)
	{
		title = title_arg;
		message = message_arg;
		ctrl_c = prompt_skip;
	}

	assert(message != NULL);

	curs_set(FALSE);
	werase(error_win);

	getmaxyx(stdscr, sy, sx);
	getmaxyx(error_win, y, x);

	z = strlen(message);
	if(z <= x - 2 && strchr(message, '\n') == NULL)
	{
		y = 6;
		wresize(error_win, y, x);
		mvwin(error_win, (sy - y)/2, (sx - x)/2);
		wmove(error_win, 2, (x - z)/2);
		wprint(error_win, message);
	}
	else
	{
		int i;
		int cy = 2;
		i = 0;
		while(i < z)
		{
			int j;
			char buf[x - 2 + 1];

			snprintf(buf, sizeof(buf), "%s", message + i);

			for(j = 0; buf[j] != '\0'; j++)
				if(buf[j] == '\n')
					break;

			if(buf[j] != '\0')
				i++;
			buf[j] = '\0';
			i += j;

			if(buf[0] == '\0')
				continue;

			y = cy + 4;
			mvwin(error_win, (sy - y)/2, (sx - x)/2);
			wresize(error_win, y, x);

			wmove(error_win, cy++, 1);
			wprint(error_win, buf);
		}
	}

	box(error_win, 0, 0);
	if(title[0] != '\0')
		mvwprintw(error_win, 0, (x - strlen(title) - 2)/2, " %s ", title);

	if(curr_stats.errmsg_shown == 1)
	{
		if(ctrl_c)
		{
			text = "Press Return to continue or Ctrl-C to skip other error messages";
		}
		else
		{
			text = "Press Return to continue";
		}
	}
	else
	{
		text = "Enter [y]es or [n]o";
	}
	mvwaddstr(error_win, y - 2, (x - strlen(text))/2, text);
}

/* Returns non-zero if there were errors, closes ef */
int
print_errors(FILE *ef)
{
	char linebuf[160];
	char buf[sizeof(linebuf)*5];
	int error = 0;

	if(ef == NULL)
		return 0;

	buf[0] = '\0';
	while(fgets(linebuf, sizeof(linebuf), ef) == linebuf)
	{
		error = 1;
		if(linebuf[0] == '\n')
			continue;
		if(strlen(buf) + strlen(linebuf) + 1 >= sizeof(buf))
		{
			int skip = (prompt_error_msg("Background Process Error", buf) != 0);
			buf[0] = '\0';
			if(skip)
				break;
		}
		strcat(buf, linebuf);
	}

	if(buf[0] != '\0')
		show_error_msg("Background Process Error", buf);

	fclose(ef);
	return error;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
