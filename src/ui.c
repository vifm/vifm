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

#include <sys/ioctl.h>
#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR */
#include <grp.h> /* getgrgid() */
#include <pwd.h> /* getpwent() */
#include <stdlib.h> /* malloc */
#include <termios.h> /* struct winsize */
#include <unistd.h>

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
#include "menus.h"
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
	cur_x += 10;

	snprintf(name_buf, sizeof(name_buf), "%d %s filtered",
			view->filtered, view->filtered == 1 ? "file" : "files");
	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) + 2) , name_buf);

	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		*strchr(id_buf, ':') = '\0';
	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		id_buf[0] = '\0';
	mvwaddstr(stat_win, 0, cur_x, id_buf);

	wnoutrefresh(stat_win);
}

void
status_bar_messagef(const char *format, ...)
{
	va_list ap;
	char buf[512];

	va_start(ap, format);

	vsnprintf(buf, sizeof(buf), format, ap);
	status_bar_message(buf);

	va_end(ap);
}

/*
 * Repeats last message if message is NULL
 */
void
status_bar_message(const char *message)
{
	static char *msg;

	int len;
	const char *p, *q;
	int lines;

	if(!curr_stats.vifm_started)
		return;

	if(message != NULL)
	{
		free(msg);
		msg = strdup(message);
	}

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

	lines = status_bar_lines;
	if(status_bar_lines > 1 || strlen(p) > getmaxx(status_bar))
		lines++;

	werase(status_bar);
	mvwin(stat_win, getmaxy(stdscr) - lines - 1, 0);
	mvwin(status_bar, getmaxy(stdscr) - lines, 0);
	if(lines == 1)
		wresize(status_bar, lines, getmaxx(stdscr) - 19);
	else
		wresize(status_bar, lines, getmaxx(stdscr));
	wmove(status_bar, 0, 0);
	wprintw(status_bar, "%s", msg);
	if(lines > 1)
		wprintw(status_bar, "%s", "\nPress ENTER or type command to continue");
	wnoutrefresh(status_bar);
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
	int i;
	int color_scheme;

	initscr();
	noecho();
	nonl();
	raw();

	curs_set(0);

	getmaxyx(stdscr, screen_y, screen_x);
	/* screen is too small to be useful*/
	if(screen_y < 10)
		finish("Terminal is too small to run vifm\n");
	if(screen_x < 30)
		finish("Terminal is too small to run vifm\n");

	if(!has_colors())
		finish("Vifm requires a console that can support color.\n");

	start_color();
	/* Changed for pdcurses */
	use_default_colors();

	for(i = 0; i < cfg.color_scheme_num; i++)
	{
		int x;
		for(x = 0; x < MAXNUM_COLOR; x++)
		{
			if(x == MENU_COLOR)
				init_pair(1 + i*MAXNUM_COLOR + x, col_schemes[i].color[x].bg,
						col_schemes[i].color[x].fg);
			else
				init_pair(1 + i*MAXNUM_COLOR + x, col_schemes[i].color[x].fg,
						col_schemes[i].color[x].bg);
		}
	}

	color_scheme = 1 + cfg.color_scheme_cur*MAXNUM_COLOR;
	lwin.color_scheme = color_scheme;
	rwin.color_scheme = color_scheme;

	werase(stdscr);

	menu_win = newwin(screen_y - 1, screen_x, 0, 0);
	wbkgdset(menu_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	werase(menu_win);

	sort_win = newwin(NUM_SORT_OPTIONS + 3, 30, (screen_y -12)/2, (screen_x -30)/2);
	wbkgdset(sort_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	werase(sort_win);

	change_win = newwin(20, 30, (screen_y -20)/2, (screen_x -30)/2);
	wbkgdset(change_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	werase(change_win);

	error_win = newwin(10, screen_x -2, (screen_y -10)/2, 1);
	wbkgdset(error_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	werase(error_win);

	lborder = newwin(screen_y - 3, 1, 1, 0);

	wbkgdset(lborder, COLOR_PAIR(color_scheme + BORDER_COLOR));

	werase(lborder);

	if(curr_stats.number_of_windows == 1)
		lwin.title = newwin(1, screen_x - 2 + 3, 0, 0);
	else
		lwin.title = newwin(1, screen_x/2 - 2 + screen_x%2 + 3, 0, 0);

	wattrset(lwin.title, A_BOLD);
	wbkgdset(lwin.title, COLOR_PAIR(color_scheme + TOP_LINE_COLOR));

	werase(lwin.title);

	if(curr_stats.number_of_windows == 1)
		lwin.win = newwin(screen_y - 3, screen_x - 2, 1, 1);
	else
		lwin.win = newwin(screen_y - 3, screen_x/2 - 2 + screen_x%2, 1, 1);

	wbkgdset(lwin.win, COLOR_PAIR(color_scheme + WIN_COLOR));
	wattrset(lwin.win, A_BOLD);
	wattron(lwin.win, A_BOLD);
	werase(lwin.win);
	getmaxyx(lwin.win, y, x);
	lwin.window_rows = y -1;
	lwin.window_width = x -1;

	mborder = newwin(screen_y - 3, 2 - screen_x%2, 1,
			screen_x/2 - 1 + screen_x%2);

	wbkgdset(mborder, COLOR_PAIR(color_scheme + BORDER_COLOR));

	werase(mborder);

	if(curr_stats.number_of_windows == 1)
		rwin.title = newwin(1, screen_x - 2, 0, 0);
	else
		rwin.title = newwin(1, screen_x/2 - 1 + screen_x%2, 0, screen_x/2 + 1);

	wbkgdset(rwin.title, COLOR_PAIR(color_scheme + TOP_LINE_COLOR));
	wattrset(rwin.title, A_BOLD);
	wattroff(rwin.title, A_BOLD);

	werase(rwin.title);

	if(curr_stats.number_of_windows == 1)
		rwin.win = newwin(screen_y - 3, screen_x - 2, 1, 1);
	else
		rwin.win = newwin(screen_y - 3, screen_x/2 - 2 + screen_x%2, 1,
				screen_x/2 + 1);

	wattrset(rwin.win, A_BOLD);
	wattron(rwin.win, A_BOLD);
	wbkgdset(rwin.win, COLOR_PAIR(color_scheme + WIN_COLOR));
	werase(rwin.win);
	getmaxyx(rwin.win, y, x);
	rwin.window_rows = y - 1;
	rwin.window_width = x - 1;

	rborder = newwin(screen_y - 3, 1, 1, screen_x - 1);

	wbkgdset(rborder, COLOR_PAIR(color_scheme + BORDER_COLOR));

	werase(rborder);

	stat_win = newwin(1, screen_x, screen_y -2, 0);

	wbkgdset(stat_win, COLOR_PAIR(color_scheme + BORDER_COLOR));

	werase(stat_win);

	status_bar = newwin(1, screen_x - 19, screen_y -1, 0);
#ifdef ENABLE_EXTENDED_KEYS
	keypad(status_bar, TRUE);
#endif /* ENABLE_EXTENDED_KEYS */
	wattrset(status_bar, A_BOLD);
	wattron(status_bar, A_BOLD);
	wbkgdset(status_bar, COLOR_PAIR(color_scheme + STATUS_BAR_COLOR));
	werase(status_bar);

	pos_win = newwin(1, 13, screen_y - 1, screen_x - 13);
	wattrset(pos_win, A_BOLD);
	wattron(pos_win, A_BOLD);
	wbkgdset(pos_win, COLOR_PAIR(color_scheme + STATUS_BAR_COLOR));
	werase(pos_win);

	input_win = newwin(1, 6, screen_y - 1, screen_x -19);
	wattrset(input_win, A_BOLD);
	wattron(input_win, A_BOLD);
	wbkgdset(input_win, COLOR_PAIR(color_scheme + STATUS_BAR_COLOR));
	werase(input_win);

	wnoutrefresh(mborder);
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
	struct winsize ws = { .ws_col = -1, .ws_row = -1 };
	if(ioctl(0, TIOCGWINSZ, &ws) == -1)
		finish("Terminal error");
	if(ws.ws_row < 0)
		finish("Terminal is too small to run vifm\n");
	if(ws.ws_col < 0)
		finish("Terminal is too small to run vifm\n");
}

static void
resize_window(void)
{
	int screen_x, screen_y;
	int x, y;
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);

	/* changed for pdcurses */
	resize_term(ws.ws_row, ws.ws_col);

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
	mvwin(lborder, 1, 0);
	wresize(lborder, screen_y - 3, 1);

	if(curr_stats.number_of_windows == 1)
	{
		wresize(lwin.title, 1, screen_x - 1);
		wresize(lwin.win, screen_y - 3, screen_x - 2);
		getmaxyx(lwin.win, y, x);
		mvwin(lwin.win, 1, 1);
		lwin.window_width = x - 1;
		lwin.window_rows = y - 1;

		wresize(rwin.title, 1, screen_x - 1);
		mvwin(rwin.title, 0, 1);
		wresize(rwin.win, screen_y - 3, screen_x - 2);
		mvwin(rwin.win, 1, 1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x - 1;
		rwin.window_rows = y - 1;
	}
	else
	{
		wresize(lwin.title, 1, screen_x/2 - 2 + screen_x%2 + 3);
		wresize(lwin.win, screen_y - 3, screen_x/2 - 2 + screen_x%2);
		mvwin(lwin.win, 1, 1);
		getmaxyx(lwin.win, y, x);
		lwin.window_width = x - 1;
		lwin.window_rows = y - 1;

		mvwin(mborder, 0, screen_x/2 - 1 + screen_x%2);
		wresize(mborder, screen_y, 2 - screen_x%2);

		wresize(rwin.title, 1, screen_x/2 - 2 + screen_x%2 + 1);
		mvwin(rwin.title, 0, screen_x/2 + 1);

		wresize(rwin.win, screen_y - 3, screen_x/2 - 2 + screen_x%2);
		mvwin(rwin.win, 1, screen_x/2 + 1);
		getmaxyx(rwin.win, y, x);
		rwin.window_width = x - 1;
		rwin.window_rows = y - 1;
	}

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

	curs_set(0);
}

void
redraw_window(void)
{
	resize_window();
	if(curr_stats.too_small_term)
		return;

	if(curr_stats.show_full)
	{
		redraw_full_file_properties(curr_view);
		curr_stats.need_redraw = 0;
		return;
	}

	load_saving_pos(curr_view, 1);

	if(curr_stats.view)
	{
		wclear(other_view->win);
		quick_view_file(curr_view);
	}
	else
	{
		load_saving_pos(other_view, 1);
	}

	update_stat_window(curr_view);

	if(!is_status_bar_multiline())
	{
		if(curr_view->selected_files)
		{
			char status_buf[24];
			snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
					curr_view->selected_files,
					curr_view->selected_files == 1 ? "File" : "Files");
			status_bar_message(status_buf);
		}
		else
		{
			clean_status_bar();
		}

		update_pos_window(curr_view);
	}

	update_all_windows();

	moveto_list_pos(curr_view, curr_view->list_pos);
	wrefresh(curr_view->win);
	curr_stats.need_redraw = 0;

	if(curr_stats.errmsg_shown)
	{
		redraw_error_msg(NULL, NULL);
		redrawwin(error_win);
		wnoutrefresh(error_win);
		doupdate();
	}
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
		wattroff(other_view->title, A_BOLD);
		wattroff(other_view->win,
				COLOR_PAIR(other_view->color_scheme + CURR_LINE_COLOR) | A_BOLD);
		mvwaddstr(other_view->win, other_view->curr_line, 0, "*");
		erase_current_line_bar(other_view);
		update_view_title(other_view);
	}

	if(curr_stats.view)
	{
		wbkgdset(curr_view->title,
				COLOR_PAIR(TOP_LINE_COLOR + curr_view->color_scheme));
		wbkgdset(curr_view->win,
				COLOR_PAIR(WIN_COLOR + curr_view->color_scheme));
		change_directory(other_view, other_view->curr_dir);
		load_dir_list(other_view, 1);
		change_directory(curr_view, curr_view->curr_dir);
		load_dir_list(curr_view, 1);
	}

	wattron(curr_view->title, A_BOLD);
	update_view_title(curr_view);

	wnoutrefresh(other_view->win);
	wnoutrefresh(curr_view->win);

	change_directory(curr_view, curr_view->curr_dir);

	if(curr_stats.number_of_windows == 1)
		load_dir_list(curr_view, 1);

	draw_dir_list(curr_view, curr_view->top_line);
	moveto_list_pos(curr_view, curr_view->list_pos);
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

		update_view(&lwin);
		update_view(&rwin);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(stat_win);
	wnoutrefresh(pos_win);
	wnoutrefresh(input_win);
	wnoutrefresh(rborder);
	wnoutrefresh(status_bar);

	if(!curr_stats.errmsg_shown && curr_stats.vifm_started == 2)
		doupdate();
}

void
update_input_bar(wchar_t c)
{
	wchar_t buf[] = {c, '\0'};

	if(!curr_stats.use_input_bar)
		return;

	if(getcurx(input_win) == getmaxx(input_win) - 1)
	{
		mvwdelch(input_win, 0, 0);
		wmove(input_win, 0, getmaxx(input_win) - 2);
	}

	waddwstr(input_win, buf);
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
	wrefresh(status_bar);

	count = (count + 1) % sizeof(marks);
}

void
redraw_lists(void)
{
	draw_dir_list(curr_view, curr_view->top_line);
	moveto_list_pos(curr_view, curr_view->list_pos);
	draw_dir_list(other_view, other_view->top_line);
}

void
load_color_scheme(const char *name)
{
	int i;
	int color_scheme;

	i = find_color_scheme(name);
	if(i < 0)
	{
		show_error_msg("Color Scheme", "Invalid color scheme name");
		return;
	}

	cfg.color_scheme_cur = i;
	cfg.color_scheme = 1 + MAXNUM_COLOR*cfg.color_scheme_cur;
	color_scheme = 1 + i*MAXNUM_COLOR;

	wbkgdset(lborder, COLOR_PAIR(color_scheme + BORDER_COLOR));
	werase(lborder);
	wbkgdset(mborder, COLOR_PAIR(color_scheme + BORDER_COLOR));
	werase(mborder);
	wbkgdset(rborder, COLOR_PAIR(color_scheme + BORDER_COLOR));
	werase(rborder);
	wbkgdset(stat_win, COLOR_PAIR(color_scheme + STATUS_LINE_COLOR));

	wbkgdset(menu_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	wbkgdset(sort_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	wbkgdset(change_win, COLOR_PAIR(color_scheme + WIN_COLOR));
	wbkgdset(error_win, COLOR_PAIR(color_scheme + WIN_COLOR));

	if(curr_stats.vifm_started < 2)
		return;

	draw_dir_list(curr_view, curr_view->top_line);
	moveto_list_pos(curr_view, curr_view->list_pos);
	if(curr_stats.view)
		quick_view_file(curr_view);
	else
		draw_dir_list(other_view, other_view->top_line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
