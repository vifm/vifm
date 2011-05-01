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

#include <grp.h> /* getgrgid() */
#include <signal.h> /* signal() */
#include <stdlib.h> /* malloc */
#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR */
#include <pwd.h> /* getpwent() */
#include <string.h>
#include <time.h>
#include <termios.h> /* struct winsize */
#include <sys/ioctl.h>

#include "color_scheme.h"
#include "config.h" /* for menu colors */
#include "file_info.h"
#include "filelist.h"
#include "menus.h"
#include "signals.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

static void
finish(char *message)
{
	endwin();
	write_config_file();
	system("clear");
	printf("%s", message);
	exit(0);
}

void
update_pos_window(FileView *view)
{
	char buf[13];

	if(curr_stats.freeze)
		return;
	werase(pos_win);
	snprintf(buf, sizeof(buf), "%d-%d ", view->list_pos + 1, view->list_rows);
	mvwaddstr(pos_win, 0, 13 - strlen(buf), buf);
	wnoutrefresh(pos_win);
}

void
write_stat_win(char *message)
{
	werase(stat_win);
	wprintw(stat_win, "%s", message);
	wnoutrefresh(stat_win);
}

static void
get_id_string(FileView *view, size_t len, char *buf)
{
	char uid_buf[26];
	char gid_buf[26];
	struct passwd *pwd_buf;
	struct group *group_buf;

	if((pwd_buf = getpwuid(view->dir_entry[view->list_pos].uid)) == NULL)
	{
		snprintf(uid_buf, sizeof(uid_buf), "%d",
				(int) view->dir_entry[view->list_pos].uid);
	}
	else
	{
		snprintf(uid_buf, sizeof(uid_buf), "%s", pwd_buf->pw_name);
	}

	if((group_buf = getgrgid(view->dir_entry[view->list_pos].gid)) == NULL)
	{
		snprintf(gid_buf, sizeof(gid_buf), "%d",
				(int) view->dir_entry[view->list_pos].gid);
	}
	else
	{
		snprintf(gid_buf, sizeof(gid_buf), "%s", group_buf->gr_name);
	}

	snprintf(buf, len, "  %s:%s", uid_buf, gid_buf);
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
	char *current_file;

	getmaxyx(stat_win, y, x);
	current_file = get_current_file_name(view);
	print_width = get_real_string_width(current_file, 20 + MAX(0, x - 83));
	snprintf(name_buf, MIN(sizeof(name_buf), print_width + 1), "%s",
			current_file);
	friendly_size_notation(view->dir_entry[view->list_pos].size,
			sizeof(size_buf), size_buf);

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
	cur_x += 9;
	mvwaddstr(stat_win, 0, cur_x, id_buf);
	snprintf(name_buf, sizeof(name_buf), "%d %s filtered",
			view->filtered, view->filtered == 1 ? "file" : "files");

	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) +2) , name_buf);

	wnoutrefresh(stat_win);
	update_pos_window(view);
}

void
status_bar_message(char *message)
{
	werase(status_bar);
	wprintw(status_bar, "%s", message);
	wnoutrefresh(status_bar);
}

int
setup_ncurses_interface()
{
	int screen_x, screen_y;
	int i, x, y;

	initscr();
	noecho();
	nonl();
	raw();

	getmaxyx(stdscr, screen_y, screen_x);
	/* screen is too small to be useful*/
	if(screen_y < 10)
		finish("Terminal is too small to run vifm\n");
	if(screen_x < 30)
		finish("Terminal is too small to run vifm\n");

	if(! has_colors())
		finish("Vifm requires a console that can support color.\n");

	start_color();
	// Changed for pdcurses
	use_default_colors();

	x = 0;

	for (i = 0; i < cfg.color_scheme_num; i++)
	{
		for(x = 0; x < 12; x++)
			init_pair(col_schemes[i].color[x].name,
				col_schemes[i].color[x].fg, col_schemes[i].color[x].bg);
	}

	werase(stdscr);

	menu_win = newwin(screen_y - 1, screen_x , 0, 0);
	wbkgdset(menu_win, COLOR_PAIR(WIN_COLOR));
	werase(menu_win);

	sort_win = newwin(NUM_SORT_OPTIONS + 3, 30, (screen_y -12)/2, (screen_x -30)/2);
	wbkgdset(sort_win, COLOR_PAIR(WIN_COLOR));
	werase(sort_win);

	change_win = newwin(20, 30, (screen_y -20)/2, (screen_x -30)/2);
	wbkgdset(change_win, COLOR_PAIR(WIN_COLOR));
	werase(change_win);

	error_win = newwin(10, screen_x -2, (screen_y -10)/2, 1);
	wbkgdset(error_win, COLOR_PAIR(WIN_COLOR));
	werase(error_win);

	lborder = newwin(screen_y - 2, 1, 0, 0);

	wbkgdset(lborder, COLOR_PAIR(BORDER_COLOR));

	werase(lborder);

	if (curr_stats.number_of_windows == 1)
		lwin.title = newwin(0, screen_x -2, 0, 1);
	else
		lwin.title = newwin(1, screen_x/2 -1, 0, 1);

	wattrset(lwin.title, A_BOLD);
	wbkgdset(lwin.title, COLOR_PAIR(BORDER_COLOR));

	werase(lwin.title);

	if (curr_stats.number_of_windows == 1)
		lwin.win = newwin(screen_y - 3, screen_x -2, 1, 1);
	else
		lwin.win = newwin(screen_y - 3, screen_x/2 -2, 1, 1);

	keypad(lwin.win, TRUE);
	wbkgdset(lwin.win, COLOR_PAIR(WIN_COLOR));
	wattrset(lwin.win, A_BOLD);
	wattron(lwin.win, A_BOLD);
	werase(lwin.win);
	getmaxyx(lwin.win, y, x);
	lwin.window_rows = y -1;
	lwin.window_width = x -1;

	mborder = newwin(screen_y, 2, 0, screen_x/2 -1);

	wbkgdset(mborder, COLOR_PAIR(BORDER_COLOR));

	werase(mborder);

	if (curr_stats.number_of_windows == 1)
		rwin.title = newwin(0, screen_x -2	, 0, 1);
	else
		rwin.title = newwin(1, screen_x/2 -1  , 0, screen_x/2 +1);

	wbkgdset(rwin.title, COLOR_PAIR(BORDER_COLOR));
	wattrset(rwin.title, A_BOLD);
	wattroff(rwin.title, A_BOLD);

	werase(rwin.title);

	if (curr_stats.number_of_windows == 1)
		rwin.win = newwin(screen_y - 3, screen_x -2 , 1, 1);
	else
		rwin.win = newwin(screen_y - 3, screen_x/2 -2 , 1, screen_x/2 +1);

	keypad(rwin.win, TRUE);
	wattrset(rwin.win, A_BOLD);
	wattron(rwin.win, A_BOLD);
	wbkgdset(rwin.win, COLOR_PAIR(WIN_COLOR));
	werase(rwin.win);
	getmaxyx(rwin.win, y, x);
	rwin.window_rows = y - 1;
	rwin.window_width = x -1;

	if (screen_x % 2)
	{
		rborder = newwin(screen_y - 2, 2, 0, screen_x -2);
	}
	else
	{
		rborder = newwin(screen_y - 2, 1, 0, screen_x -1);
	}

	wbkgdset(rborder, COLOR_PAIR(BORDER_COLOR));

	werase(rborder);

	stat_win = newwin(1, screen_x, screen_y -2, 0);

	wbkgdset(stat_win, COLOR_PAIR(BORDER_COLOR));

	werase(stat_win);

	status_bar = newwin(1, screen_x - 19, screen_y -1, 0);
	keypad(status_bar, TRUE);
	wattrset(status_bar, A_BOLD);
	wattron(status_bar, A_BOLD);
	wbkgdset(status_bar, COLOR_PAIR(STATUS_BAR_COLOR));
	werase(status_bar);

	pos_win = newwin(1, 13, screen_y - 1, screen_x -13);
	wattrset(pos_win, A_BOLD);
	wattron(pos_win, A_BOLD);
	wbkgdset(pos_win, COLOR_PAIR(STATUS_BAR_COLOR));
	werase(pos_win);

	num_win = newwin(1, 6, screen_y - 1, screen_x -19);
	wattrset(num_win, A_BOLD);
	wattron(num_win, A_BOLD);
	wbkgdset(num_win, COLOR_PAIR(STATUS_BAR_COLOR));
	werase(num_win);


	wnoutrefresh(lwin.title);
	wnoutrefresh(lwin.win);
	wnoutrefresh(rwin.win);
	wnoutrefresh(rwin.title);
	wnoutrefresh(stat_win);
	wnoutrefresh(status_bar);
	wnoutrefresh(pos_win);
	wnoutrefresh(num_win);
	wnoutrefresh(lborder);
	wnoutrefresh(mborder);
	wnoutrefresh(rborder);

	return 1;
}

void
redraw_window(void)
{
	int screen_x, screen_y;
	int x, y;
	struct winsize ws;

	curr_stats.freeze = 1;

	ioctl(0, TIOCGWINSZ, &ws);

	/* changed for pdcurses */
	resize_term(ws.ws_row, ws.ws_col);

	getmaxyx(stdscr, screen_y, screen_x);

	if(screen_y < 10)
		finish("Terminal is too small to run vifm\n");
	if(screen_x < 30)
		finish("Terminal is too small to run vifm\n");

	wclear(stdscr);
	wclear(lwin.title);
	wclear(lwin.win);
	wclear(rwin.title);
	wclear(rwin.win);
	wclear(stat_win);
	wclear(status_bar);
	wclear(pos_win);
	wclear(num_win);
	wclear(rborder);
	wclear(mborder);
	wclear(lborder);

	wresize(stdscr, screen_y, screen_x);
	mvwin(sort_win, (screen_y - NUM_SORT_OPTIONS + 3)/2, (screen_x -30)/2);
	mvwin(change_win, (screen_y - 10)/2, (screen_x -30)/2);
	//wresize(menu_win, screen_y - 1, screen_x);
	wresize(error_win, (screen_y -10)/2, screen_x -2);
	mvwin(error_win, (screen_y -10)/2, 1);
	wresize(lborder, screen_y -2, 1);

	if(curr_stats.number_of_windows == 1)
	{
		wresize(lwin.title, 1, screen_x -1);
		wresize(lwin.win, screen_y -3, screen_x -2);
		getmaxyx(lwin.win, y, x);
		lwin.window_width = x -1;
		lwin.window_rows = y -1;

		wresize(rwin.title, 1, screen_x -1);
		mvwin(rwin.title, 0, 1);
		wresize(rwin.win, screen_y -3, screen_x -2);
		mvwin(rwin.win, 1, 1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x -1;
		rwin.window_rows = y -1;
	}
	else
	{
		wresize(lwin.title, 1, screen_x/2 -2);
		wresize(lwin.win, screen_y -3, screen_x/2 -2);
		getmaxyx(lwin.win, y, x);
		lwin.window_width = x -1;
		lwin.window_rows = y -1;

		mvwin(mborder, 0, screen_x/2 -1);
		wresize(mborder, screen_y, 2);

		wresize(rwin.title, 1, screen_x/2 -2);
		mvwin(rwin.title, 0, screen_x/2 +1);

		wresize(rwin.win, screen_y -3, screen_x/2 -2);
		mvwin(rwin.win, 1, screen_x/2 +1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x -1;
		rwin.window_rows = y -1;
	}

	/* For FreeBSD */
	keypad(lwin.win, TRUE);
	keypad(rwin.win, TRUE);

	if(screen_x % 2)
	{
		wresize(rborder, screen_y -2, 2);
		mvwin(rborder, 0, screen_x -2);
	}
	else
	{
		wresize(rborder, screen_y -2, 1);
		mvwin(rborder, 0, screen_x -1);
	}

	wresize(stat_win, 1, screen_x);
	mvwin(stat_win, screen_y -2, 0);
	wresize(status_bar, 1, screen_x -19);

	/* For FreeBSD */
	keypad(status_bar, TRUE);

	mvwin(status_bar, screen_y -1, 0);
	wresize(pos_win, 1, 13);
	mvwin(pos_win, screen_y -1, screen_x -13);

	wresize(num_win, 1, 6);
	mvwin(num_win, screen_y -1, screen_x -19);

	curs_set(0);

	change_directory(&rwin, rwin.curr_dir);
	load_dir_list(&rwin, 0);
	change_directory(&lwin, lwin.curr_dir);
	load_dir_list(&lwin, 0);

	if(curr_stats.view)
	{
		wclear(other_view->win);

		change_directory(curr_view, curr_view->curr_dir);
		load_dir_list(curr_view, 0);

		quick_view_file(curr_view);
	}
	else
		change_directory(curr_view, curr_view->curr_dir);

	update_stat_window(curr_view);

	if(curr_view->selected_files)
	{
		char status_buf[24];
		snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
				curr_view->selected_files,
				curr_view->selected_files == 1 ? "File" : "Files");
		status_bar_message(status_buf);
	}
	else
		status_bar_message(" ");

	update_pos_window(curr_view);

	update_all_windows();

	moveto_list_pos(curr_view, curr_view->list_pos);
	wrefresh(curr_view->win);
	curr_stats.freeze = 0;
	curr_stats.need_redraw = 0;
}

void
clean_status_bar()
{
	werase(status_bar);
	wnoutrefresh(status_bar);
}

void
change_window(void)
{
	switch_views();

	if(curr_stats.number_of_windows != 1)
	{
		wattroff(other_view->title, A_BOLD);
		wattroff(other_view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
		mvwaddstr(other_view->win, other_view->curr_line, 0, "*");
		erase_current_line_bar(other_view);
		werase(other_view->title);
		wprintw(other_view->title, "%s", other_view->curr_dir);
		wnoutrefresh(other_view->title);
	}

	if(curr_stats.view)
	{
		wbkgdset(curr_view->title,
				COLOR_PAIR(BORDER_COLOR + curr_view->color_scheme));
		wbkgdset(curr_view->win,
				COLOR_PAIR(WIN_COLOR + curr_view->color_scheme));
		change_directory(other_view, other_view->curr_dir);
		load_dir_list(other_view, 0);
		change_directory(curr_view, curr_view->curr_dir);
		load_dir_list(curr_view, 0);
	}

	wattron(curr_view->title, A_BOLD);
	werase(curr_view->title);
	wprintw(curr_view->title, "%s", curr_view->curr_dir);
	wnoutrefresh(curr_view->title);

	wnoutrefresh(other_view->win);
	wnoutrefresh(curr_view->win);

	change_directory(curr_view, curr_view->curr_dir);

	if (curr_stats.number_of_windows == 1)
		load_dir_list(curr_view, 1);

	moveto_list_pos(curr_view, curr_view->list_pos);
	werase(status_bar);
	wnoutrefresh(status_bar);

	if (curr_stats.number_of_windows == 1)
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
	touchwin(status_bar);
	touchwin(pos_win);
	touchwin(num_win);
	touchwin(rborder);

	/*
	 * redrawwin() shouldn't be needed.  But without it there is a
	 * lot of flickering when redrawing the windows?
	 */

	redrawwin(lborder);
	redrawwin(stat_win);
	redrawwin(status_bar);
	redrawwin(pos_win);
	redrawwin(num_win);
	redrawwin(rborder);

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

		update_view(&lwin);
		update_view(&rwin);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(stat_win);
	wnoutrefresh(status_bar);
	wnoutrefresh(pos_win);
	wnoutrefresh(num_win);
	wnoutrefresh(rborder);

	doupdate();
}

void
update_input_bar(wchar_t c)
{
	wchar_t buf[] = {c, '\0'};

	if(!curr_stats.use_input_bar)
	{
		return;
	}

	if(getcurx(num_win) == getmaxx(num_win) - 1)
	{
		mvwdelch(num_win, 0, 0);
		wmove(num_win, 0, getmaxx(num_win) - 2);
	}

	waddwstr(num_win, buf);
	wrefresh(num_win);
}

void
switch_views(void)
{
	FileView *tmp = curr_view;
	curr_view = other_view;
	other_view = tmp;
}

void
clear_num_window(void)
{
	if(curr_stats.use_input_bar)
	{
		werase(num_win);
		wrefresh(num_win);
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

	char buf[strlen(msg) + 2 + 1];

	if(period == 0)
	{
		pause = 1;
		return;
	}

	pause++;

	if(pause%period != 0)
		return;

	pause = 1;

	sprintf(buf, "%s %c", msg, marks[count]);
	status_bar_message(buf);
	wrefresh(status_bar);

	count = (count + 1) % sizeof(marks);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
