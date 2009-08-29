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

#include<sys/types.h>
#include<regex.h>
#include<ctype.h> /* isspace() */
#include<string.h> /* strchr() */
#include<unistd.h> /* access() */
#include<termios.h> /* struct winsize */
#include<sys/ioctl.h>
#include<signal.h>

#include "background.h"
#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "keys.h"
#include "registers.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

typedef struct menu_info 
{
	int top;
	int current;
	int len;
	int pos;
	int win_rows;
	int type;
	int match_dir;
	int matching_entries;
	char *regexp;
	char *title;
	char *args;
	char **data;
	/* For user menus only */
	char *get_info_script; /* program + args to fill in menu. */
}menu_info;

enum {
	APROPOS,
	BOOKMARK,
	COMMAND,
	FILETYPE,
	HISTORY,
	JOBS,
	LOCATE,
	REGISTER,
	USER,
	VIFM
};

enum {
	NONE,
	UP,
	DOWN
};

static void draw_menu(FileView *view, menu_info *m);

void
show_progress(void)
{
	static int count = 0;
	static int pause = 1;

	pause++;

	if ((pause % 1000) == 0)
	{
		pause = 1;
				
		switch(count)
		{
			case 0:
				status_bar_message("Loading Menu |");
				break;
			case 1:
				status_bar_message("Loading Menu /");
				break;
			case 2:
				status_bar_message("Loading Menu -");
				break;
			case 3: 
				status_bar_message("Loading Menu \\");
				count = -1;
				break;
			default:
				count = -1;
				break;
		}
	}
	else
		return;

	wrefresh(status_bar);

	count++;
}

static void
show_position_in_menu(menu_info *m)
{
	char pos_buf[14];
	snprintf(pos_buf, sizeof(pos_buf), " %d-%d ", m->pos + 1, m->len);
	werase(pos_win);
	mvwaddstr(pos_win, 0, 13 - strlen(pos_buf),  pos_buf);
	wrefresh(pos_win);
}

static void
clean_menu_position(menu_info *m)
{
	int x, y, z;
	char * buf = (char *)NULL;

	getmaxyx(menu_win, y, x);

	buf = (char *)malloc(x + 2);


	snprintf(buf, x, " %s", m->data[m->pos]);

	for (z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	wattron(menu_win, COLOR_PAIR(WIN_COLOR));

	mvwaddnstr(menu_win, m->current, 1, buf, x - 2);

	wattroff(menu_win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);

	my_free(buf);
}

void
show_error_msg(char *title, char *message)
{
	int x, y;
	int key;
	int done = 0;
	int z = 0;
	char *dup = strdup(message);

	curr_stats.freeze = 1;
	curs_set(0);
	werase(error_win);

	getmaxyx(error_win, y, x);

/*_SZ_BEGIN_*/
	if(strlen(dup) > x-2)
		dup[x-2] = '\0';
/*you might have had a reason having (z < strlen(dup) -1) there before, but for me, it has just incorrectly
truncated my messages by the last character.*/
	while ((z < strlen(dup)) && isprint(dup[z]))
		z++;
/*_SZ_END_*/

	dup[z] = '\0';

	mvwaddstr(error_win, 2, (x - strlen(dup))/2, dup);

	box(error_win, 0, 0);
	mvwaddstr(error_win, 0, (x - strlen(title))/2, title);

	mvwaddstr(error_win, y -2, (x - 25)/2, "Press Return to continue");

	while(!done)
	{
		key = wgetch(error_win);
		if(key == 13 || key == 3) /* ascii Return  ascii Ctrl-c */
			done = 1;
	}

	my_free(dup);

	curr_stats.freeze = 0;

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_redraw)
		redraw_window();
}

void
reset_popup_menu(menu_info *m)
{
	int z;

	if (m->args)
		my_free(m->args);

	for (z = 0; z < m->len; z++)
	{
		if (m->data[z])
			my_free(m->data[z]);
	}
	if (m->regexp)
		my_free(m->regexp);
	if (m->data)
		my_free(m->data);
	if (m->title)
		my_free(m->title);

	werase(menu_win);
	curr_stats.menu = 0;
	redraw_window();
}

void
setup_menu(FileView *view)
{
	scrollok(menu_win, FALSE);
	curr_stats.menu = 1;
	curs_set(0);
	werase(menu_win);
	werase(status_bar);
	werase(pos_win);
	wrefresh(status_bar);
	wrefresh(pos_win);
}

void
init_active_bookmarks(void)
{
	int i, x;

	i = 0;
	for (x = 0; x < NUM_BOOKMARKS; ++x)
	{
		if (is_bookmark(x))
			active_bookmarks[i++] = x;
	}
}

static void
moveto_menu_pos(FileView *view, int pos,  menu_info *m)
{
	int redraw = 0;
	int x, y, z;
	char * buf = (char *)NULL;

	getmaxyx(menu_win, y, x);


	if(pos < 1)
		pos = 0;

	if(pos > m->len -1)
		pos = m->len -1;

	if((m->top <=  pos) && (pos <= (m->top + m->win_rows +1)))
	{
		m->current = pos - m->top +1;
	}
	if((pos >= (m->top + m->win_rows -2)))
	{
		while(pos >= (m->top + m->win_rows -2))
			m->top++;

		m->current = m->win_rows -2;
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
		draw_menu(view, m);


	buf = (char *)malloc((x + 2));
	if (!buf)
		return;
	snprintf(buf, x, " %s", m->data[pos]);

	for (z = strlen(buf); z < x; z++)
		buf[z] = ' ';

	buf[x] = ' ';
	buf[x + 1] = '\0';

	wattron(menu_win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	
	mvwaddnstr(menu_win, m->current, 1, buf, x - 2);

	wattroff(menu_win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);

	m->pos = pos;
	my_free(buf);
	show_position_in_menu(m);
}

static void
redraw_menu(FileView *view, menu_info *m)
{
	int screen_x, screen_y;
	struct winsize ws;

	curr_stats.freeze = 1;
	curr_stats.redraw_menu = 0;

	ioctl(0, TIOCGWINSZ, &ws);
	//changed for pdcurses
	//resizeterm(ws.ws_row, ws.ws_col);
	getmaxyx(stdscr, screen_y, screen_x);

	wclear(stdscr);
	wclear(menu_win);
	wclear(status_bar);
	wclear(pos_win);

	wresize(menu_win, screen_y - 1, screen_x);
	wresize(status_bar, 1, screen_x -19);
	mvwin(status_bar, screen_y -1, 0);
	wresize(pos_win, 1, 13);
	mvwin(pos_win, screen_y -1, screen_x -13);
	box(menu_win, 0, 0);
	wrefresh(status_bar);
	wrefresh(pos_win);
	wrefresh(menu_win);
	curr_stats.freeze = 0;
	draw_menu(view, m);
	moveto_menu_pos(view, m->pos, m);
}

int
search_menu_forwards(FileView *view, menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int x;
	regex_t re;
	m->matching_entries = 0;

	for(x = 0; x < m->len; x++)
	{
		if(regcomp(&re, m->regexp, REG_EXTENDED) == 0)
		{
			if(regexec(&re, m->data[x], 0, NULL, 0) == 0)
			{
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
	}

	if((match_up > -1) || (match_down > -1))
	{
		char buf[64];
		int pos;

		if (match_down > -1)
			pos = match_down;
		else
			pos = match_up;

		clean_menu_position(m);
		moveto_menu_pos(view, pos, m);
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
search_menu_backwards(FileView *view, menu_info *m, int start_pos)
{
	int match_up = -1;
	int match_down = -1;
	int x;
	regex_t re;
	m->matching_entries = 0;

	for(x = m->len - 1; x > -1; x--)
	{
		if(regcomp(&re, m->regexp, REG_EXTENDED) == 0)
		{
			if(regexec(&re, m->data[x], 0, NULL, 0) == 0)
			{
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
	}

	if ((match_up  > -1) || (match_down > -1))
	{
		char buf[64];
		int pos;

		if (match_up > - 1)
			pos = match_up;
		else
			pos = match_down;

		clean_menu_position(m);
		moveto_menu_pos(view, pos, m);
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
search_menu_list(FileView *view, char * pattern, menu_info *m)
{
	int save = 0;

	if (pattern)
		m->regexp = strdup(pattern);

	switch (m->match_dir)
	{
		case NONE:
				save = search_menu_forwards(view, m, m->pos);
			break;
		case UP:
				save = search_menu_backwards(view, m, m->pos - 1);
			break;
		case DOWN:
				save = search_menu_forwards(view, m, m->pos + 1);
			break;
		default:
		break;
	}
	return save;
}

static void
execute_jobs_cb(FileView *view, menu_info *m)
{

}

static void
execute_apropos_cb(FileView *view, menu_info *m)
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
		my_free(free_this);
	}
	else
		my_free(free_this);

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
			{
				strcat(file, "/");
			}

			if(file[0] == '.')
				show_dot_files(view);

			if(find_file_pos_in_list(view, file) < 0)
				remove_filename_filter(view);

			moveto_list_pos(view, find_file_pos_in_list(view, file));
		}
		else
			moveto_list_pos(view, find_file_pos_in_list(view, file));
	}

	my_free(free_this);
}

static void
execute_filetype_cb(FileView *view, menu_info *m)
{
	char *filename = get_current_file_name(view);
	char *prog_str = get_all_programs_for_file(filename);
	char command[NAME_MAX];
	char *ptr = NULL;


	if((ptr = strchr(prog_str, ',')) == NULL)
	{
		if(strchr(prog_str, '%'))
		{
			int m = 0;
			char *expanded_command = expand_macros(view, prog_str, NULL, &m, 0);
			shellout(expanded_command, 0);
			my_free(expanded_command);
			return;
		}
		else
		{
			snprintf(command, sizeof(command), "%s %s", prog_str, filename);
			shellout(command, 0);
			return;
		}
	}
	else
	{
		char *prog_copy = strdup(prog_str);
		char *free_this = prog_copy;
		char *ptr1 = NULL;
		int x = 1;

		while((ptr = ptr1 = strchr(prog_copy, ',')) != NULL)
		{
			*ptr = '\0';
			ptr1++;

			if(x == m->pos +1)
			{
				if(strchr(prog_str, '%'))
				{
					int m = 0;
					char *expanded_command = expand_macros(view, prog_copy, NULL, &m, 0);
					shellout(expanded_command, 0);
					my_free(expanded_command);
					free(free_this);
					return;
				}
				else
				{
					snprintf(command, sizeof(command), "%s %s", 
							prog_copy, filename);
					shellout(command, 0);
					free(free_this);
					return;
				}
			}
			prog_copy = ptr1;
			x++;
		}
		if(strchr(prog_str, '%'))
		{
			int m = 0;
			char *expanded_command = expand_macros(view, prog_copy, NULL, &m, 0);
			shellout(expanded_command, 0);
			my_free(expanded_command);
			free(free_this);
			return;
		}
		else
		{
			snprintf(command, sizeof(command), "%s %s", prog_copy, filename);
			shellout(command, 0);
			free(free_this);
			return;
		}
	}
}

static void
execute_menu_cb(FileView *view, menu_info *m)
{
	switch(m->type)
	{
		case APROPOS:
				execute_apropos_cb(view, m);
			break;
		case BOOKMARK:
				move_to_bookmark(view, index2mark(active_bookmarks[m->pos]));
			break;
		case COMMAND:
				execute_command(view, command_list[m->pos].name);
			break;
		case FILETYPE:
				execute_filetype_cb(view, m);
			break;
		case HISTORY:
			{
				change_directory(view, view->history[m->pos].dir);
				load_dir_list(view, 1);
				moveto_list_pos(view, find_file_pos_in_list(view, 
							view->history[m->pos].file));
			}
			break;
		case JOBS:
			execute_jobs_cb(view, m);
			break;
		case LOCATE:
				execute_locate_cb(view, m);
			break;
			break;
		case VIFM:
			break;
		default:
			break;
	}
}

static void
reload_bookmarks_menu_list(menu_info *m)
{
	int x, i, z, len, j;
	char buf[PATH_MAX];

	getmaxyx(menu_win, z, len);


	for (z = 0; z < m->len; z++)
	{
		if (m->data[z])
			my_free(m->data[z]);
	}

	init_active_bookmarks();
	m->len = cfg.num_bookmarks;
	x = 0;

	for(i = 1; x < m->len; i++)
	{
		j = active_bookmarks[x];
		if (!strcmp(bookmarks[j].directory, "/"))
			snprintf(buf, sizeof(buf), "  %c   /%s", index2mark(j), bookmarks[j].file);
		else if (!strcmp(bookmarks[j].file, "../"))
			snprintf(buf, sizeof(buf), "  %c   %s", index2mark(j),
					bookmarks[j].directory);
		else
			snprintf(buf, sizeof(buf), "  %c   %s/%s", index2mark(j),
					bookmarks[j].directory,	bookmarks[j].file);

		m->data = (char **)realloc(m->data, sizeof(char *) * (x + 1));
		m->data[x] = (char *)malloc(sizeof(buf) + 2);
		snprintf(m->data[x], sizeof(buf), "%s", buf);

		x++;
	}
	m->len = x;
}

static void
reload_command_menu_list(menu_info *m)
{

	int x, i, z, len;

	getmaxyx(menu_win, z, len);

	for (z = 0; z < m->len; z++)
	{
		if (m->data[z])
			my_free(m->data[z]);
	}

	m->len = cfg.command_num;

	qsort(command_list, cfg.command_num, sizeof(command_t),
			sort_this);

	x = 0;

	for (i = 1; x < m->len; i++)
	{
		m->data = (char **)realloc(m->data, sizeof(char *) * (x + 1));
		m->data[x] = (char *)malloc(len + 2);
		snprintf(m->data[x], len, " %-*s  %s ", 10, command_list[x].name,
				command_list[x].action);

		x++;
		/* This will show the expanded command instead of the macros 
		 * char *expanded = expand_macros(view, command_list[x].action, NULL);
		 * my_free(expanded);
		*/
	}
}

static void
menu_key_cb(FileView *view, menu_info *m)
{
	int done = 0;
	int abort = 0;
	int save_msg = 0;
	int y, len;

	getmaxyx(menu_win, y, len);
	keypad(menu_win, TRUE);
	werase(status_bar);
	wtimeout(menu_win, 1000);

	while(!done)
	{
		int key = wgetch(menu_win);

		switch(key)
		{
			case '/':
				{
					m->match_dir = NONE;
					if (m->regexp)
						my_free(m->regexp);

					get_command(view, MENU_SEARCH, m);
				}
				break;
			case ':':
				{
					save_msg = get_command(view, MENU_COMMAND, m);
					if (save_msg < 0)
					{
						done = 1;
						abort = 1;
					}
				}
				break;
			case '?':
				{
					m->match_dir = UP;

					if (m->regexp)
						my_free(m->regexp);

					get_command(view, MENU_SEARCH, m);
				}
				break;
			case 2: /* ascii Ctrl B */
			case KEY_PPAGE:
				clean_menu_position(m);
				moveto_menu_pos(view, m->top - m->win_rows + 2, m);
				break;
			case 3: /* ascii Ctrl C */
			case 27: /* ascii Escape */
				done = 1;
				abort = 1;
				break;
			case 6: /* ascii Ctrl F */
				{
					clean_menu_position(m);
					m->pos = m->pos + m->win_rows;
					moveto_menu_pos(view, m->pos, m);
				}
				break;
			case KEY_NPAGE:
				break;
			case 'l':
			case 13: /* ascii Return */
						done = 1;
				break;
			case 'G':
				clean_menu_position(m);
				moveto_menu_pos(view, m->len -1, m);
				break;
			case 'N':
				{
					if (m->regexp != NULL)
					{
						m->match_dir = UP;
						m->matching_entries = 0;
						search_menu_list(view, NULL, m);
					}
					else
					{
						status_bar_message("No search pattern set.");
						wrefresh(status_bar);
					}
				}
				break;
			case 'g':
				{
					key = wgetch(menu_win);
					if(key == 'g')
					{
						clean_menu_position(m);
						moveto_menu_pos(view, 0, m);
					}
				}
				break;
			case 'd':
				{
					key = wgetch(menu_win);

					if(key != 'd')
						break;

					if(m->type == COMMAND)
					{
						clean_menu_position(m);
						remove_command(command_list[m->pos].name);

						reload_command_menu_list(m);
						draw_menu(view, m);

						if(m->pos -1 >= 0)
							moveto_menu_pos(view, m->pos -1, m);
						else
							moveto_menu_pos(view, 0, m);
					}
					else if(m->type == BOOKMARK)
					{
						clean_menu_position(m);
						remove_bookmark(active_bookmarks[m->pos]);

						reload_bookmarks_menu_list(m);
						draw_menu(view, m);

						if(m->pos -1 >= 0)
							moveto_menu_pos(view, m->pos -1, m);
						else
							moveto_menu_pos(view, 0, m);
					}
				}
				break;
			case 'j':
			case KEY_DOWN:
				{
					clean_menu_position(m);
					m->pos++;
					moveto_menu_pos(view, m->pos, m);
				}
				break;
			case 'k':
			case KEY_UP:
				{
					clean_menu_position(m);
					m->pos--;
					moveto_menu_pos(view, m->pos, m);
					wrefresh(menu_win);
				}
				break;
			case 'n':
				{
					if (m->regexp != NULL)
					{
						m->match_dir = DOWN;
						m->matching_entries = 0;
						search_menu_list(view, NULL, m);
					}
					else
					{
						status_bar_message("No search pattern set>");
						wrefresh(status_bar);
					}
				}
				break;
			default:
				break;
		}
		if (curr_stats.redraw_menu)
			redraw_menu(view, m);

	} /* end of while(!done) */

	if (abort)
		return;

	execute_menu_cb(view, m);
}

static void
draw_menu(FileView *view,  menu_info *m)
{
	int i;
	int x, y;
	int len;

	getmaxyx(menu_win, y, x);
	len = x;
	werase(menu_win);

	box(menu_win, 0, 0);

	if(m->win_rows - 2 >= m->len)
	{
		m->top = 0;
	}
	x = m->top;

	wattron(menu_win, A_BOLD);
	mvwaddstr(menu_win, 0, 3, m->title);
	wattroff(menu_win, A_BOLD);

	for(i = 1; x < m->len; i++)
	{
		char *ptr = NULL;
		chomp(m->data[x]);
		if ((ptr = strchr(m->data[x], '\n')) || (ptr = strchr(m->data[x], '\r')))
			*ptr = '\0';
		mvwaddnstr(menu_win, i, 2, m->data[x], len - 4);
		x++;

		if(i +3 > y)
			break;
	}
}

void
show_apropos_menu(FileView *view, char *args)
{
	int x = 0;
	char buf[256];
	FILE *file;
	int len = 0;

	menu_info m;
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
		show_error_msg(" Nothing Appropriate ", buf);
	}
	else
	{
		setup_menu(view);
		draw_menu(view, &m);
		moveto_menu_pos(view, 0, &m);
		menu_key_cb(view, &m);
	}
	reset_popup_menu(&m);
}

void
show_bookmarks_menu(FileView *view)
{
	int i, j, x;
	char buf[PATH_MAX];

	menu_info m;
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

	getmaxyx(menu_win, m.win_rows, x);

	init_active_bookmarks();

	m.title = (char *)malloc((strlen(" Mark -- File ") + 1) * sizeof(char));
	snprintf(m.title, strlen(" Mark -- File "),  " Mark -- File ");

	x = 0;

	for(i = 1; x < m.len; i++)
	{
		j = active_bookmarks[x];
		if (!strcmp(bookmarks[j].directory, "/"))
			snprintf(buf, sizeof(buf), "  %c   /%s", index2mark(j), bookmarks[j].file);
		else if (!strcmp(bookmarks[j].file, "../"))
			snprintf(buf, sizeof(buf), "  %c   %s", index2mark(j),
					bookmarks[j].directory);
		else
			snprintf(buf, sizeof(buf), "  %c   %s/%s", index2mark(j),
					bookmarks[j].directory,	bookmarks[j].file);

		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(sizeof(buf) + 2);
		snprintf(m.data[x], sizeof(buf), "%s", buf);

		x++;
	}
	m.len = x;

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_commands_menu(FileView *view)
{
	int len, i, x;

	menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = cfg.command_num;
	m.pos = 0;
	m.win_rows = 0;
	m.type = COMMAND;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	if (cfg.command_num < 1)
	{
		show_error_msg("No commands set", "No commands are set.");
		return;
	}

	getmaxyx(menu_win, m.win_rows, len);
	qsort(command_list, cfg.command_num, sizeof(command_t),
			sort_this);

	m.title = (char *)malloc((strlen(" Command ------ Action ") + 1) 
			* sizeof(char));
	snprintf(m.title, strlen(" Command ------ Action ") + 1, 
			" Command ------ Action  ");

	x = 0;

	for (i = 1; x < m.len; i++)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
		m.data[x] = (char *)malloc(len + 2);
		snprintf(m.data[x], len, " %-*s  %s ", 10, command_list[x].name,
				command_list[x].action);

		x++;
		/* This will show the expanded command instead of the macros 
		 * char *expanded = expand_macros(view, command_list[x].action, NULL);
		 * my_free(expanded);
		*/
	}

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_filetypes_menu(FileView *view)
{
	char *filename = get_current_file_name(view);
	char *prog_str = get_all_programs_for_file(filename);
	if (prog_str == NULL)
	{
		show_error_msg("  Filetype is not set.  ", 
				"No programs set for this filetype.");
		return;
	}
	else
	{
		int x = 0;
		int len = 0;
		char *ptr = NULL;

		menu_info m;
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

		getmaxyx(menu_win, m.win_rows, len);

		if ((ptr = strchr(prog_str, ',')) == NULL)
		{
			m.len = 1;
			m.data = (char **)realloc(m.data, sizeof(char *) * (len + 1));
			m.data[0] = strdup(prog_str);
		}
		else
		{
			char *prog_copy = strdup(prog_str);
			char *free_this = prog_copy;
			char *ptr1 = NULL;

			while ((ptr = ptr1 = strchr(prog_copy, ',')) != NULL)
			{
				*ptr = '\0';
				ptr1++;
				m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
				m.data[x] = strdup(prog_copy);
				prog_copy = ptr1;
				x++;
				m.len = x;
			}
			m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
			m.data[x] = (char *)malloc((len + 1) * sizeof(char)); 
			snprintf(m.data[x], len, "%s", prog_copy);
			m.len++;

			free(free_this);
		}
		setup_menu(view);
		draw_menu(view, &m);
		moveto_menu_pos(view, 0, &m);
		menu_key_cb(view, &m);
		reset_popup_menu(&m);
	}
}

void
show_history_menu(FileView *view)
{
	int x;
	menu_info m;

	if (view->history_num < 2)
		return;

	m.top = 0;
	m.current = 1;
	m.len = view->history_num + 1;
	m.pos = 0;
	m.win_rows = 0;
	m.type = HISTORY;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < view->history_num + 1; x++)
	{
		if(strlen(view->history[x].dir) < 1)
			break;

		/* Change the current dir to reflect the current file. */ 
		if(!strcmp(view->history[x].dir, view->curr_dir))
			snprintf(view->history[x].file, sizeof(view->history[x].file),
						"%s", view->dir_entry[view->list_pos].name);

		if(!strcmp(view->history[x].dir, "/"))
		{
			m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
			m.data[x] = (char *)malloc((strlen(view->history[x].file) + 1) 
					* sizeof(char));
			snprintf(m.data[x], strlen(view->history[x].file), 
						"%s", view->history[x].file);
		}
		else
		{
			m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
			m.data[x] = (char *)malloc((strlen(view->history[x].file) + 
						strlen(view->history[x].dir) + 2) * sizeof(char));
			snprintf(m.data[x], strlen(view->history[x].file) + 
					strlen(view->history[x].dir) + 1, "%s/%s", 
						view->history[x].dir, view->history[x].file); 
		}
		m.len = x;
	}
	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_locate_menu(FileView *view, char *args)
{
	int x = 0;
	char buf[256];
	FILE *file;

	menu_info m;
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
	m.args = strdup(args);
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);
	
	snprintf(buf, sizeof(buf), "locate %s",  args);
	m.title = strdup(buf);
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
		snprintf(buf, sizeof(buf), "No files found matching \"%s\"", args);
		status_bar_message(buf);
		wrefresh(status_bar);
		return;
	}

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_user_menu(FileView *view, char *command)
{
	int x;
	char buf[256];
	FILE *file;

	menu_info m;
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


	for ( x = 0; x < strlen(command); x++)
	{
		if (command[x] == ',')
		{
			command[x] = '\0';
			break;
		}
	}

	getmaxyx(menu_win, m.win_rows, x);

	snprintf(buf, sizeof(buf), " %s ",  m.get_info_script);
	m.title = strdup(buf);
	file = popen(buf, "r");

	if(!file)
	{
		reset_popup_menu(&m);
		show_error_msg("Trouble opening a file", "Unable to open file");
		return ;
	}
	x = 0;

	curr_stats.search = 1;

	while(fgets(buf, sizeof(buf), file))
	{
		show_progress();
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

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_jobs_menu(FileView *view)
{
	Jobs_List *p = jobs;
	Finished_Jobs *fj = NULL;
	sigset_t new_mask;
	int x;
	menu_info m;
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
	 * SIGCHLD  needs to be blocked anytime the Finished_Jobs list 
	 * is accessed from anywhere except the received_sigchld().
	 */
	sigemptyset(&new_mask);
	sigaddset(&new_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &new_mask, NULL);

	fj = fjobs;

	while (p)
	{

		/* Mark any finished jobs */
		while (fj)
		{
			if (p->pid == fj->pid)
			{
				p->running = 0;
				fj->remove = 1;
			}
			fj = fj->next;
		}

		if (p->running)
		{
			m.data = (char **)realloc(m.data, sizeof(char *) * (x + 1));
			m.data[x] = (char *)malloc(strlen(p->cmd) + 24);
			snprintf(m.data[x], strlen(p->cmd) + 22, " %d     %s ",
					p->pid, p->cmd);

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

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);

}

void
show_register_menu(FileView *view)
{
	int x;

	menu_info m;
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

	for (x = 0; x < NUM_REGISTERS; x++)
	{
		if (reg[x].num_files > 0)
		{
			char buf[56];
			int y = reg[x].num_files;
			snprintf(buf, sizeof(buf), "\"%c", reg[x].name);
			m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
			m.data[m.len] = strdup(buf);
			m.len++;

			while (y)
			{

				y--;
				m.data = (char **)realloc(m.data, sizeof(char *) * (m.len + 1));
				m.data[m.len] = strdup(reg[x].files[y]);

				m.len++;
			}
		}
	}

	if (!m.len)
	{
		m.data = (char **)realloc(m.data, sizeof(char *) * 1);
		m.data[0] = strdup(" Registers are empty ");
		m.len = 1;
	}

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

void
show_vifm_menu(FileView *view)
{
	int x;

	menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.win_rows = 0;
	m.type = VIFM;
	m.matching_entries = 0;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	setup_menu(view);
	draw_menu(view, &m);
	moveto_menu_pos(view, 0, &m);
	menu_key_cb(view, &m);
	reset_popup_menu(&m);
}

int
execute_menu_command(FileView *view, char * command, menu_info *m)
{
	if (!strncmp("quit", command, strlen(command)))
		return -1;
	else if (!strcmp("x", command))
		return -1;
	else if (isdigit(*command))
	{
		clean_menu_position(m);
		moveto_menu_pos(view, atoi(command) - 1, m);
	}

	return 0;
}

int
query_user_menu(char *title, char *message)
{
	int x, y;
	int key;
	int done = 0;
	int z = 0;
	char *dup = strdup(message);

	curr_stats.freeze = 1;
	curs_set(0);
	werase(error_win);

	getmaxyx(error_win, y, x);

	if(strlen(dup) > x -2)
		dup[x -2] = '\0';

	while ((z < strlen(dup) -1) && isprint(dup[z]))
		z++;

	dup[z] = '\0';

	mvwaddstr(error_win, 2, (x - strlen(dup))/2, dup);

	box(error_win, 0, 0);
	mvwaddstr(error_win, 0, (x - strlen(title))/2, title);

	mvwaddstr(error_win, y -2, (x - 25)/2, "Enter y[es] or n[o] ");

	while(!done)
	{
		key = wgetch(error_win);
		if(key == 'y' || key == 'n') /* ascii Return  ascii Ctrl-c */
			done = 1;
	}

	my_free(dup);

	curr_stats.freeze = 0;

	werase(error_win);
	wrefresh(error_win);

	touchwin(stdscr);

	update_all_windows();

	if(curr_stats.need_redraw)
		redraw_window();

	if (key == 'y')
		return 1;
	else
		return 0;
}





