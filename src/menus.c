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

#include <sys/types.h>
#include <regex.h>
#include <ctype.h> /* isspace() */
#include <string.h> /* strchr() */
#include <unistd.h> /* access() */
#include <termios.h> /* struct winsize */
#include <stdarg.h>
#include <sys/ioctl.h>
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
#include "registers.h"
#include "search.h"
#include "status.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"

enum {
	APROPOS,
	BOOKMARK,
	CMDHISTORY,
	COLORSCHEME,
	COMMAND,
	DIRSTACK,
	FILETYPE,
	HISTORY,
	JOBS,
	LOCATE,
	MAP,
	REGISTER,
	UNDOLIST,
	USER,
	VIFM,
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
	int x, y, z;
	char * buf = (char *)NULL;

	getmaxyx(menu_win, y, x);

	x += get_utf8_overhead(m->data[m->pos]);

	buf = (char *)malloc(x + 2);

	if(m->data != NULL && m->data[m->pos] != NULL)
		snprintf(buf, x, " %s", m->data[m->pos]);

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	wattron(menu_win, COLOR_PAIR(cfg.color_scheme + WIN_COLOR));

	if(strlen(m->data[m->pos]) > x - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		mvwaddnstr(menu_win, m->current, 1, buf, len);
		waddstr(menu_win, "...");
	}
	else
	{
		mvwaddnstr(menu_win, m->current, 1, buf, x - 4 + 1);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(cfg.color_scheme + CURR_LINE_COLOR) | A_BOLD);

	free(buf);
}

void
redraw_error_msg(char *title_arg, const char *message_arg)
{
	static char *title;
	static const char *message;
	
	int sx, sy;
	int x, y;
	int z;

	if(title_arg != NULL && message_arg != NULL)
	{
		title = title_arg;
		message = message_arg;
	}

	curs_set(0);
	werase(error_win);

	getmaxyx(stdscr, sy, sx);
	getmaxyx(error_win, y, x);

	z = strlen(message);
	if(z <= x - 2)
	{
		y = 6;
		wresize(error_win, y, x);
		mvwin(error_win, (sy - y)/2, (sx - x)/2);
		mvwaddstr(error_win, 2, (x - z)/2, message);
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
				buf[j] = '\0';
			i += j;

			if(buf[0] == '\0')
				continue;

			y = cy + 4;
			mvwin(error_win, (sy - y)/2, (sx - x)/2);
			wresize(error_win, y, x);

			mvwaddstr(error_win, cy++, 1, buf);
		}
	}

	box(error_win, 0, 0);
	if(title[0] != '\0')
		mvwprintw(error_win, 0, (x - strlen(title) - 2)/2, " %s ", title);

	if(curr_stats.errmsg_shown == 1)
		mvwaddstr(error_win, y - 2, (x - 63)/2,
				"Press Return to continue or Ctrl-C to skip other error messages");
	else
		mvwaddstr(error_win, y - 2, (x - 20)/2, "Enter y[es] or n[o]");
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
show_error_msg(char *title, const char *message)
{
	static int skip_until_started;
	int key;

	if(!curr_stats.vifm_started)
		return 1;
	if(curr_stats.vifm_started != 2 && skip_until_started)
		return 1;

	curr_stats.errmsg_shown = 1;

	redraw_error_msg(title, message);

	do
		key = wgetch(error_win);
	while(key != 13 && key != 3); /* ascii Return, ascii Ctrl-c */

	if(curr_stats.vifm_started != 2)
		skip_until_started = key == 3;

	curr_stats.errmsg_shown = 0;

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_redraw)
		redraw_window();

	return key == 3;
}

void
reset_popup_menu(menu_info *m)
{
	int z;

	free(m->args);

	for(z = 0; z < m->len; z++)
	{
		free(m->data[z]);
	}
	free(m->regexp);
	free(m->data);
	free(m->title);

	werase(menu_win);
}

void
setup_menu(void)
{
	scrollok(menu_win, FALSE);
	curs_set(0);
	werase(menu_win);
	werase(status_bar);
	werase(pos_win);
	wrefresh(status_bar);
	wrefresh(pos_win);
}

void
moveto_menu_pos(int pos, menu_info *m)
{
	int redraw = 0;
	int x, y, z;
	char * buf = (char *)NULL;

	getmaxyx(menu_win, y, x);

	if(pos < 1)
		pos = 0;

	if(pos > m->len - 1)
		pos = m->len - 1;

	if(pos < 0)
		return;

	x += get_utf8_overhead(m->data[pos]);

	if((m->top <=  pos) && (pos <= (m->top + m->win_rows + 1)))
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
	else if(pos  < m->top)
	{
		while(pos  < m->top)
			m->top--;
		m->current = 1;
		redraw = 1;
	}
	if(redraw)
		draw_menu(m);

	buf = (char *)malloc(x + 2);
	if(buf == NULL)
		return;
	if(m->data != NULL && m->data[pos] != NULL)
		snprintf(buf, x, " %s", m->data[pos]);

	for(z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	wattron(menu_win, COLOR_PAIR(cfg.color_scheme + CURR_LINE_COLOR) | A_BOLD);

	if(strlen(m->data[pos]) > x - 4)
	{
		size_t len = get_normal_utf8_string_widthn(buf,
				getmaxx(menu_win) - 3 - 4 + 1);
		mvwaddnstr(menu_win, m->current, 1, buf, len);
		waddstr(menu_win, "...");
	}
	else
	{
		mvwaddnstr(menu_win, m->current, 1, buf, x - 4 + 1);
	}
	waddstr(menu_win, " ");

	wattroff(menu_win, COLOR_PAIR(cfg.color_scheme + CURR_LINE_COLOR) | A_BOLD);

	m->pos = pos;
	free(buf);
	show_position_in_menu(m);
}

void
redraw_menu(menu_info *m)
{
	int screen_x, screen_y;
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	/* changed for pdcurses */
	resizeterm(ws.ws_row, ws.ws_col);
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
	moveto_menu_pos(m->pos, m);
	wrefresh(menu_win);
}

static int
search_menu_forwards(menu_info *m, int start_pos)
{
	int cflags;
	int match_up = -1;
	int match_down = -1;
	regex_t re;
	m->matching_entries = 0;

	cflags = get_regexp_cflags(m->regexp);
	if(regcomp(&re, m->regexp, cflags) == 0)
	{
		int x;
		for(x = 0; x < m->len; x++)
		{
			if(regexec(&re, m->data[x], 0, NULL, 0) != 0)
				continue;

			if(match_up < 0)
			{
				if (x < start_pos)
					match_up = x;
			}
			if(match_down < 0)
			{
				if (x >= start_pos)
					match_down = x;
			}
			m->matching_entries++;
		}
	}
	regfree(&re);

	if((match_up > -1) || (match_down > -1))
	{
		char buf[64];
		int pos;

		if(match_down > -1)
			pos = match_down;
		else
			pos = match_up;

		clean_menu_position(m);
		moveto_menu_pos(pos, m);
		snprintf(buf, sizeof(buf), "%d %s", m->matching_entries,
				m->matching_entries == 1 ? "match" : "matches");
		status_bar_message(buf);
		wrefresh(status_bar);
	}
	else
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "No matches for %s", m->regexp);
		status_bar_message(buf);
		wrefresh(status_bar);
		return 1;
	}
	return 0;
}

static int
search_menu_backwards(menu_info *m, int start_pos)
{
	int cflags;
	int match_up = -1;
	int match_down = -1;
	regex_t re;
	m->matching_entries = 0;

	cflags = get_regexp_cflags(m->regexp);
	if(regcomp(&re, m->regexp, cflags) == 0)
	{
		int x;
		for(x = m->len - 1; x > -1; x--)
		{
			if(regexec(&re, m->data[x], 0, NULL, 0) != 0)
				continue;

			if(match_up < 0)
			{
				if (x <= start_pos)
					match_up = x;
			}
			if(match_down < 0)
			{
				if (x > start_pos)
					match_down = x;
			}
			m->matching_entries++;
		}
	}
	regfree(&re);

	if((match_up  > -1) || (match_down > -1))
	{
		char buf[64];
		int pos;

		if (match_up > - 1)
			pos = match_up;
		else
			pos = match_down;

		clean_menu_position(m);
		moveto_menu_pos(pos, m);
		snprintf(buf, sizeof(buf), "%d %s", m->matching_entries,
				m->matching_entries == 1 ? "match" : "matches");
		status_bar_message(buf);
		wrefresh(status_bar);
	}
	else
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "No matches for %s", m->regexp);
		status_bar_message(buf);
		wrefresh(status_bar);
		return 1;
	}
	return 0;
}

int
search_menu_list(char * pattern, menu_info *m)
{
	int save = 0;

	if(pattern)
		m->regexp = strdup(pattern);

	switch(m->match_dir)
	{
		case NONE:
			save = search_menu_forwards(m, m->pos);
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

static void
execute_jobs_cb(FileView *view, menu_info *m)
{
	/* TODO write code for job control */
}

static void
execute_apropos_cb(menu_info *m)
{
	char *line = NULL;
	char *man_page = NULL;
	char *free_this = NULL;
	char *num_str = NULL;
	char command[256];
	int z = 0;

	free_this = man_page = line = strdup(m->data[m->pos]);

	if((num_str = strchr(line, '(')))
	{
		num_str++;
		while(num_str[z] != ')')
		{
			z++;
			if(z > 40)
				return;
		}

		num_str[z] = '\0';
		line = strchr(line, ' ');
		line[0] = '\0';

		snprintf(command, sizeof(command), "man %s %s", num_str,
				man_page);

		shellout(command, 0);
		free(free_this);
	}
	else
		free(free_this);
}

static void
execute_locate_cb(FileView *view, menu_info *m)
{
	char *dir = NULL;
	char *file = NULL;
	char *free_this = NULL;
	int isdir = 0;

	free_this = file = dir = strdup(m->data[m->pos]);
	chomp(file);

	/* :locate -h will show help message */
	if(!access(file, R_OK))
	{
		if(is_dir(file))
			isdir = 1;

		file = strrchr(dir, '/');
		*file = '\0';
		file++;

		change_directory(view, dir);

		status_bar_message("Finding the correct directory.");

		wrefresh(status_bar);
		load_dir_list(view, 1);

		if(find_file_pos_in_list(view, file) < 0)
		{
			if(isdir)
				strcat(file, "/");

			if(file[0] == '.')
				set_dot_files_visible(view, 1);

			if(find_file_pos_in_list(view, file) < 0)
				remove_filename_filter(view);

			moveto_list_pos(view, find_file_pos_in_list(view, file));
		}
		else
			moveto_list_pos(view, find_file_pos_in_list(view, file));
	}

	free(free_this);
}

static void
execute_filetype_cb(FileView *view, menu_info *m)
{
	char *prog_str;
	int background;

	if(view->dir_entry[view->list_pos].type == DIRECTORY && m->pos == 0)
	{
		handle_file(view, 0, 0);
		return;
	}

	prog_str = m->data[m->pos];
	if(prog_str[0] == '\0')
		return;

	background = m->extra_data & 1;
	run_using_prog(view, prog_str, 0, background);

	clean_selected_files(view);
}

void
execute_menu_cb(FileView *view, menu_info *m)
{
	switch(m->type)
	{
		case APROPOS:
			execute_apropos_cb(m);
			break;
		case BOOKMARK:
			move_to_bookmark(view, index2mark(active_bookmarks[m->pos]));
			break;
		case CMDHISTORY:
			exec_commands(m->data[m->pos], view, 1);
			break;
		case COLORSCHEME:
			load_color_scheme(m->data[m->pos]);
			break;
		case COMMAND:
			*strchr(m->data[m->pos] + 1, ' ') = '\0';
			execute_command(view, m->data[m->pos] + 1);
			break;
		case FILETYPE:
			execute_filetype_cb(view, m);
			break;
		case HISTORY:
			change_directory(view, m->data[m->pos]);
			break;
		case JOBS:
			execute_jobs_cb(view, m);
			break;
		case LOCATE:
			execute_locate_cb(view, m);
			break;
		case VIFM:
			break;
		default:
			break;
	}
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
	mvwaddstr(menu_win, 0, 3, m->title);
	wattroff(menu_win, A_BOLD);

	for(i = 1; x < m->len; i++)
	{
		char *ptr = NULL;
		chomp(m->data[x]);
		if((ptr = strchr(m->data[x], '\n')) || (ptr = strchr(m->data[x], '\r')))
			*ptr = '\0';
		len = win_len + get_utf8_overhead(m->data[x]);
		if(strlen(m->data[x]) > len - 4)
		{
			size_t len = get_normal_utf8_string_widthn(m->data[x], win_len - 3 - 4);
			mvwaddnstr(menu_win, i, 2, m->data[x], len);
			waddstr(menu_win, "...");
		}
		else
		{
			mvwaddnstr(menu_win, i, 2, m->data[x], len - 4);
		}
		waddstr(menu_win, " ");
		x++;

		if(i + 3 > y)
			break;
	}
}

void
show_map_menu(FileView *view, const char *mode_str, wchar_t **list)
{
	int x;
	int len;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = MAP;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

	m.title = (char *)malloc((strlen(mode_str) + 21) * sizeof(char));
	sprintf(m.title, " Mappings for %s mode ", mode_str);

	x = 0;
	while(list[x] != NULL)
	{
		int i, str_len, buf_len;

		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));

		str_len = wcslen(list[x]);
		buf_len = 0;
		for(i = 0; i < str_len; i++)
			buf_len += strlen(uchar2str(list[x][i]));

		if(str_len > 0)
			buf_len += 1 + wcslen(list[x] + str_len + 1) + 1;
		else
			buf_len += 1 + 0 + 1;

		m.data[x] = (char *)malloc(buf_len);
		m.data[x][0] = '\0';
		for(i = 0; i < str_len; i++)
			strcat(m.data[x], uchar2str(list[x][i]));

		if(str_len > 0)
		{
			strcat(m.data[x], " ");
			sprintf(m.data[x] + strlen(m.data[x]), "%ls", list[x] + str_len + 1);
		}

		free(list[x]);

		x++;
	}
	free(list);
	m.len = x;

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
}

void
show_apropos_menu(FileView *view, char *args)
{
	int x = 0;
	char buf[256];
	FILE *file;
	int len = 0;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = APROPOS;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = strdup(args);
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

	m.title = (char *)malloc((strlen(args) + 12) * sizeof(char));
	snprintf(m.title, strlen(args) + 11,  " Apropos %s ",  args);
	snprintf(buf, sizeof(buf), "apropos %s", args);
	file = popen(buf, "r");

	if(!file)
	{
		show_error_msg("Trouble opening a file", "Unable to open file");
		return ;
	}
	x = 0;

	curr_stats.search = 1;
	while(fgets(buf, sizeof(buf), file))
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(len);

		snprintf(m.data[x], len - 2, "%s", buf);

		x++;
	}
	pclose(file);
	m.len = x;
	curr_stats.search = 0;
	if (m.len <= 0)
	{
		snprintf(buf, sizeof(buf), "No matches for \'%s\'", m.title);
		show_error_msg("Nothing Appropriate", buf);
		reset_popup_menu(&m);
	}
	else
	{
		setup_menu();
		draw_menu(&m);
		moveto_menu_pos(m.pos, &m);
		enter_menu_mode(&m, view);
	}
}

static int
bookmark_khandler(struct menu_info *m, wchar_t *keys)
{
	if(wcscmp(keys, L"dd") == 0)
	{
		clean_menu_position(m);
		remove_bookmark(active_bookmarks[m->pos]);
		memmove(active_bookmarks + m->pos, active_bookmarks + m->pos + 1,
				sizeof(char *)*(m->len - 1 - m->pos));

		memmove(m->data + m->pos, m->data + m->pos + 1,
				sizeof(char *)*(m->len - 1 - m->pos));
		m->len--;
		draw_menu(m);

		moveto_menu_pos(m->pos, m);
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
	m.win_rows = 0;
	m.type = BOOKMARK;
	m.matching_entries = 0;
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
		show_error_msg("No bookmarks set", "No bookmarks are set.");
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
	max_len += 3;

	x = 0;
	while(x < m.len)
	{
		const char *with_tilde;
		int overhead;

		j = active_bookmarks[x];
		with_tilde = replace_home_part(bookmarks[j].directory);
		overhead = get_utf8_overhead(with_tilde);
		if(!strcmp(bookmarks[j].file, "..") || !strcmp(bookmarks[j].file, "../"))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[none]");
		}
		else if(!strcmp(bookmarks[j].directory, "/"))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j), max_len, "/",
					bookmarks[j].file);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, bookmarks[j].file);
		}

		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(sizeof(buf) + 2);
		snprintf(m.data[x], sizeof(buf), "%s", buf);

		x++;
	}
	m.len = x;

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_dirstack_menu(FileView *view)
{
	int len, i;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = DIRSTACK;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

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
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

void
show_colorschemes_menu(FileView *view)
{
	int len, i, x;

	static menu_info m;
	m.top = 0;
	m.current = 1 + cfg.color_scheme_cur;
	m.len = cfg.color_scheme_num;
	m.pos = cfg.color_scheme_cur;
	m.win_rows = 0;
	m.type = COLORSCHEME;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

	m.title = strdup(" Choose the default Color Scheme ");

	x = 0;
	i = 1;
	while(x < m.len)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(len + 2);
		snprintf(m.data[x], len, "%s", col_schemes[x].name);

		x++;
		i++;
	}

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
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

		memmove(m->data + m->pos, m->data + m->pos + 1,
				sizeof(char *)*(m->len - 1 - m->pos));
		m->len--;
		draw_menu(m);

		moveto_menu_pos(m->pos, m);
		return 1;
	}
	return -1;
}

int
show_commands_menu(FileView *view)
{
	char **list;
	int len, i;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = -1;
	m.pos = 0;
	m.win_rows = 0;
	m.type = COMMAND;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;
	m.key_handler = command_khandler;

	getmaxyx(menu_win, m.win_rows, len);

	m.title = (char *)malloc((23 + 1)*sizeof(char));
	snprintf(m.title, 23 + 1, " Command ------ Action  ");

	list = list_udf();

	if(list[0] == NULL)
	{
		free(list);
		show_error_msg("No commands set", "No commands are set.");
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
	moveto_menu_pos(m.pos, &m);
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
		moveto_menu_pos(m->pos, m);

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
		moveto_menu_pos(m->pos, m);

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
			return 0;
		snprintf(ext, sizeof(ext), "*.%s", extension);

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

int
show_filetypes_menu(FileView *view, int background)
{
	char *filename = get_current_file_name(view);
	char *ft_str, *prog_str, *mime_str;
	int is_dir;

	ft_str = get_all_programs_for_file(filename);
	mime_str = get_magic_handlers(filename);

	is_dir = view->dir_entry[view->list_pos].type == DIRECTORY;
	if(ft_str == NULL && mime_str == NULL && !is_dir) {
		show_error_msg("Filetype is not set.",
				"No programs set for this filetype.");
		return 0;
	}

	if(ft_str == NULL)
		ft_str = strdup("");
	if(mime_str == NULL)
		mime_str = "";

	prog_str = malloc(5 + strlen(ft_str) + 3 + strlen(mime_str) + 1);
	if(prog_str == NULL)
		return 0;

	prog_str[0] = '\0';
	if(is_dir)
		strcat(prog_str, "vifm,");
	if(*ft_str != '\0')
	{
		strcat(prog_str, ft_str);
		strcat(prog_str, ",");
	}
	strcat(prog_str, "*,");
	strcat(prog_str, mime_str);
	free(ft_str);

	{
		int x = 0;
		int len = 0;
		char *p;
		char *ptr = NULL;

		static menu_info m;
		m.top = 0;
		m.current = 1;
		m.len = 0;
		m.pos = 0;
		m.win_rows = 0;
		m.type = FILETYPE;
		m.matching_entries = 0;
		m.match_dir = NONE;
		m.regexp = NULL;
		m.title = NULL;
		m.args = NULL;
		m.data = NULL;
		m.extra_data = (background ? 1 : 0);
		m.key_handler = filetypes_khandler;

		getmaxyx(menu_win, m.win_rows, len);

		p = prog_str;
		while(isspace(*p) || *p == ',')
			p++;

		if((ptr = strchr(p, ',')) == NULL)
		{
			m.len = 1;
			m.data = (char **)realloc(m.data, sizeof(char *) * (len + 1));
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
					m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
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
				m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
				m.data[x] = (char *)malloc((len + 1) * sizeof(char));
				snprintf(m.data[x], len, "%s", prog_copy);
				replace_double_comma(m.data[x], 0);
				m.len++;
			}

			free(free_this);
		}
		setup_menu();
		draw_menu(&m);
		moveto_menu_pos(m.pos, &m);
		enter_menu_mode(&m, view);
	}

	free(prog_str);
	return 0;
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
	m.win_rows = 0;
	m.type = HISTORY;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Directory History ");
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < view->history_num + 1 && x < cfg.history_len; x++)
	{
		int y;
		if(strlen(view->history[x].dir) < 1)
			break;
		for(y = x + 1; y < view->history_num + 1 && y < cfg.history_len; y++)
			if(strcmp(view->history[x].dir, view->history[y].dir) == 0)
				break;
		if(y < view->history_num + 1 && y < cfg.history_len)
			continue;

		/* Change the current dir to reflect the current file. */
		if(strcmp(view->history[x].dir, view->curr_dir) == 0)
		{
			snprintf(view->history[x].file, sizeof(view->history[x].file),
					"%s", view->dir_entry[view->list_pos].name);
			m.pos = m.len;
		}

		m.data = (char **)realloc(m.data, sizeof(char *)*(m.len + 1));
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
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);

	return 0;
}

int
show_cmdhistory_menu(FileView *view)
{
	int x;
	static menu_info m;

	m.top = 0;
	m.current = 1;
	m.len = cfg.cmd_history_num + 1;
	m.pos = 0;
	m.win_rows = 0;
	m.type = CMDHISTORY;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Command Line History ");
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < cfg.cmd_history_num + 1; x++)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc((strlen(cfg.cmd_history[x]) + 1)*sizeof(char));
		strcpy(m.data[x], cfg.cmd_history[x]);
	}
	m.len = x;

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_locate_menu(FileView *view, char *args)
{
	int x = 0;
	char buf[256];
	FILE *file;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = LOCATE;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = escape_filename(args, 0, 0);
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	snprintf(buf, sizeof(buf), "locate %s", m.args);
	m.title = strdup(buf);
	file = popen(buf, "r");

	if(file == NULL)
	{
		show_error_msg("Trouble opening a file", "Unable to open file");
		return 0;
	}
	x = 0;

	curr_stats.search = 1;

	while(fgets(buf, sizeof(buf), file))
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(sizeof(buf) + 2);
		snprintf(m.data[x], sizeof(buf), "%s", buf);

		x++;
	}

	pclose(file);
	m.len = x;
	curr_stats.search = 0;

	if(m.len < 1)
	{
		reset_popup_menu(&m);
		status_bar_messagef("No files found matching \"%s\"", args);
		return 1;
	}

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

void
show_user_menu(FileView *view, char *command)
{
	size_t x;
	char buf[256];
	FILE *file;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = USER;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;
	m.get_info_script = command;

	for(x = 0; command[x] != '\0'; x++)
	{
		if(command[x] == ',')
		{
			command[x] = '\0';
			break;
		}
	}

	getmaxyx(menu_win, m.win_rows, x);

	snprintf(buf, sizeof(buf), " %s ",	m.get_info_script);
	m.title = strdup(buf);
	file = popen(buf, "r");

	if(!file)
	{
		reset_popup_menu(&m);
		show_error_msg("Trouble opening a file", "Unable to open file");
		return;
	}
	x = 0;

	curr_stats.search = 1;

	show_progress("", 0);
	while(fgets(buf, sizeof(buf), file))
	{
		show_progress("Loading menu", 1000);
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(sizeof(buf) + 2);
		snprintf(m.data[x], sizeof(buf), "%s", buf);

		x++;
	}

	pclose(file);
	m.len = x;
	curr_stats.search = 0;

	if(m.len < 1)
	{
		char buf[256];

		reset_popup_menu(&m);
		snprintf(buf, sizeof(buf), "No results found");
		status_bar_message(buf);
		wrefresh(status_bar);
		return;
	}

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
}

int
show_jobs_menu(FileView *view)
{
	Jobs_List *p = jobs;
	Finished_Jobs *fj = NULL;
	sigset_t new_mask;
	int x;
	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = JOBS;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	x = 0;

	/*
	 * SIGCHLD needs to be blocked anytime the Finished_Jobs list
	 * is accessed from anywhere except the received_sigchld().
	 */
	sigemptyset(&new_mask);
	sigaddset(&new_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &new_mask, NULL);

	fj = fjobs;

	while(p)
	{
		/* Mark any finished jobs */
		while(fj)
		{
			if (p->pid == fj->pid)
			{
				p->running = 0;
				fj->remove = 1;
			}
			fj = fj->next;
		}

		if(p->running)
		{
			m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
			m.data[x] = (char *)malloc(strlen(p->cmd) + 24);
			snprintf(m.data[x], strlen(p->cmd) + 22, " %d %s ", p->pid, p->cmd);

			x++;
		}

		p = p->next;
	}

	/* Unblock SIGCHLD signal */
	sigprocmask(SIG_UNBLOCK, &new_mask, NULL);

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
		m.title = strdup(" Pid --- Command ");

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_register_menu(FileView *view, const char *registers)
{
	int x;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = REGISTER;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Registers ");
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

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
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_undolist_menu(FileView *view, int with_details)
{
	int x;
	char **p;

	static menu_info m;
	m.top = 0;
	m.current = get_undolist_pos(with_details) + 1;
	m.len = 0;
	m.pos = m.current - 1;
	m.win_rows = 0;
	m.type = UNDOLIST;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Undolist ");
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

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
	moveto_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

int
show_vifm_menu(FileView *view)
{
	int x;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = fill_version_info(NULL);
	m.pos = 0;
	m.win_rows = 0;
	m.type = VIFM;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" vifm information ");
	m.args = NULL;
	m.data = malloc(sizeof(char*)*m.len);

	getmaxyx(menu_win, m.win_rows, x);

	m.len = fill_version_info(m.data);

	setup_menu();
	draw_menu(&m);
	moveto_menu_pos(m.pos, &m);
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
		show_error_msg("Background Process Error", buf);

	fclose(ef);
	return error;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
