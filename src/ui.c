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
#include <signal.h> /* signal() */
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../config.h"

#include "color_scheme.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "macros.h"
#include "main_loop.h"
#include "menus.h"
#include "modes.h"
#include "opt_handlers.h"
#include "signals.h"
#include "status.h"
#include "utils.h"

#include "ui.h"

static int status_bar_lines;

static void _gnuc_noreturn
finish(const char *message)
{
	endwin();
	write_info_file();
	printf("%s", message);
	exit(0);
}

void
update_pos_window(FileView *view)
{
	char buf[13];

	werase(pos_win);
	snprintf(buf, sizeof(buf), "%d-%d ", view->list_pos + 1, view->list_rows);
	mvwaddstr(pos_win, 0, 13 - strlen(buf), buf);
	wnoutrefresh(pos_win);
}

static void
get_id_string(FileView *view, size_t len, char *out_buf)
{
#ifndef _WIN32
	char buf[MAX(sysconf(_SC_GETPW_R_SIZE_MAX), sysconf(_SC_GETGR_R_SIZE_MAX))];
	char uid_buf[26];
	char gid_buf[26];
	struct passwd pwd_b;
	struct group group_b;
	struct passwd *pwd_buf;
	struct group *group_buf;

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

	snprintf(out_buf, len, "  %s:%s", uid_buf, gid_buf);
#else
	out_buf[0] = '\0';
#endif
}

void
update_stat_window(FileView *view)
{
	char name_buf[160*2 + 1];
	char perm_buf[26];
	char size_buf[56];
	char id_buf[52];
	int x, y;
	int cur_x;
	size_t print_width;
	char *filename;

	getmaxyx(stdscr, y, x);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	filename = get_current_file_name(view);
	print_width = get_real_string_width(filename, 20 + MAX(0, x - 83));
	snprintf(name_buf, MIN(sizeof(name_buf), print_width + 1), "%s", filename);
	friendly_size_notation(view->dir_entry[view->list_pos].size, sizeof(size_buf),
			size_buf);

	get_id_string(view, sizeof(id_buf), id_buf);
	get_perm_string(perm_buf, sizeof(perm_buf),
			view->dir_entry[view->list_pos].mode);

	werase(stat_win);
	cur_x = 2;
	mvwaddstr(stat_win, 0, cur_x, name_buf);
	cur_x += 22;
	if(x > 83)
		cur_x += x - 83;
	mvwaddstr(stat_win, 0, cur_x, size_buf);
	cur_x += 12;
	mvwaddstr(stat_win, 0, cur_x, perm_buf);
	cur_x += 10;

	snprintf(name_buf, sizeof(name_buf), "%d %s filtered", view->filtered,
			(view->filtered == 1) ? "file" : "files");
	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) + 2), name_buf);

	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		*strchr(id_buf, ':') = '\0';
	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		id_buf[0] = '\0';
	mvwaddstr(stat_win, 0, cur_x, id_buf);

	wrefresh(stat_win);
}

static void
status_bar_message_i(const char *message, int error)
{
	static char *msg;
	static int err;

	int len;
	const char *p, *q;
	int lines;

	if(curr_stats.vifm_started == 0)
		return;

	if(message != NULL)
	{
		char *p;

		if((p = strdup(message)) == NULL)
			return;

		free(msg);
		msg = p;
		err = error;
	}

	assert(msg != NULL);

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

	mvwin(stat_win, getmaxy(stdscr) - lines - 1, 0);
	mvwin(status_bar, getmaxy(stdscr) - lines, 0);
	if(lines == 1)
		wresize(status_bar, lines, getmaxx(stdscr) - 19);
	else
		wresize(status_bar, lines, getmaxx(stdscr));
	wmove(status_bar, 0, 0);

	if(err)
	{
		Col_attr col = cfg.cs.color[CMD_LINE_COLOR];
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

	wprintw(status_bar, "%s", msg);
	if(lines > 1)
		wprintw(status_bar, "%s", "\nPress ENTER or type command to continue");

	wattrset(status_bar, 0);
	wrefresh(status_bar);
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
	mvwin(status_bar, getmaxy(stdscr) - 1, 0);
	wresize(status_bar, 1, getmaxx(stdscr) - 19);
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
	if(screen_y < 10)
		finish("Terminal is too small to run vifm\n");
	if(screen_x < 30)
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

	sort_win = newwin(NUM_SORT_OPTIONS + 3, 30, (screen_y - 12)/2,
			(screen_x - 30)/2);
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

	lborder = newwin(screen_y - 3, 1, 1, 0);
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
	lwin.window_rows = y -1;
	lwin.window_width = x -1;

	mborder = newwin(screen_y - 1, 2 - screen_x%2, 1,
			screen_x/2 - 1 + screen_x%2);
	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	werase(mborder);

	ltop_line = newwin(1, 1, 0, 0);
	wbkgdset(ltop_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line);

	top_line = newwin(1, 2 - screen_x%2, 0, screen_x/2 - 1 + screen_x%2);
	wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(top_line);

	rtop_line = newwin(1, 1, 0, screen_x - 1);
	wbkgdset(rtop_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line);

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

	rborder = newwin(screen_y - 3, 1, 1, screen_x - 1);
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

	wnoutrefresh(mborder);
	wnoutrefresh(ltop_line);
	wnoutrefresh(top_line);
	wnoutrefresh(rtop_line);
	wnoutrefresh(lwin.title);
	wnoutrefresh(lwin.win);
	wnoutrefresh(rwin.win);
	wnoutrefresh(rwin.title);
	wnoutrefresh(stat_win);
	wnoutrefresh(pos_win);
	wnoutrefresh(input_win);
	wnoutrefresh(lborder);
	wnoutrefresh(rborder);
	wnoutrefresh(status_bar);

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

	if(screen_y < 10 || screen_x < 30)
		curr_stats.too_small_term = 1;
	else if(curr_stats.too_small_term)
		curr_stats.too_small_term = -1;
#endif
}

static void
resize_all(void)
{
	int screen_x, screen_y;
	int x, y;
#ifndef _WIN32
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);

	/* changed for pdcurses */
	resize_term(ws.ws_row, ws.ws_col);
#endif

	getmaxyx(stdscr, screen_y, screen_x);

	if(screen_y < 10 || screen_x < 30)
	{
		curr_stats.too_small_term = 1;
		return;
	}
	else if(curr_stats.too_small_term)
	{
		curr_stats.too_small_term = -1;
		return;
	}

	wclear(stdscr);
	wclear(mborder);
	wclear(ltop_line);
	wclear(top_line);
	wclear(rtop_line);
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
	mvwin(lborder, 1, 0);
	wresize(lborder, screen_y - 3, 1);

	if(curr_stats.number_of_windows == 1)
	{
		wresize(lwin.title, 1, screen_x - 2);
		wresize(lwin.win, screen_y - 3, screen_x - 2);
		getmaxyx(lwin.win, y, x);
		mvwin(lwin.win, 1, 1);
		lwin.window_width = x - 1;
		lwin.window_rows = y - 1;

		wresize(rwin.title, 1, screen_x - 2);
		mvwin(rwin.title, 0, 1);
		wresize(rwin.win, screen_y - 3, screen_x - 2);
		mvwin(rwin.win, 1, 1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x - 1;
		rwin.window_rows = y - 1;
	}
	else
	{
		wresize(lwin.title, 1, screen_x/2 - 2 + screen_x%2);
		wresize(lwin.win, screen_y - 3, screen_x/2 - 2 + screen_x%2);
		mvwin(lwin.win, 1, 1);
		getmaxyx(lwin.win, y, x);
		lwin.window_width = x - 1;
		lwin.window_rows = y - 1;

		wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
				cfg.cs.color[BORDER_COLOR].attr);
		mvwin(mborder, 1, screen_x/2 - 1 + screen_x%2);
		wresize(mborder, screen_y - 3, 2 - screen_x%2);

		mvwin(top_line, 0, screen_x/2 - 1 + screen_x%2);
		wresize(top_line, 1, 2 - screen_x%2);

		mvwin(rtop_line, 0, screen_x - 1);

		wresize(rwin.title, 1, screen_x/2 - 2 + screen_x%2);
		mvwin(rwin.title, 0, screen_x/2 + 1);

		wresize(rwin.win, screen_y - 3, screen_x/2 - 2 + screen_x%2);
		mvwin(rwin.win, 1, screen_x/2 + 1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x - 1;
		rwin.window_rows = y - 1;
	}

	wbkgdset(rborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) |
			cfg.cs.color[BORDER_COLOR].attr);
	wresize(rborder, screen_y - 3, 1);
	mvwin(rborder, 1, screen_x - 1);

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
	resize_all();
	werase(lborder);
	werase(mborder);
	werase(rborder);

	if(curr_stats.show_full)
	{
		redraw_full_file_properties(NULL);
		curr_stats.need_redraw = curr_stats.too_small_term;
		return;
	}

	if(curr_stats.too_small_term)
		return;

	load_saving_pos(curr_view, 1);

	if(curr_stats.view)
		quick_view_file(curr_view);
	else
		load_saving_pos(other_view, 1);

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
		mvwaddstr(other_view->win, other_view->curr_line, 0, "*");
		erase_current_line_bar(other_view);
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

	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
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
	touchwin(lborder);
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

		touchwin(ltop_line);
		redrawwin(ltop_line);
		wnoutrefresh(ltop_line);

		touchwin(top_line);
		redrawwin(top_line);
		wnoutrefresh(top_line);

		touchwin(rtop_line);
		redrawwin(rtop_line);
		wnoutrefresh(rtop_line);

		update_view(&lwin);
		update_view(&rwin);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(rborder);
	wnoutrefresh(stat_win);
	wnoutrefresh(pos_win);
	wnoutrefresh(input_win);
	wnoutrefresh(status_bar);

	if(!curr_stats.errmsg_shown && curr_stats.vifm_started >= 2)
		doupdate();
}

void
update_input_bar(wchar_t *str)
{
	if(!curr_stats.use_input_bar)
		return;

	if(getcurx(input_win) >= getmaxx(input_win) - wcslen(str))
	{
		mvwdelch(input_win, 0, 0);
		wmove(input_win, 0, getmaxx(input_win) - wcslen(str) - 1);
	}

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
	int attr;

	if(!find_color_scheme(name))
	{
		show_error_msgf("Color Scheme", "Invalid color scheme name: \"%s\"", name);
		return 0;
	}

	curr_stats.cs_base = DCOLOR_BASE;
	curr_stats.cs = &cfg.cs;
	cfg.cs.defaulted = 0;

	snprintf(full, sizeof(full), "%s/colors/%s", cfg.config_dir, name);
	source_file(full);
	strcpy(cfg.cs.name, name);
	check_color_scheme(&cfg.cs);

	attr = cfg.cs.color[BORDER_COLOR].attr;
	wbkgdset(lborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(lborder);
	wbkgdset(mborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(mborder);
	wbkgdset(rborder, COLOR_PAIR(DCOLOR_BASE + BORDER_COLOR) | attr);
	werase(rborder);

	wbkgdset(ltop_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(ltop_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(ltop_line);

	wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(top_line);

	wbkgdset(rtop_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
			(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
	wattrset(rtop_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
	werase(rtop_line);

	attr = cfg.cs.color[STATUS_LINE_COLOR].attr;
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) | attr);

	attr = cfg.cs.color[WIN_COLOR].attr;
	wbkgdset(menu_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(sort_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(change_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);
	wbkgdset(error_win, COLOR_PAIR(DCOLOR_BASE + WIN_COLOR) | attr);

	if(curr_stats.vifm_started < 2)
		return 0;

	if(cfg.cs.defaulted)
	{
		load_color_schemes();
		show_error_msg("Color Scheme Error", "Not supported by the terminal");
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
