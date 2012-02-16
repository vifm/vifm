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

#define _GNU_SOURCE /* I don't know how portable this is but it is
                     * needed in Linux for the ncurses wide char
                     * functions
                     */

#ifndef _WIN32
#include <sys/ioctl.h>
#endif
#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR */
#ifndef _WIN32
#include <grp.h> /* getgrgid() */
#include <pwd.h> /* getpwent() */
#endif
#include <stdlib.h> /* malloc */
#ifndef _WIN32
#include <termios.h> /* struct winsize */
#endif
#include <unistd.h>

#include <assert.h>
#include <ctype.h>
#include <signal.h> /* signal() */
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../config.h"

#include "color_scheme.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "log.h"
#include "macros.h"
#include "main_loop.h"
#include "menus.h"
#include "modes.h"
#include "opt_handlers.h"
#include "signals.h"
#include "status.h"
#include "utf8.h"
#include "utils.h"
#include "view.h"

#include "ui.h"

static int status_bar_lines;

static WINDOW *ltop_line1;
static WINDOW *ltop_line2;
static WINDOW *rtop_line1;
static WINDOW *rtop_line2;

static void update_attributes(void);
static void reload_view(FileView *view);

static void _gnuc_noreturn
finish(const char *message)
{
	endwin();
	write_info_file();
	printf("%s", message);
	exit(0);
}

static char *
break_in_two(char *str, size_t max)
{
	int i;
	size_t len, size;
	char *result;
	char *break_point = strstr(str, "%=");
	if(break_point == NULL)
		return str;

	len = get_utf8_string_length(str) - 2;
	size = strlen(str);
	size = MAX(size, max);
	result = malloc(size*4 + 2);

	snprintf(result, break_point - str + 1, "%s", str);

	if(len > max)
	{
		int l = get_utf8_string_length(result) - (len - max);
		break_point = str + get_real_string_width(str, MAX(l, 0));
	}

	snprintf(result, break_point - str + 1, "%s", str);
	i = break_point - str;
	while(max > len)
	{
		result[i++] = ' ';
		max--;
	}
	result[i] = '\0';

	if(len > max)
		break_point = strstr(str, "%=");
	strcat(result, break_point + 2);

	free(str);
	return result;
}

static char *
expand_ruler_macros(FileView *view, const char *format)
{
	char *result = strdup("");
	size_t len = 0;
	char c;

	while((c = *format++) != '\0')
	{
		size_t width = 0;
		int left_align = 0;
		char *p;
		char buf[32];
		if(c != '%' || (strchr("-lLS%0123456789", *format) == NULL))
		{
			p = realloc(result, len + 1 + 1);
			if(p == NULL)
				break;
			result = p;
			result[len++] = c;
			result[len] = '\0';
			continue;
		}
		if(*format == '-')
		{
			left_align = 1;
			format++;
		}
		while(isdigit(*format))
			width = width*10 + *format++ - '0';
		c = *format++;
		switch(c)
		{
			case '-':
				snprintf(buf, sizeof(buf), "%d", view->filtered);
				break;
			case 'l':
				snprintf(buf, sizeof(buf), "%d", view->list_pos + 1);
				break;
			case 'L':
				snprintf(buf, sizeof(buf), "%d", view->list_rows + view->filtered);
				break;
			case 'S':
				snprintf(buf, sizeof(buf), "%d", view->list_rows);
				break;
			case '%':
				snprintf(buf, sizeof(buf), "%%");
				break;
		}
		if(strlen(buf) < width)
		{
			if(left_align)
			{
				int i = width - strlen(buf);
				memset(buf + strlen(buf), ' ', i);
				buf[width] = '\0';
			}
			else
			{
				int i = width - strlen(buf);
				memmove(buf + i, buf, width - i + 1);
				memset(buf, ' ', i);
			}
		}
		p = realloc(result, len + strlen(buf) + 1);
		if(p == NULL)
			break;
		result = p;
		strcat(result, buf);
		len += strlen(buf);
	}

	return result;
}

void
update_pos_window(FileView *view)
{
	char *buf;

	buf = expand_ruler_macros(view, cfg.ruler_format);
	buf = break_in_two(buf, 13);

	werase(pos_win);
	mvwaddstr(pos_win, 0, 0, buf);
	wnoutrefresh(pos_win);

	free(buf);
}

static void
get_uid_string(FileView *view, size_t len, char *out_buf)
{
#ifndef _WIN32
	char buf[sysconf(_SC_GETPW_R_SIZE_MAX) + 1];
	char uid_buf[26];
	struct passwd pwd_b;
	struct passwd *pwd_buf;

	if(getpwuid_r(view->dir_entry[view->list_pos].uid, &pwd_b, buf, sizeof(buf),
			&pwd_buf) != 0 || pwd_buf == NULL)
	{
		snprintf(uid_buf, sizeof(uid_buf), "%d",
				(int) view->dir_entry[view->list_pos].uid);
	}
	else
	{
		snprintf(uid_buf, sizeof(uid_buf), "%s", pwd_buf->pw_name);
	}

	snprintf(out_buf, len, "%s", uid_buf);
#else
	out_buf[0] = '\0';
#endif
}

static void
get_gid_string(FileView *view, size_t len, char *out_buf)
{
#ifndef _WIN32
	char buf[sysconf(_SC_GETGR_R_SIZE_MAX) + 1];
	char gid_buf[26];
	struct group group_b;
	struct group *group_buf;

	if(getgrgid_r(view->dir_entry[view->list_pos].gid, &group_b, buf, sizeof(buf),
			&group_buf) != 0 || group_buf == NULL)
	{
		snprintf(gid_buf, sizeof(gid_buf), "%d",
				(int) view->dir_entry[view->list_pos].gid);
	}
	else
	{
		snprintf(gid_buf, sizeof(gid_buf), "%s", group_buf->gr_name);
	}

	snprintf(out_buf, len, "%s", gid_buf);
#else
	out_buf[0] = '\0';
#endif
}

static char *
expand_status_line_macros(FileView *view, const char *format)
{
	char *result = strdup("");
	size_t len = 0;
	char c;

	while((c = *format++) != '\0')
	{
		size_t width = 0;
		int left_align = 0;
		char *p;
		char buf[PATH_MAX];
		if(c != '%' || (strchr("tAugsd-lLS%0123456789", *format) == NULL))
		{
			p = realloc(result, len + 1 + 1);
			if(p == NULL)
				break;
			result = p;
			result[len++] = c;
			result[len] = '\0';
			continue;
		}
		if(*format == '-')
		{
			left_align = 1;
			format++;
		}
		while(isdigit(*format))
			width = width*10 + *format++ - '0';
		c = *format++;
		switch(c)
		{
			case 't':
				snprintf(buf, sizeof(buf), "%s", get_current_file_name(view));
				break;
			case 'A':
#ifndef _WIN32
				get_perm_string(buf, sizeof(buf), view->dir_entry[view->list_pos].mode);
#else
				snprintf(buf, sizeof(buf), "%s",
						attr_str_long(view->dir_entry[view->list_pos].attrs));
#endif
				break;
			case 'u':
				get_uid_string(view, sizeof(buf), buf);
				break;
			case 'g':
				get_gid_string(view, sizeof(buf), buf);
				break;
			case 's':
				friendly_size_notation(view->dir_entry[view->list_pos].size,
						sizeof(buf), buf);
				break;
			case 'd':
				{
					struct tm *tm_ptr = localtime(&view->dir_entry[view->list_pos].mtime);
					strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
				}
				break;
			case '-':
				snprintf(buf, sizeof(buf), "%d", view->filtered);
				break;
			case 'l':
				snprintf(buf, sizeof(buf), "%d", view->list_pos + 1);
				break;
			case 'L':
				snprintf(buf, sizeof(buf), "%d", view->list_rows + view->filtered);
				break;
			case 'S':
				snprintf(buf, sizeof(buf), "%d", view->list_rows);
				break;
			case '%':
				snprintf(buf, sizeof(buf), "%%");
				break;
		}
		if(strlen(buf) < width)
		{
			if(left_align)
			{
				int i = width - strlen(buf);
				memset(buf + strlen(buf), ' ', i);
				buf[width] = '\0';
			}
			else
			{
				int i = width - strlen(buf);
				memmove(buf + i, buf, width - i + 1);
				memset(buf, ' ', i);
			}
		}
		p = realloc(result, len + strlen(buf) + 1);
		if(p == NULL)
			break;
		result = p;
		strcat(result, buf);
		len += strlen(buf);
	}

	return result;
}

static void
update_stat_window_old(FileView *view)
{
	char name_buf[160*2 + 1];
	char perm_buf[26];
	char size_buf[56];
	char id_buf[52];
	int x;
	int cur_x;
	size_t print_width;
	char *filename;

	if(!cfg.last_status)
		return;

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	filename = get_current_file_name(view);
	print_width = get_real_string_width(filename, 20 + MAX(0, x - 83));
	snprintf(name_buf, MIN(sizeof(name_buf), print_width + 1), "%s", filename);
	friendly_size_notation(view->dir_entry[view->list_pos].size, sizeof(size_buf),
			size_buf);

	get_uid_string(view, sizeof(id_buf), id_buf);
	if(id_buf[0] != '\0')
		strcat(id_buf, ":");
	get_gid_string(view, sizeof(id_buf) - strlen(id_buf),
			id_buf + strlen(id_buf));
#ifndef _WIN32
	get_perm_string(perm_buf, sizeof(perm_buf),
			view->dir_entry[view->list_pos].mode);
#else
	snprintf(perm_buf, sizeof(perm_buf), "%s",
			attr_str_long(view->dir_entry[view->list_pos].attrs));
#endif

	werase(stat_win);
	cur_x = 2;
	wmove(stat_win, 0, cur_x);
	wprint(stat_win, name_buf);
	cur_x += 22;
	if(x > 83)
		cur_x += x - 83;
	mvwaddstr(stat_win, 0, cur_x, size_buf);
	cur_x += 12;
	mvwaddstr(stat_win, 0, cur_x, perm_buf);
	cur_x += 11;

	snprintf(name_buf, sizeof(name_buf), "%d %s filtered", view->filtered,
			(view->filtered == 1) ? "file" : "files");
	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) + 2), name_buf);

	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		break_at(id_buf, ':');
	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		id_buf[0] = '\0';
	mvwaddstr(stat_win, 0, cur_x, id_buf);

	wrefresh(stat_win);
}

void
update_stat_window(FileView *view)
{
	int x;
	char *buf;

	if(!cfg.last_status)
		return;

	if(cfg.status_line[0] == '\0')
	{
		update_stat_window_old(view);
		return;
	}

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	buf = expand_status_line_macros(view, cfg.status_line);
	buf = break_in_two(buf, getmaxx(stdscr));

	werase(stat_win);
	wmove(stat_win, 0, 0);
	wprint(stat_win, buf);
	wrefresh(stat_win);

	free(buf);
}

static void
save_status_bar_msg(const char *msg)
{
	if(!curr_stats.save_msg_in_list)
		return;

	if(curr_stats.msg_tail != curr_stats.msg_head &&
			strcmp(curr_stats.msgs[curr_stats.msg_tail], msg) == 0)
		return;

	curr_stats.msg_tail = (curr_stats.msg_tail + 1) % ARRAY_LEN(curr_stats.msgs);
	if(curr_stats.msg_tail == curr_stats.msg_head)
		free(curr_stats.msgs[curr_stats.msg_head]);
	curr_stats.msgs[curr_stats.msg_tail] = strdup(msg);
}

static void
status_bar_message_i(const char *message, int error)
{
	static char *msg;
	static int err;

	int len;
	const char *p, *q;
	int lines;

	if(curr_stats.load_stage == 0)
		return;

	if(message != NULL)
	{
		char *p;

		if((p = strdup(message)) == NULL)
			return;

		free(msg);
		msg = p;
		err = error;

		save_status_bar_msg(msg);
	}

	if(msg == NULL)
		return;

	p = msg;
	q = msg - 1;
	status_bar_lines = 0;
	len = getmaxx(stdscr);
	while((q = strchr(q + 1, '\n')) != NULL)
	{
		status_bar_lines += (q - p + len - 1)/len;
		p = q + 1;
	}
	status_bar_lines += (strlen(p) + len - 1)/len;
	if(status_bar_lines == 0)
		status_bar_lines = 1;

	if(status_bar_lines > 1 || strlen(p) > getmaxx(status_bar))
		status_bar_lines++;
	lines = status_bar_lines;

	if(lines > getmaxy(stdscr))
		lines = getmaxy(stdscr);

	mvwin(stat_win, getmaxy(stdscr) - lines - 1, 0);
	mvwin(status_bar, getmaxy(stdscr) - lines, 0);
	if(lines == 1)
		wresize(status_bar, lines, getmaxx(stdscr) - 19);
	else
		wresize(status_bar, lines, getmaxx(stdscr));
	wmove(status_bar, 0, 0);

	if(err)
	{
		col_attr_t col = cfg.cs.color[CMD_LINE_COLOR];
		mix_colors(&col, &cfg.cs.color[ERROR_MSG_COLOR]);
		init_pair(DCOLOR_BASE + ERROR_MSG_COLOR, col.fg, col.bg);
		wattron(status_bar, COLOR_PAIR(DCOLOR_BASE + ERROR_MSG_COLOR) | col.attr);
	}
	else
	{
		int attr = cfg.cs.color[CMD_LINE_COLOR].attr;
		wattron(status_bar, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR) | attr);
	}
	werase(status_bar);

	wprint(status_bar, msg);
	if(lines > 1)
	{
		wmove(status_bar, lines - 1, 0);
		wclrtoeol(status_bar);
		if(lines < status_bar_lines)
			wprintw(status_bar, "%d of %d lines.  ", lines, status_bar_lines);
		wprintw(status_bar, "%s", "Press ENTER or type command to continue");
	}

	wattrset(status_bar, 0);
	wrefresh(status_bar);
	if(get_mode() != MENU_MODE && get_mode() != FILE_INFO_MODE && cfg.last_status)
		wrefresh(stat_win);
}

static void
vstatus_bar_messagef(int error, const char *format, va_list ap)
{
	char buf[1024];

	vsnprintf(buf, sizeof(buf), format, ap);
	status_bar_message_i(buf, error);
}

void
status_bar_error(const char *message)
{
	status_bar_message_i(message, 1);
}

void
status_bar_errorf(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);

	vstatus_bar_messagef(1, message, ap);

	va_end(ap);
}

void
status_bar_messagef(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vstatus_bar_messagef(0, format, ap);

	va_end(ap);
}

/*
 * Repeats last message if message is NULL
 */
void
status_bar_message(const char *message)
{
	status_bar_message_i(message, 0);
}

int
is_status_bar_multiline(void)
{
	return status_bar_lines > 1;
}

void
clean_status_bar(void)
{
	werase(status_bar);
	mvwin(stat_win, getmaxy(stdscr) - 2, 0);
	wresize(status_bar, 1, getmaxx(stdscr) - 19);
	mvwin(status_bar, getmaxy(stdscr) - 1, 0);
	wnoutrefresh(status_bar);

	if(status_bar_lines > 1)
	{
		status_bar_lines = 1;
		redraw_window();
	}
	status_bar_lines = 1;
}

int
setup_ncurses_interface(void)
{
	int screen_x, screen_y;
	int x, y;

	initscr();
	noecho();
	nonl();
	raw();

	curs_set(FALSE);

	getmaxyx(stdscr, screen_y, screen_x);
	/* screen is too small to be useful*/
	if(screen_y < MIN_TERM_HEIGHT)
		finish("Terminal is too small to run vifm\n");
	if(screen_x < MIN_TERM_WIDTH)
		finish("Terminal is too small to run vifm\n");

	if(!has_colors())
		finish("Vifm requires a console that can support color.\n");

	start_color();
	use_default_colors();

	load_def_scheme();

	werase(stdscr);

	menu_win = newwin(screen_y - 1, screen_x, 0, 0);
	wbkgdset(menu_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(menu_win, cfg.cs.color[WIN_COLOR].attr);
	werase(menu_win);

	sort_win = newwin(NUM_SORT_OPTIONS + 5, SORT_WIN_WIDTH,
			(screen_y - NUM_SORT_OPTIONS)/2, (screen_x - SORT_WIN_WIDTH)/2);
	wbkgdset(sort_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(sort_win, cfg.cs.color[WIN_COLOR].attr);
	werase(sort_win);

	change_win = newwin(20, 30, (screen_y - 20)/2, (screen_x -30)/2);
	wbkgdset(change_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(change_win, cfg.cs.color[WIN_COLOR].attr);
	werase(change_win);

	error_win = newwin(10, screen_x -2, (screen_y -10)/2, 1);
	wbkgdset(error_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(error_win, cfg.cs.color[WIN_COLOR].attr);
	werase(error_win);

	lborder = newwin(screen_y - 2, 1, 1, 0);
	wbkgdset(lborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	werase(lborder);

	if(curr_stats.number_of_windows == 1)
		lwin.title = newwin(1, screen_x - 2, 0, 1);
	else
		lwin.title = newwin(1, screen_x/2 - 2, 0, 1);

	wbkgdset(lwin.title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_SEL_COLOR) |
			(cfg.cs.color[TOP_LINE_SEL_COLOR].attr & A_REVERSE));
	wattrset(lwin.title, cfg.cs.color[TOP_LINE_SEL_COLOR].attr & ~A_REVERSE);
	werase(lwin.title);

	if(curr_stats.number_of_windows == 1)
		lwin.win = newwin(screen_y - 3, screen_x - 2, 1, 1);
	else
		lwin.win = newwin(screen_y - 3, screen_x/2 - 2 + screen_x%2, 1, 1);

	wbkgdset(lwin.win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(lwin.win, cfg.cs.color[WIN_COLOR].attr);
	werase(lwin.win);
	getmaxyx(lwin.win, y, x);
	lwin.window_rows = y - 1;
	lwin.window_width = x - 1;

	mborder = newwin(screen_y - 1, 2 - screen_x%2, 1,
			screen_x/2 - 1 + screen_x%2);
	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	werase(mborder);

	ltop_line1 = newwin(1, 1, 0, 0);
	wbkgdset(ltop_line1, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line1, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line1);

	ltop_line2 = newwin(1, 1, 0, 0);
	wbkgdset(ltop_line2, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line2, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line2);

	top_line = newwin(1, 2 - screen_x%2, 0, screen_x/2 - 1 + screen_x%2);
	wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(top_line);

	rtop_line1 = newwin(1, 1, 0, screen_x - 1);
	wbkgdset(rtop_line1, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line1, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line1);

	rtop_line2 = newwin(1, 1, 0, screen_x - 1);
	wbkgdset(rtop_line2, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line2, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line2);

	if(curr_stats.number_of_windows == 1)
		rwin.title = newwin(1, screen_x - 2, 0, 1);
	else
		rwin.title = newwin(1, screen_x/2 - 2 + screen_x%2, 0, screen_x/2 + 1);
	wbkgdset(rwin.title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) & A_REVERSE));
	wattrset(rwin.title, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rwin.title);

	if(curr_stats.number_of_windows == 1)
		rwin.win = newwin(screen_y - 3, screen_x - 2, 1, 1);
	else
		rwin.win = newwin(screen_y - 3, screen_x/2 - 2 + screen_x%2, 1,
				screen_x/2 + 1);

	wbkgdset(rwin.win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR));
	wattrset(rwin.win, cfg.cs.color[WIN_COLOR].attr);
	werase(rwin.win);
	getmaxyx(rwin.win, y, x);
	rwin.window_rows = y - 1;
	rwin.window_width = x - 1;

	rborder = newwin(screen_y - 2, 1, 1, screen_x - 1);
	wbkgdset(rborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	werase(rborder);

	stat_win = newwin(1, screen_x, screen_y - 2, 0);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);
	werase(stat_win);

	status_bar = newwin(1, screen_x - 19, screen_y -1, 0);
#ifdef ENABLE_EXTENDED_KEYS
	keypad(status_bar, TRUE);
#endif /* ENABLE_EXTENDED_KEYS */
	wattrset(status_bar, cfg.cs.color[CMD_LINE_COLOR].attr);
	wbkgdset(status_bar, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR));
	werase(status_bar);

	pos_win = newwin(1, 13, screen_y - 1, screen_x - 13);
	wattrset(pos_win, cfg.cs.color[CMD_LINE_COLOR].attr);
	wbkgdset(pos_win, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR));
	werase(pos_win);

	input_win = newwin(1, 6, screen_y - 1, screen_x -19);
	wattrset(input_win, cfg.cs.color[CMD_LINE_COLOR].attr);
	wbkgdset(input_win, COLOR_PAIR(DCOLOR_BASE + CMD_LINE_COLOR));
	werase(input_win);

	cfg.tab_stop = TABSIZE;

#ifdef NCURSES_EXT_FUNCS
	set_escdelay(0);
#endif /* NCURSES_EXT_FUNCS */

	return 1;
}

void
is_term_working(void)
{
#ifndef _WIN32
	int screen_x, screen_y;
	struct winsize ws = { .ws_col = -1, .ws_row = -1 };

	if(ioctl(0, TIOCGWINSZ, &ws) == -1)
		finish("Terminal error");
	if(ws.ws_row < 0)
		finish("Terminal is too small to run vifm\n");
	if(ws.ws_col < 0)
		finish("Terminal is too small to run vifm\n");

	resize_term(ws.ws_row, ws.ws_col);

	getmaxyx(stdscr, screen_y, screen_x);

	if(screen_y < MIN_TERM_HEIGHT || screen_x < MIN_TERM_WIDTH)
		curr_stats.too_small_term = 1;
	else if(curr_stats.too_small_term)
		curr_stats.too_small_term = -1;
#endif
}

static void
correct_size(FileView *view)
{
	int x, y;

	getmaxyx(view->win, y, x);
	view->window_width = x - 1;
	view->window_rows = y - 1;
}

static void
only_layout(FileView *view, int screen_x, int screen_y)
{
	wresize(view->title, 1, screen_x - 2);
	mvwin(view->title, 0, 1);

	wresize(view->win, screen_y - 3 + !cfg.last_status, screen_x - 2);
	mvwin(view->win, 1, 1);
}

static void
vertical_layout(int screen_x, int screen_y)
{
	int splitter_pos;
	int splitter_width;

	if(curr_stats.splitter_pos < 0)
		splitter_pos = screen_x/2 - 1 + screen_x%2;
	else
		splitter_pos = curr_stats.splitter_pos;

	splitter_width = 2 - screen_x%2;
	if(splitter_pos < 4)
		splitter_pos = 4;
	if(splitter_pos > screen_x - 4 - splitter_width)
		splitter_pos = screen_x - 4 - splitter_width;
	if(curr_stats.splitter_pos >= 0)
		curr_stats.splitter_pos = splitter_pos;

	wresize(lwin.title, 1, splitter_pos - 1);
	wresize(lwin.win, screen_y - 3 + !cfg.last_status, splitter_pos - 1);
	mvwin(lwin.win, 1, 1);

	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	wresize(mborder, screen_y - 2, splitter_width);
	mvwin(mborder, 1, splitter_pos);

	mvwin(ltop_line1, 0, 0);
	mvwin(ltop_line2, 0, 0);

	wresize(top_line, 1, splitter_width);
	mvwin(top_line, 0, splitter_pos);

	mvwin(rtop_line1, 0, screen_x - 1);
	mvwin(rtop_line2, 0, screen_x - 1);

	wresize(rwin.title, 1, screen_x - (splitter_pos + splitter_width + 1));
	mvwin(rwin.title, 0, splitter_pos + splitter_width);

	wresize(rwin.win, screen_y - 3 + !cfg.last_status,
			screen_x - (splitter_pos + splitter_width + 1));
	mvwin(rwin.win, 1, splitter_pos + splitter_width);
}

float
get_splitter_pos(int max)
{
	if(curr_stats.split == HSPLIT)
	{
		return (curr_stats.splitter_pos*max)/100;
	}
	else
	{
		if(curr_stats.splitter_pos == 50.f)
			return max/2 - 1 + max%2;
		else
			return (curr_stats.splitter_pos*max)/100;
	}
}

static void
horizontal_layout(int screen_x, int screen_y)
{
	int splitter_pos;

	if(curr_stats.splitter_pos < 0)
		splitter_pos = screen_y/2 - 1;
	else
		splitter_pos = curr_stats.splitter_pos;
	if(splitter_pos < 2)
		splitter_pos = 2;
	if(splitter_pos > screen_y - 3 - cfg.last_status - 1)
		splitter_pos = screen_y - 3 - cfg.last_status;
	if(curr_stats.splitter_pos >= 0)
		curr_stats.splitter_pos = splitter_pos;

	wresize(lwin.title, 1, screen_x - 2);
	mvwin(lwin.title, 0, 1);

	wresize(rwin.title, 1, screen_x - 2);
	mvwin(rwin.title, splitter_pos, 1);

	wresize(lwin.win, splitter_pos - 1, screen_x - 2);
	mvwin(lwin.win, 1, 1);

	wresize(rwin.win, screen_y - splitter_pos - 1 - cfg.last_status - 1,
			screen_x - 2);
	mvwin(rwin.win, splitter_pos + 1, 1);

	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	wresize(mborder, 1, screen_x);
	mvwin(mborder, splitter_pos, 0);

	mvwin(ltop_line1, 0, 0);
	mvwin(ltop_line2, splitter_pos, 0);

	wresize(top_line, 1, 2 - screen_x%2);
	mvwin(top_line, 0, screen_x/2 - 1 + screen_x%2);

	mvwin(rtop_line1, 0, screen_x - 1);
	mvwin(rtop_line2, splitter_pos, screen_x - 1);

	wresize(lborder, screen_y - 1, 1);
	mvwin(lborder, 0, 0);

	wresize(rborder, screen_y - 1, 1);
	mvwin(rborder, 0, screen_x - 1);
}

static void
resize_all(void)
{
	static float prev_x = -1.f, prev_y = -1.f;
	int screen_x, screen_y;
#ifndef _WIN32
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	LOG_INFO_MSG("ws.ws_row = %d; ws.ws_col = %d", ws.ws_row, ws.ws_col);

	/* changed for pdcurses */
	resize_term(ws.ws_row, ws.ws_col);
#endif

	getmaxyx(stdscr, screen_y, screen_x);
	cfg.lines = screen_y;
	cfg.columns = screen_x;

	LOG_INFO_MSG("screen_y = %d; screen_x = %d", screen_y, screen_x);

	if(screen_y < MIN_TERM_HEIGHT || screen_x < MIN_TERM_WIDTH)
	{
		curr_stats.too_small_term = 1;
		return;
	}
	else if(curr_stats.too_small_term)
	{
		curr_stats.too_small_term = -1;
		return;
	}

	if(prev_x < 0)
	{
		prev_x = screen_x;
		prev_y = screen_y;
	}

	if(curr_stats.splitter_pos >= 0)
	{
		if(curr_stats.split == HSPLIT)
			curr_stats.splitter_pos *= screen_y/prev_y;
		else
			curr_stats.splitter_pos *= screen_x/prev_x;
	}

	prev_x = screen_x;
	prev_y = screen_y;

	wclear(stdscr);
	wclear(mborder);
	wclear(ltop_line1);
	wclear(ltop_line2);
	wclear(top_line);
	wclear(rtop_line1);
	wclear(rtop_line2);
	wclear(lwin.title);
	wclear(lwin.win);
	wclear(rwin.title);
	wclear(rwin.win);
	wclear(stat_win);
	wclear(pos_win);
	wclear(input_win);
	wclear(rborder);
	wclear(lborder);
	wclear(status_bar);

	wresize(stdscr, screen_y, screen_x);
	wresize(menu_win, screen_y - 1, screen_x);
	wresize(error_win, (screen_y - 10)/2, screen_x - 2);
	mvwin(error_win, (screen_y - 10)/2, 1);

	wbkgdset(lborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	wresize(lborder, screen_y - 2, 1);
	mvwin(lborder, 1, 0);

	wbkgdset(rborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	wresize(rborder, screen_y - 2, 1);
	mvwin(rborder, 1, screen_x - 1);

	if(curr_stats.number_of_windows == 1)
	{
		only_layout(&lwin, screen_x, screen_y);
		only_layout(&rwin, screen_x, screen_y);

		mvwin(ltop_line1, 0, 0);
		mvwin(ltop_line2, 0, 0);
		mvwin(rtop_line1, 0, screen_x - 1);
		mvwin(rtop_line2, 0, screen_x - 1);
	}
	else
	{
		if(curr_stats.split == HSPLIT)
			horizontal_layout(screen_x, screen_y);
		else
			vertical_layout(screen_x, screen_y);
	}

	correct_size(&lwin);
	correct_size(&rwin);

	wresize(stat_win, 1, screen_x);
	mvwin(stat_win, screen_y - 2, 0);
	wresize(status_bar, 1, screen_x - 19);

#ifdef ENABLE_EXTENDED_KEYS
	/* For FreeBSD */
	keypad(status_bar, TRUE);
#endif /* ENABLE_EXTENDED_KEYS */

	mvwin(status_bar, screen_y - 1, 0);
	wresize(pos_win, 1, 13);
	mvwin(pos_win, screen_y - 1, screen_x - 13);

	wresize(input_win, 1, 6);
	mvwin(input_win, screen_y - 1, screen_x - 19);

	curs_set(FALSE);
}

void
redraw_window(void)
{
	if(curr_stats.load_stage < 2)
		return;

	resize_all();
	update_attributes();
	werase(lborder);
	werase(mborder);
	werase(rborder);

	if(curr_stats.show_full)
	{
		redraw_file_info_dialog();
		curr_stats.need_redraw = curr_stats.too_small_term;
		return;
	}

	if(curr_stats.too_small_term)
		return;

	reload_view(curr_view);

	update_view_title(other_view);
	if(curr_stats.view)
		quick_view_file(curr_view);
	else if(!other_view->explore_mode)
		reload_view(other_view);

	update_stat_window(curr_view);

	if(!is_status_bar_multiline())
	{
		if(curr_view->selected_files)
			print_selected_msg();
		else
			clean_status_bar();

		update_pos_window(curr_view);
	}

	if(curr_stats.save_msg == 0)
		status_bar_message("");

	update_all_windows();

	if(!curr_view->explore_mode)
		move_to_list_pos(curr_view, curr_view->list_pos);
	curr_stats.need_redraw = 0;

	if(curr_stats.errmsg_shown)
	{
		redraw_error_msg(NULL, NULL);
		redrawwin(error_win);
		wnoutrefresh(error_win);
		doupdate();
	}

	update_input_buf();
	
	if(get_mode() == VIEW_MODE || lwin.explore_mode || rwin.explore_mode)
		view_redraw();
}

/* reloads view on window_reload() call */
static void
reload_view(FileView *view)
{
	if(curr_stats.load_stage >= 3)
		load_saving_pos(view, 1);
	else
		load_dir_list(view,
				!(cfg.vifm_info&VIFMINFO_SAVEDIRS) || view->list_pos != 0);
}

static void
switch_views(void)
{
	FileView *tmp = curr_view;
	curr_view = other_view;
	other_view = tmp;

	load_local_options(curr_view);
}

void
change_window(void)
{
	switch_views();

	if(curr_stats.number_of_windows != 1)
	{
		if(!other_view->explore_mode)
		{
			mvwaddstr(other_view->win, other_view->curr_line, 0, "*");
			erase_current_line_bar(other_view);
		}
		update_view_title(other_view);
	}

	if(curr_stats.view)
	{
		if(change_directory(other_view, other_view->curr_dir) >= 0)
			load_dir_list(other_view, 1);
		if(change_directory(curr_view, curr_view->curr_dir) >= 0)
			load_dir_list(curr_view, 1);
	}

	update_view_title(curr_view);

	wnoutrefresh(other_view->win);
	wnoutrefresh(curr_view->win);

	(void)change_directory(curr_view, curr_view->curr_dir);

	if(curr_stats.number_of_windows == 1)
		load_dir_list(curr_view, 1);

	if(!curr_view->explore_mode)
	{
		draw_dir_list(curr_view, curr_view->top_line);
		move_to_list_pos(curr_view, curr_view->list_pos);
	}
	werase(status_bar);
	wnoutrefresh(status_bar);

	if(curr_stats.number_of_windows == 1)
		update_all_windows();
}

static void
update_view(FileView *win)
{
	touchwin(win->title);
	touchwin(win->win);

	redrawwin(win->title);
	redrawwin(win->win);

	wnoutrefresh(win->title);
	wnoutrefresh(win->win);
}

void
update_all_windows(void)
{
	if(curr_stats.load_stage < 2)
		return;

	touchwin(lborder);
	if(cfg.last_status)
		touchwin(stat_win);
	touchwin(pos_win);
	touchwin(input_win);
	touchwin(rborder);
	touchwin(status_bar);

	/*
	 * redrawwin() shouldn't be needed.  But without it there is a
	 * lot of flickering when redrawing the windows?
	 */

	redrawwin(lborder);
	if(cfg.last_status)
		redrawwin(stat_win);
	redrawwin(pos_win);
	redrawwin(input_win);
	redrawwin(rborder);
	redrawwin(status_bar);

	/* In One window view */
	if(curr_stats.number_of_windows == 1)
	{
		update_view(curr_view);
	}
	/* Two Pane View */
	else
	{
		touchwin(mborder);
		redrawwin(mborder);
		wnoutrefresh(mborder);

		touchwin(top_line);
		redrawwin(top_line);
		wnoutrefresh(top_line);

		update_view(&lwin);
		update_view(&rwin);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(rborder);

	touchwin(ltop_line1);
	redrawwin(ltop_line1);
	wnoutrefresh(ltop_line1);

	touchwin(ltop_line2);
	redrawwin(ltop_line2);
	wnoutrefresh(ltop_line2);

	touchwin(rtop_line1);
	redrawwin(rtop_line1);
	wnoutrefresh(rtop_line1);

	touchwin(rtop_line2);
	redrawwin(rtop_line2);
	wnoutrefresh(rtop_line2);

	if(cfg.last_status)
		wnoutrefresh(stat_win);
	wnoutrefresh(pos_win);
	wnoutrefresh(input_win);
	wnoutrefresh(status_bar);

	if(!curr_stats.errmsg_shown && curr_stats.load_stage >= 2)
		doupdate();
}

void
update_input_bar(wchar_t *str)
{
	if(!curr_stats.use_input_bar)
		return;

	if(wcslen(str) > getmaxx(input_win))
	{
		str += wcslen(str) - getmaxx(input_win);
	}

	werase(input_win);
	waddwstr(input_win, str);
	wrefresh(input_win);
}

void
clear_num_window(void)
{
	if(curr_stats.use_input_bar)
	{
		werase(input_win);
		wrefresh(input_win);
	}
}

/* msg can't be NULL
 * period - how often status bar should be updated
 * if period equals 0 reset inner counter
 */
void
show_progress(const char *msg, int period)
{
	static char marks[] = {'|', '/', '-', '\\'};
	static int count = 0;
	static int pause = 1;

	if(period == 0)
	{
		pause = 1;
		return;
	}

	pause++;

	if(pause%period != 0)
		return;

	pause = 1;

	status_bar_messagef("%s %c", msg, marks[count]);

	count = (count + 1) % sizeof(marks);
}

void
redraw_lists(void)
{
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
	if(curr_stats.view)
		quick_view_file(curr_view);
	else
		draw_dir_list(other_view, other_view->top_line);
}

int
load_color_scheme(const char *name)
{
	char full[PATH_MAX];

	if(!find_color_scheme(name))
	{
		show_error_msgf("Color Scheme", "Invalid color scheme name: \"%s\"", name);
		return 0;
	}

	curr_stats.cs_base = DCOLOR_BASE;
	curr_stats.cs = &cfg.cs;
	cfg.cs.defaulted = 0;

	snprintf(full, sizeof(full), "%s/colors/%s", cfg.config_dir, name);
	(void)source_file(full);
	strcpy(cfg.cs.name, name);
	check_color_scheme(&cfg.cs);

	update_attributes();

	if(curr_stats.load_stage < 2)
		return 0;

	if(cfg.cs.defaulted)
	{
		load_color_scheme_colors();
		show_error_msg("Color Scheme Error", "Not supported by the terminal");
		return 1;
	}
	return 0;
}

static void
update_attributes(void)
{
	int attr;

	if(curr_stats.load_stage < 2)
		return;

	attr = cfg.cs.color[BORDER_COLOR].attr;
	wbkgdset(lborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(lborder);
	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(mborder);
	wbkgdset(rborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(rborder);

	wbkgdset(ltop_line1, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line1, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line1);

	wbkgdset(ltop_line2, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line2, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line2);

	wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(top_line);

	wbkgdset(rtop_line1, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line1, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line1);

	wbkgdset(rtop_line2, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line2, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line2);

	attr = cfg.cs.color[STATUS_LINE_COLOR].attr;
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) | attr);

	attr = cfg.cs.color[WIN_COLOR].attr;
	wbkgdset(menu_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(sort_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(change_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(error_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
}

void
wprint(WINDOW *win, const char *str)
{
#ifndef _WIN32
	waddstr(win, str);
#else
	wchar_t *t = to_wide(str);
	if(t == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	waddwstr(win, t);
	free(t);
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
