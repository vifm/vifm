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
#ifndef _WIN32
#include <sys/ioctl.h>
#include <termios.h> /* struct winsize */
#endif
#include <unistd.h> /* access() */

#include <assert.h>
#include <ctype.h> /* isspace() */
#include <string.h> /* strchr() */
#include <stdarg.h>
#include <signal.h>

#include "../config.h"

#include "background.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "cmds.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "dir_stack.h"
#include "file_magic.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "menu.h"
#include "modes.h"
#include "registers.h"
#include "search.h"
#include "status.h"
#include "string_array.h"
#include "ui.h"
#include "undo.h"
#include "utf8.h"
#include "utils.h"

enum
{
	APROPOS,
	BOOKMARK,
	CMDHISTORY,
	PROMPTHISTORY,
	FSEARCHHISTORY,
	BSEARCHHISTORY,
	COLORSCHEME,
	COMMAND,
	DIRSTACK,
	FILETYPE,
	FIND,
	HISTORY,
	JOBS,
	LOCATE,
	MAP,
	REGISTER,
	UNDOLIST,
	USER,
	USER_NAVIGATE,
	VIFM,
	GREP,
#ifdef _WIN32
	VOLUMES,
#endif
};

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

	x += get_utf8_overhead(m->data[m->pos]);

	buf = malloc(x + 2);

	if(m->data != NULL && m->data[m->pos] != NULL)
	{
		int off = 0;
		z = m->hor_pos;
		while(z-- > 0 && m->data[m->pos][off] != '\0')
		{
			size_t l = get_char_width(m->data[m->pos] + off);
			off += l;
			x -= l - 1;
		}
		snprintf(buf, x, " %s", m->data[m->pos] + off);
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
	if(strlen(m->data[m->pos]) > x - 4)
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
redraw_error_msg(const char *title_arg, const char *message_arg)
{
	static const char *title;
	static const char *message;

	int sx, sy;
	int x, y;
	int z;

	if(title_arg != NULL && message_arg != NULL)
	{
		title = title_arg;
		message = message_arg;
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
		mvwaddstr(error_win, y - 2, (x - 63)/2,
				"Press Return to continue or Ctrl-C to skip other error messages");
	else
		mvwaddstr(error_win, y - 2, (x - 20)/2, "Enter [y]es or [n]o");
}

/* Returns not zero when user asked to skip error messages that left */
int
show_error_msgf(char *title, const char *format, ...)
{
	char buf[2048];
	va_list pa;
	va_start(pa, format);
	vsnprintf(buf, sizeof(buf), format, pa);
	va_end(pa);
	return show_error_msg(title, buf);
}

/* Returns not zero when user asked to skip error messages that left */
int
show_error_msg(const char *title, const char *message)
{
	static int skip_until_started;
	int key;

	if(!curr_stats.load_stage)
		return 1;
	if(curr_stats.load_stage < 2 && skip_until_started)
		return 1;

	curr_stats.errmsg_shown = 1;

	redraw_error_msg(title, message);

	do
		key = wgetch(error_win);
	while(key != 13 && key != 3); /* ascii Return, ascii Ctrl-c */

	if(curr_stats.load_stage < 2)
		skip_until_started = key == 3;

	werase(error_win);
	wrefresh(error_win);

	curr_stats.errmsg_shown = 0;

	modes_update();
	if(curr_stats.need_redraw)
		modes_redraw();

	return key == 3;
}

void
reset_popup_menu(menu_info *m)
{
	free(m->args);
	free_string_array(m->data, m->len);
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
	int redraw = 0;
	int x, z;
	char *buf = NULL;
	col_attr_t col;

	x = getmaxx(menu_win);

	if(pos < 1)
		pos = 0;

	if(pos > m->len - 1)
		pos = m->len - 1;

	if(pos < 0)
		return;

	if(m->top + m->win_rows - 3 > m->len)
		m->top = m->len - (m->win_rows - 3);

	if(m->top < 0)
		m->top = 0;

	x += get_utf8_overhead(m->data[pos]);

	if((m->top <= pos) && (pos <= (m->top + m->win_rows + 1)))
	{
		m->current = pos - m->top + 1;
	}
	if((pos >= (m->top + m->win_rows - 3 + 1)))
	{
		while(pos >= (m->top + m->win_rows - 3 + 1))
			m->top++;

		m->current = m->win_rows - 3 + 1;
		redraw = 1;
	}
	else if(pos < m->top)
	{
		while(pos < m->top)
			m->top--;
		m->current = 1;
		redraw = 1;
	}

	if(cfg.scroll_off > 0)
	{
		int s = MIN((m->win_rows - 2 + 1)/2, cfg.scroll_off);
		if(pos - m->top < s && m->top > 0)
		{
			m->top -= s - (pos - m->top);
			if(m->top < 0)
				m->top = 0;
			m->current = 1 + m->pos - m->top;
			redraw = 1;
		}
		if((m->top + m->win_rows - 2) - pos - 1 < s)
		{
			m->top += s - ((m->top + m->win_rows - 2) - pos - 1);
			if(m->top + m->win_rows - 2 > m->len)
				m->top = m->len - (m->win_rows - 2);
			m->current = 1 + pos - m->top;
			redraw = 1;
		}
	}

	if(redraw)
		draw_menu(m);

	buf = malloc(x + 2);
	if(buf == NULL)
		return;
	if(m->data[pos] != NULL)
	{
		int off = 0;
		z = m->hor_pos;
		while(z-- > 0 && m->data[pos][off] != '\0')
		{
			size_t l = get_char_width(m->data[pos] + off);
			off += l;
			x -= l - 1;
		}

		snprintf(buf, x, " %s", m->data[pos] + off);
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
	if(strlen(m->data[pos]) > x - 4)
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
	int screen_x, screen_y;
#ifndef _WIN32
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	/* changed for pdcurses */
	resizeterm(ws.ws_row, ws.ws_col);
#endif
	flushinp(); /* without it we will get strange character on input */
	getmaxyx(stdscr, screen_y, screen_x);

	wclear(stdscr);
	wclear(status_bar);
	wclear(pos_win);

	wresize(menu_win, screen_y - 1, screen_x);
	wresize(status_bar, 1, screen_x - 19);
	mvwin(status_bar, screen_y - 1, 0);
	wresize(pos_win, 1, 13);
	mvwin(pos_win, screen_y - 1, screen_x - 13);
	mvwin(input_win, screen_y - 1, screen_x - 19);
	wrefresh(status_bar);
	wrefresh(pos_win);
	wrefresh(input_win);
	m->win_rows = screen_y - 1;

	draw_menu(m);
	move_to_menu_pos(m->pos, m);
	wrefresh(menu_win);
}

static void
execute_jobs_cb(FileView *view, menu_info *m)
{
	/* TODO write code for job control */
}

static void
execute_apropos_cb(menu_info *m)
{
	char *line;
	char *man_page;
	char *free_this;
	char *num_str;
	char command[256];

	free_this = man_page = line = strdup(m->data[m->pos]);
	if(free_this == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if((num_str = strchr(line, '(')))
	{
		int z = 0;

		num_str++;
		while(num_str[z] != ')')
		{
			z++;
			if(z > 40)
			{
				free(free_this);
				return;
			}
		}

		num_str[z] = '\0';
		line = strchr(line, ' ');
		line[0] = '\0';

		snprintf(command, sizeof(command), "man %s %s", num_str, man_page);

		curr_stats.auto_redraws = 1;
		shellout(command, 0, 1);
		curr_stats.auto_redraws = 0;
	}
	free(free_this);
}

static void
goto_selected_file(FileView *view, menu_info *m)
{
	char *dir;
	char *file;
	char *free_this;
	char *num = NULL;
	char *p = NULL;

	free_this = file = dir = malloc(2 + strlen(m->data[m->pos]) + 1 + 1);
	if(free_this == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(m->data[m->pos][0] != '/')
		strcpy(dir, "./");
	else
		dir[0] = '\0';
	if(m->type == GREP)
	{
		p = strchr(m->data[m->pos], ':');
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
	strcat(dir, m->data[m->pos]);
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

static void
execute_filetype_cb(FileView *view, menu_info *m)
{
	if(view->dir_entry[view->list_pos].type == DIRECTORY && m->pos == 0)
	{
		handle_dir(view);
	}
	else
	{
		char *prog_str = m->data[m->pos];
		if(prog_str[0] != '\0')
		{
			int background = m->extra_data & 1;
			run_using_prog(view, prog_str, 0, background);
		}
	}

	clean_selected_files(view);
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
}

static void
execute_dirstack_cb(FileView *view, menu_info *m)
{
	int pos = 0;
	int i;

	if(m->data[m->pos][0] == '-')
		return;

	for(i = 0; i < m->pos; i++)
		if(m->data[i][0] == '-')
			pos++;
	rotate_stack(pos);
}

#ifdef _WIN32
static void
execute_volumes_cb(FileView *view, menu_info *m)
{
	char buf[4];
	snprintf(buf, 4, "%s", m->data[m->pos]);

	if(change_directory(view, buf) < 0)
		return;

	load_dir_list(view, 0);
	move_to_list_pos(view, 0);
}
#endif

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
			exec_commands(m->data[m->pos], view, 1, GET_COMMAND);
			break;
		case FSEARCHHISTORY:
			exec_commands(m->data[m->pos], view, 1, GET_FSEARCH_PATTERN);
			break;
		case BSEARCHHISTORY:
			exec_commands(m->data[m->pos], view, 1, GET_BSEARCH_PATTERN);
			break;
		case COLORSCHEME:
			load_color_scheme(m->data[m->pos]);
			break;
		case COMMAND:
			*strchr(m->data[m->pos], ' ') = '\0';
			exec_command(m->data[m->pos], view, GET_COMMAND);
			break;
		case FILETYPE:
			execute_filetype_cb(view, m);
			break;
		case HISTORY:
			if(!cfg.auto_ch_pos)
			{
				clean_positions_in_history(curr_view);
				curr_stats.ch_pos = 0;
			}
			if(change_directory(view, m->data[m->pos]) >= 0)
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

	if(m->win_rows - 2 >= m->len)
		m->top = 0;
	else if(m->len - m->top < m->win_rows - 2)
		m->top = m->len - (m->win_rows - 2);
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

		chomp(m->data[x]);
		if((ptr = strchr(m->data[x], '\n')) || (ptr = strchr(m->data[x], '\r')))
			*ptr = '\0';
		len = win_len + get_utf8_overhead(m->data[x]);

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
		while(z-- > 0 && m->data[x][off] != '\0')
		{
			size_t l = get_char_width(m->data[x] + off);
			off += l;
			len -= l - 1;
		}

		buf = strdup(m->data[x] + off);
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

static char *
uchar2str(wchar_t *c, size_t *len)
{
	static char buf[32];

	*len = 1;
	switch(*c)
	{
		case L' ':
			strcpy(buf, "<space>");
			break;
		case L'\033':
			if(c[1] == L'[' && c[2] == 'Z')
			{
				strcpy(buf, "<s-tab>");
				*len += 2;
				break;
			}
			if(c[1] != L'\0' && c[1] != L'\033')
			{
				strcpy(buf, "<m-a>");
				buf[3] += c[1] - L'a';
				++*len;
				break;
			}
			strcpy(buf, "<esc>");
			break;
		case L'\177':
			strcpy(buf, "<del>");
			break;
		case KEY_HOME:
			strcpy(buf, "<home>");
			break;
		case KEY_END:
			strcpy(buf, "<end>");
			break;
		case KEY_LEFT:
			strcpy(buf, "<left>");
			break;
		case KEY_RIGHT:
			strcpy(buf, "<right>");
			break;
		case KEY_UP:
			strcpy(buf, "<up>");
			break;
		case KEY_DOWN:
			strcpy(buf, "<down>");
			break;
		case KEY_BACKSPACE:
			strcpy(buf, "<bs>");
			break;
		case KEY_BTAB:
			strcpy(buf, "<s-tab>");
			break;
		case KEY_DC:
			strcpy(buf, "<delete>");
			break;
		case KEY_PPAGE:
			strcpy(buf, "<pageup>");
			break;
		case KEY_NPAGE:
			strcpy(buf, "<pagedown>");
			break;
		case '\r':
			strcpy(buf, "<cr>");
			break;

		default:
			if(*c == '\n' || (*c > L' ' && *c < 256))
			{
				buf[0] = *c;
				buf[1] = '\0';
			}
			else if(*c >= KEY_F0 && *c < KEY_F0 + 10)
			{
				strcpy(buf, "<f0>");
				buf[2] += *c - KEY_F0;
			}
			else if(*c >= KEY_F0 + 13 && *c <= KEY_F0 + 21)
			{
				strcpy(buf, "<s-f1>");
				buf[4] += *c - (KEY_F0 + 13);
			}
			else if(*c >= KEY_F0 + 22 && *c <= KEY_F0 + 24)
			{
				strcpy(buf, "<s-f10>");
				buf[5] += *c - (KEY_F0 + 22);
			}
			else if(*c >= KEY_F0 + 25 && *c <= KEY_F0 + 33)
			{
				strcpy(buf, "<c-f1>");
				buf[4] += *c - (KEY_F0 + 25);
			}
			else if(*c >= KEY_F0 + 34 && *c <= KEY_F0 + 36)
			{
				strcpy(buf, "<c-f10>");
				buf[5] += *c - (KEY_F0 + 34);
			}
			else if(*c >= KEY_F0 + 37 && *c <= KEY_F0 + 45)
			{
				strcpy(buf, "<a-f1>");
				buf[4] += *c - (KEY_F0 + 37);
			}
			else if(*c >= KEY_F0 + 46 && *c <= KEY_F0 + 48)
			{
				strcpy(buf, "<a-f10>");
				buf[5] += *c - (KEY_F0 + 46);
			}
			else if(*c >= KEY_F0 + 10 && *c < KEY_F0 + 63)
			{
				strcpy(buf, "<f00>");
				buf[2] += (*c - KEY_F0)/10;
				buf[3] += (*c - KEY_F0)%10;
			}
			else
			{
				strcpy(buf, "<c-A>");
				buf[3] = tolower(buf[3] + *c - 1);
			}
			break;
	}
	return buf;
}

void
show_map_menu(FileView *view, const char *mode_str, wchar_t **list,
		const wchar_t *start)
{
	int x;
	size_t start_len = wcslen(start);

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = MAP;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	m.title = malloc((strlen(mode_str) + 21)*sizeof(char));
	sprintf(m.title, " Mappings for %s mode ", mode_str);

	x = 0;
	while(list[x] != NULL)
	{
		enum { MAP_WIDTH = 10 };
		size_t len;
		int i, str_len, buf_len;

		if(list[x][0] != L'\0' && wcsncmp(start, list[x], start_len) != 0)
		{
			x++;
			continue;
		}

		m.data = realloc(m.data, sizeof(char *)*(m.len + 1));

		str_len = wcslen(list[x]);
		buf_len = 0;
		for(i = 0; i < str_len; i += len)
			buf_len += strlen(uchar2str(list[x] + i, &len));

		if(str_len > 0)
			buf_len += 1 + wcslen(list[x] + str_len + 1)*4 + 1;
		else
			buf_len += 1 + 0 + 1;

		m.data[m.len] = malloc(buf_len + MAP_WIDTH);
		m.data[m.len][0] = '\0';
		for(i = 0; i < str_len; i += len)
			strcat(m.data[m.len], uchar2str(list[x] + i, &len));

		if(str_len > 0)
		{
			int i;
			for(i = strlen(m.data[m.len]); i < MAP_WIDTH; i++)
				strcat(m.data[m.len], " ");

			strcat(m.data[m.len], " ");

			for(i = str_len + 1; list[x][i] != L'\0'; i += len)
			{
				if(list[x][i] == L' ')
				{
					strcat(m.data[m.len], " ");
					len = 1;
				}
				else
				{
					strcat(m.data[m.len], uchar2str(list[x] + i, &len));
				}
			}
		}

		free(list[x]);

		x++;
		m.len++;
	}
	free(list);

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
}

static int
capture_output_to_menu(FileView *view, const char *cmd, menu_info *m)
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
		m->data = realloc(m->data, sizeof(char *)*(x + 1));
		len = strlen(buf) + j*(cfg.tab_stop - 1) + 2;
		m->data[x] = malloc(len);

		j = 0;
		for(i = 0; buf[i] != '\0'; i++)
		{
			if(buf[i] == '\t')
			{
				int k = cfg.tab_stop - j%cfg.tab_stop;
				while(k-- > 0)
					m->data[x][j++] = ' ';
			}
			else
			{
				m->data[x][j++] = buf[i];
			}
		}
		m->data[x][j] = '\0';

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

void
show_apropos_menu(FileView *view, char *args)
{
	char buf[256];
	int were_errors;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = APROPOS;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = strdup(args);
	m.data = NULL;

	m.title = (char *)malloc((strlen(args) + 12) * sizeof(char));
	snprintf(m.title, strlen(args) + 11,  " Apropos %s ",  args);
	snprintf(buf, sizeof(buf), "apropos %s", args);

	were_errors = capture_output_to_menu(view, buf, &m);
	if(!were_errors && m.len < 1)
		show_error_msgf("Nothing Appropriate", "No matches for \'%s\'", m.title);
}

static int
bookmark_khandler(struct menu_info *m, wchar_t *keys)
{
	if(wcscmp(keys, L"dd") == 0)
	{
		clean_menu_position(m);
		remove_bookmark(active_bookmarks[m->pos]);
		memmove(active_bookmarks + m->pos, active_bookmarks + m->pos + 1,
				sizeof(int)*(m->len - 1 - m->pos));

		remove_from_string_array(m->data, m->len, m->pos);
		if(m->matches != NULL)
		{
			if(m->matches[m->pos])
				m->matching_entries--;
			memmove(m->matches + m->pos, m->matches + m->pos + 1,
					sizeof(int)*((m->len - 1) - m->pos));
		}
		m->len--;
		draw_menu(m);

		move_to_menu_pos(m->pos, m);
		return 1;
	}
	return -1;
}

int
show_bookmarks_menu(FileView *view, const char *marks)
{
	int j, x;
	int max_len;
	char buf[PATH_MAX];

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = cfg.num_bookmarks;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = BOOKMARK;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;
	m.key_handler = bookmark_khandler;

	getmaxyx(menu_win, m.win_rows, x);

	m.len = init_active_bookmarks(marks);
	if(m.len == 0)
	{
		(void)show_error_msg("No bookmarks set", "No bookmarks are set.");
		return 0;
	}

	m.title = strdup(" Mark -- Directory -- File ");

	max_len = 0;
	x = 0;
	while(x < m.len)
	{
		size_t len;

		len = get_utf8_string_length(bookmarks[active_bookmarks[x]].directory);
		if(len > max_len)
			max_len = len;
		x++;
	}
	max_len = MIN(max_len + 3, getmaxx(menu_win) - 5 - 2 - 10);

	x = 0;
	while(x < m.len)
	{
		char *with_tilde;
		int overhead;

		j = active_bookmarks[x];
		with_tilde = replace_home_part(bookmarks[j].directory);
		if(strlen(with_tilde) > max_len - 3)
			strcpy(with_tilde + max_len - 6, "...");
		overhead = get_utf8_overhead(with_tilde);
		if(!is_bookmark(j))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[invalid]");
		}
		else if(!pathcmp(bookmarks[j].file, "..") ||
				!pathcmp(bookmarks[j].file, "../"))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[none]");
		}
		else
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, bookmarks[j].file);
		}

		m.data = realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = malloc(sizeof(buf) + 2);
		snprintf(m.data[x], sizeof(buf), "%s", buf);

		x++;
	}
	m.len = x;

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_dirstack_menu(FileView *view)
{
	int i;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = DIRSTACK;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	m.title = strdup(" Directory Stack ");

	m.data = dir_stack_list();

	i = -1;
	while(m.data[++i] != NULL);
	if(i != 0)
	{
		m.len = i;
	}
	else
	{
		m.data[0] = strdup("Directory stack is empty");
		m.len = 1;
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

void
show_colorschemes_menu(FileView *view)
{
	int len;
	DIR *dir;
	struct dirent *d;
	char colors_dir[PATH_MAX];

	static menu_info m;
	m.top = 0;
	m.current = 0;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = COLORSCHEME;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

	m.title = strdup(" Choose the default Color Scheme ");

	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);

	dir = opendir(colors_dir);
	if(dir == NULL)
	{
		free(m.title);
		return;
	}

	while((d = readdir(dir)) != NULL)
	{
#ifndef _WIN32
		if(d->d_type != DT_REG && d->d_type != DT_LNK)
			continue;
#endif

		if(d->d_name[0] == '.')
			continue;

		m.data = (char **)realloc(m.data, sizeof(char *)*(m.len + 1));
		m.data[m.len] = (char *)malloc(len + 2);
		snprintf(m.data[m.len++], len, "%s", d->d_name);
		if(strcmp(d->d_name, cfg.cs.name) == 0)
		{
			m.current = m.len;
			m.pos = m.len - 1;
		}
	}
	closedir(dir);

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
}

static int
command_khandler(struct menu_info *m, wchar_t *keys)
{
	if(wcscmp(keys, L"dd") == 0) /* remove element */
	{
		char cmd_buf[512];

		*strchr(m->data[m->pos] + 1, ' ') = '\0';
		snprintf(cmd_buf, sizeof(cmd_buf), "delcommand %s", m->data[m->pos] + 1);
		execute_cmd(cmd_buf);

		remove_from_string_array(m->data, m->len, m->pos);
		if(m->matches != NULL)
		{
			if(m->matches[m->pos])
				m->matching_entries--;
			memmove(m->matches + m->pos, m->matches + m->pos + 1,
					sizeof(int)*((m->len - 1) - m->pos));
		}
		m->len--;
		draw_menu(m);

		move_to_menu_pos(m->pos, m);
		return 1;
	}
	return -1;
}

int
show_commands_menu(FileView *view)
{
	char **list;
	int i;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = -1;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = COMMAND;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;
	m.key_handler = command_khandler;

	m.title = (char *)malloc((23 + 1)*sizeof(char));
	snprintf(m.title, 23 + 1, " Command ------ Action  ");

	list = list_udf();

	if(list[0] == NULL)
	{
		free(list);
		(void)show_error_msg("No commands set", "No commands are set.");
		return 0;
	}

	while(list[++m.len] != NULL);
	m.len /= 2;

	m.data = malloc(sizeof(char *)*m.len);
	for(i = 0; i < m.len; i++)
	{
		char *buf;

		buf = malloc(strlen(list[i*2]) + 20 + 1 + strlen(list[i*2 + 1]) + 1);
		sprintf(buf, "%-*s %s", 10, list[i*2], list[i*2 + 1]);
		m.data[i] = buf;
	}
	free_string_array(list, m.len*2);

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

static int
filetypes_khandler(struct menu_info *m, wchar_t *keys)
{
	if(wcscmp(keys, L"K") == 0) /* move element up */
	{
		char* tmp;

		if(m->pos == 0)
			return 0;

		tmp = m->data[m->pos - 1];
		m->data[m->pos - 1] = m->data[m->pos];
		m->data[m->pos] = tmp;

		m->pos--;
		m->current--;

		draw_menu(m);
		move_to_menu_pos(m->pos, m);

		return 1;
	}
	else if(wcscmp(keys, L"J") == 0) /* move element down */
	{
		char* tmp;

		if(m->pos == m->len - 1)
			return 0;

		tmp = m->data[m->pos];
		m->data[m->pos] = m->data[m->pos + 1];
		m->data[m->pos + 1] = tmp;

		m->pos++;
		m->current++;

		draw_menu(m);
		move_to_menu_pos(m->pos, m);

		return 1;
	}
	else if(wcscmp(keys, L"L") == 0 && m->len != 0) /* store list */
	{
		char ext[16];
		char *tmp, *extension;
		size_t len = 1;
		int i;

		extension = strchr(get_current_file_name(curr_view), '.');
		if(extension == NULL)
			snprintf(ext, sizeof(ext), "*.%s", extension);
		else
			snprintf(ext, sizeof(ext), "%s", get_current_file_name(curr_view));

		for(i = 0; i < m->len && m->data[i][0] != '\0'; i++)
			len += strlen(m->data[i]) + 1;

		tmp = malloc(len);
		tmp[0] = '\0';
		for(i = 0; i < m->len && m->data[i][0] != '\0'; i++)
			strcat(strcat(tmp, m->data[i]), ",");
		tmp[len - 2] = '\0';

		remove_filetypes(ext);
		set_programs(ext, tmp, 1);
		free(tmp);
		return 0;
	}

	return -1;
}

char *
form_program_list(const char *filename)
{
	char *ft_str, *mime_str;
	int isdir;
	char *result;

	ft_str = get_all_programs_for_file(filename);
	mime_str = get_magic_handlers(filename);

	isdir = is_dir(filename);
	if(ft_str == NULL && mime_str == NULL && !isdir) {
		(void)show_error_msg("Filetype is not set.",
				"No programs set for this filetype.");
		return NULL;
	}

	if(ft_str == NULL)
		ft_str = strdup("");
	if(mime_str == NULL)
		mime_str = "";

	result = malloc(5 + strlen(ft_str) + 3 + strlen(mime_str) + 1);
	if(result == NULL)
	{
		free(ft_str);
		return NULL;
	}

	result[0] = '\0';
	if(isdir)
		strcat(result, "vifm,");
	if(*ft_str != '\0')
	{
		strcat(result, ft_str);
		strcat(result, ",");
	}
	strcat(result, "*,");
	strcat(result, mime_str);
	free(ft_str);

	return result;
}

int
show_filetypes_menu(FileView *view, int background)
{
	static menu_info m;

	char *filename;
	char *prog_str;

	int x = 0;
	int win_width;
	char *p;
	char *ptr;

	filename = get_current_file_name(view);
	prog_str = form_program_list(filename);
	if(prog_str == NULL)
		return 0;

	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = FILETYPE;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Filetype associated commands ");
	m.args = NULL;
	m.data = NULL;
	m.extra_data = (background ? 1 : 0);
	m.key_handler = filetypes_khandler;

	getmaxyx(menu_win, m.win_rows, win_width);

	p = prog_str;
	while(isspace(*p) || *p == ',')
		p++;

	if((ptr = strchr(p, ',')) == NULL)
	{
		m.len = 1;
		m.data = (char **)realloc(m.data, sizeof(char *)*(win_width + 1));
		m.data[0] = strdup(p);
	}
	else
	{
		char *prog_copy = strdup(p);
		char *free_this = prog_copy;
		char *ptr1 = NULL;
		int i;

		while((ptr = ptr1 = strchr(prog_copy, ',')) != NULL)
		{
			int i;

			while(ptr != NULL && ptr[1] == ',')
				ptr = ptr1 = strchr(ptr + 2, ',');
			if(ptr == NULL)
				break;

			*ptr = '\0';
			ptr1++;

			while(isspace(*prog_copy) || *prog_copy == ',')
				prog_copy++;

			for(i = 0; i < m.len; i++)
				if(strcmp(m.data[i], prog_copy) == 0)
					break;
			if(i == m.len && prog_copy[0] != '\0')
			{
				m.data = (char **)realloc(m.data, sizeof(char *)*(m.len + 1));
				if(strcmp(prog_copy, "*") == 0)
					m.data[x] = strdup("");
				else
					m.data[x] = strdup(prog_copy);
				replace_double_comma(m.data[x], 0);
				x++;
				m.len = x;
			}
			prog_copy = ptr1;
		}

		for(i = 0; i < m.len; i++)
			if(strcmp(m.data[i], prog_copy) == 0)
				break;
		if(i == m.len)
		{
			m.data = (char **)realloc(m.data, sizeof(char *)*(m.len + 1));
			m.data[x] = (char *)malloc((win_width + 1)*sizeof(char));
			snprintf(m.data[x], win_width, "%s", prog_copy);
			replace_double_comma(m.data[x], 0);
			m.len++;
		}

		free(free_this);
	}
	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);

	free(prog_str);
	return 0;
}

int
run_with_filetype(FileView *view, const char *beginning, int background)
{
	char *filename;
	char *prog_str;
	char *p;
	char *prog_copy;
	char *free_this;
	size_t len = strlen(beginning);

	filename = get_current_file_name(view);
	prog_str = form_program_list(filename);
	if(prog_str == NULL)
		return 0;

	p = prog_str;
	while(isspace(*p) || *p == ',')
		p++;

	prog_copy = strdup(p);
	free_this = prog_copy;

	while(prog_copy[0] != '\0')
	{
		char *ptr;
		if((ptr = strchr(prog_copy, ',')) == NULL)
			ptr = prog_copy + strlen(prog_copy);

		while(ptr != NULL && ptr[1] == ',')
			ptr = strchr(ptr + 2, ',');
		if(ptr == NULL)
			break;

		*ptr++ = '\0';

		while(isspace(*prog_copy) || *prog_copy == ',')
			prog_copy++;

		if(strcmp(prog_copy, "*") != 0)
		{
			replace_double_comma(prog_copy, 0);
			if(strncmp(prog_copy, beginning, len) == 0)
			{
				if(view->dir_entry[view->list_pos].type == DIRECTORY &&
						prog_copy == free_this)
					handle_dir(view);
				else
					run_using_prog(view, prog_copy, 0, background);
				free(free_this);
				free(prog_str);
				return 0;
			}
		}
		prog_copy = ptr;
	}

	free(free_this);
	free(prog_str);
	return 1;
}

/* Returns new value for save_msg flag */
int
show_history_menu(FileView *view)
{
	int x;
	static menu_info m;

	if(cfg.history_len <= 0)
	{
		status_bar_message("History is disabled");
		return 1;
	}

	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = HISTORY;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Directory History ");
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < view->history_num && x < cfg.history_len; x++)
	{
		int y;
		if(strlen(view->history[x].dir) < 1)
			break;
		if(!is_dir(view->history[x].dir))
			continue;
		for(y = x + 1; y < view->history_num && y < cfg.history_len; y++)
			if(pathcmp(view->history[x].dir, view->history[y].dir) == 0)
				break;
		if(y < view->history_num && y < cfg.history_len)
			continue;

		/* Change the current dir to reflect the current file. */
		if(pathcmp(view->history[x].dir, view->curr_dir) == 0)
		{
			snprintf(view->history[x].file, sizeof(view->history[x].file),
					"%s", view->dir_entry[view->list_pos].name);
			m.pos = m.len;
		}

		m.data = realloc(m.data, sizeof(char *)*(m.len + 1));
		m.data[m.len] = strdup(view->history[x].dir);
		m.len++;
	}
	for(x = 0; x < m.len/2; x++)
	{
		char *t = m.data[x];
		m.data[x] = m.data[m.len - 1 - x];
		m.data[m.len - 1 - x] = t;
	}
	m.pos = m.len - 1 - m.pos;
	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);

	return 0;
}

static int
show_history(FileView *view, int type, int len, char **hist, const char *msg)
{
	int x;
	static menu_info m;

	if(len <= 0)
	{
		status_bar_message("History is disabled or empty");
		return 1;
	}

	m.top = 0;
	m.current = 1;
	m.len = len;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = type;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(msg);
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < len; x++)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc((strlen(hist[x]) + 1)*sizeof(char));
		strcpy(m.data[x], hist[x]);
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_cmdhistory_menu(FileView *view)
{
	return show_history(view, CMDHISTORY, cfg.cmd_history_num + 1,
			cfg.cmd_history, " Command Line History ");
}

int
show_prompthistory_menu(FileView *view)
{
	return show_history(view, PROMPTHISTORY, cfg.prompt_history_num + 1,
			cfg.prompt_history, " Prompt History ");
}

int
show_fsearchhistory_menu(FileView *view)
{
	return show_history(view, FSEARCHHISTORY, cfg.search_history_num + 1,
			cfg.search_history, " Search History ");
}

int
show_bsearchhistory_menu(FileView *view)
{
	return show_history(view, BSEARCHHISTORY, cfg.search_history_num + 1,
			cfg.search_history, " Search History ");
}

int
show_locate_menu(FileView *view, const char *args)
{
	char buf[256];
	int were_errors;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = LOCATE;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = (args[0] == '-') ? strdup(args) : escape_filename(args, 0);
	m.data = NULL;

	snprintf(buf, sizeof(buf), "locate %s", m.args);
	m.title = strdup(buf);

	were_errors = capture_output_to_menu(view, buf, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No files found");
		return 1;
	}
	return 0;
}

int
show_find_menu(FileView *view, int with_path, const char *args)
{
	char buf[256];
	char *files;
	int menu, split;
	int were_errors;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = FIND;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	snprintf(buf, sizeof(buf), "find %s", args);
	m.title = strdup(buf);

	if(view->selected_files > 0)
		files = expand_macros(view, "%f", NULL, &menu, &split);
	else
		files = strdup(".");

	if(args[0] == '-')
		snprintf(buf, sizeof(buf), "find %s %s", files, args);
	else if(with_path)
		snprintf(buf, sizeof(buf), "find %s", args);
	else
	{
		char *escaped_args = escape_filename(args, 0);
		snprintf(buf, sizeof(buf),
				"find %s -type d \\( ! -readable -o ! -executable \\) -prune -o "
				"-name %s -print", files, escaped_args);
		free(escaped_args);
	}
	free(files);

	were_errors = capture_output_to_menu(view, buf, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No files found");
		return 1;
	}
	return 0;
}

int
show_grep_menu(FileView *view, const char *args, int invert)
{
	char title_buf[256];
	char *files;
	int menu, split;
	int were_errors;
	const char *inv_str = invert ? "-v" : "";
	const char grep_cmd[] = "grep -n -H -I -r";
	char *cmd;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = GREP;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	snprintf(title_buf, sizeof(title_buf), "grep %s", args);
	m.title = strdup(title_buf);

	if(view->selected_files > 0)
		files = expand_macros(view, "%f", NULL, &menu, &split);
	else
		files = strdup(".");

	if(args[0] == '-')
	{
		size_t var_len = strlen(inv_str) + 1 + strlen(args) + 1 + strlen(files);
		cmd = malloc(sizeof(grep_cmd) + 1 + var_len);
		sprintf(cmd, "%s %s %s %s", grep_cmd, inv_str, args, files);
	}
	else
	{
		size_t var_len;
		char *escaped_args;
		escaped_args = escape_filename(args, 0);
		var_len = strlen(inv_str) + 1 + strlen(escaped_args) + 1 + strlen(files);
		cmd = malloc(sizeof(grep_cmd) + 1 + var_len);
		sprintf(cmd, "%s %s %s %s", grep_cmd, inv_str, escaped_args, files);
		free(escaped_args);
	}
	free(files);

	status_bar_message("grep is working...");

	were_errors = capture_output_to_menu(view, cmd, &m);
	free(cmd);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No matches found");
		return 1;
	}
	return 0;
}

void
show_user_menu(FileView *view, const char *command, int navigate)
{
	int were_errors;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = navigate ? USER_NAVIGATE : USER;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	m.title = strdup(command);

	were_errors = capture_output_to_menu(view, command, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No results found");
		curr_stats.save_msg = 1;
	}
}

int
show_jobs_menu(FileView *view)
{
	job_t *p;
	finished_job_t *fj;
#ifndef _WIN32
	sigset_t new_mask;
#endif
	int x;
	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = JOBS;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	/*
	 * SIGCHLD needs to be blocked anytime the finished_jobs list
	 * is accessed from anywhere except the received_sigchld().
	 */
#ifndef _WIN32
	sigemptyset(&new_mask);
	sigaddset(&new_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &new_mask, NULL);
#else
	check_background_jobs();
#endif

	p = jobs;
	fj = fjobs;

	x = 0;
	while(p != NULL)
	{
		/* Mark any finished jobs */
		while(fj != NULL)
		{
			if(p->pid == fj->pid)
			{
				p->running = 0;
				fj->remove = 1;
			}
			fj = fj->next;
		}

		if(p->running)
		{
			m.data = (char **)realloc(m.data, sizeof(char *)*(x + 1));
			m.data[x] = (char *)malloc(strlen(p->cmd) + 24);
			if(p->pid == -1)
				snprintf(m.data[x], strlen(p->cmd) + 22, " %d/%d %s ", p->done + 1,
						p->total, p->cmd);
			else
				snprintf(m.data[x], strlen(p->cmd) + 22, " %d %s ", p->pid, p->cmd);

			x++;
		}

		p = p->next;
	}

#ifndef _WIN32
	/* Unblock SIGCHLD signal */
	sigprocmask(SIG_UNBLOCK, &new_mask, NULL);
#endif

	m.len = x;

	if(m.len < 1)
	{
		char buf[256];

		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(strlen("Press return to continue.") + 2);
		snprintf(m.data[x], strlen("Press return to continue."),
					"Press return to continue.");
		snprintf(buf, sizeof(buf), "No background jobs are running");
		m.len = 1;

		m.title = strdup(" No jobs currently running ");
	}
	else
	{
		m.title = strdup(" Pid --- Command ");
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_register_menu(FileView *view, const char *registers)
{
	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = REGISTER;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Registers ");
	m.args = NULL;
	m.data = NULL;

	m.data = list_registers_content(registers);
	while(m.data[m.len] != NULL)
		m.len++;

	if(!m.len)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * 1);
		m.data[0] = strdup(" Registers are empty ");
		m.len = 1;
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_undolist_menu(FileView *view, int with_details)
{
	char **p;

	static menu_info m;
	m.top = 0;
	m.current = get_undolist_pos(with_details) + 1;
	m.len = 0;
	m.pos = m.current - 1;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = UNDOLIST;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Undolist ");
	m.args = NULL;
	m.data = NULL;

	m.data = undolist(with_details);
	p = m.data;
	while(*p++ != NULL)
		m.len++;

	if(!m.len)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * 1);
		m.data[0] = strdup(" Undolist is empty ");
		m.len = 1;
	}
	else
	{
		size_t len;

		m.data[m.len] = strdup("list end");
		m.len++;

		len = (m.data[m.pos] != NULL) ? strlen(m.data[m.pos]) : 0;
		m.data[m.pos] = realloc(m.data[m.pos], len + 1 + 1);
		memmove(m.data[m.pos] + 1, m.data[m.pos], len + 1);
		m.data[m.pos][0] = '*';
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

#ifdef _WIN32
void
show_volumes_menu(FileView *view)
{
	TCHAR c;
	TCHAR vol_name[MAX_PATH];
	TCHAR file_buf[MAX_PATH];

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = VOLUMES;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Mounted Volumes ");
	m.args = NULL;
	m.data = NULL;

	for(c = TEXT('a'); c < TEXT('z'); c++)
	{
		if(drive_exists(c))
		{
			TCHAR drive[] = TEXT("?:\\");
			drive[0] = c;
			if(GetVolumeInformation(drive, vol_name, MAX_PATH, NULL, NULL, NULL,
					file_buf, MAX_PATH))
			{
				m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
				m.data[m.len] = (char *)malloc((MAX_PATH + 5) * sizeof(char));

				snprintf(m.data[m.len], MAX_PATH, "%s  %s ", drive, vol_name);
				m.len++;
			}
		}
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(0, &m);
	enter_menu_mode(&m, view);
}
#endif

int
show_vifm_menu(FileView *view)
{
	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = fill_version_info(NULL);
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = VIFM;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" vifm information ");
	m.args = NULL;
	m.data = malloc(sizeof(char*)*m.len);

	m.len = fill_version_info(m.data);

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
query_user_menu(char *title, char *message)
{
	int key;
	int done = 0;
	char *dup = strdup(message);

	curr_stats.errmsg_shown = 2;

	redraw_error_msg(title, message);

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

	if(curr_stats.need_redraw)
		redraw_window();

	if(key == 'y')
		return 1;
	else
		return 0;
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
			int skip = (show_error_msg("Background Process Error", buf) != 0);
			buf[0] = '\0';
			if(skip)
				break;
		}
		strcat(buf, linebuf);
	}

	if(buf[0] != '\0')
		(void)show_error_msg("Background Process Error", buf);

	fclose(ef);
	return error;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
