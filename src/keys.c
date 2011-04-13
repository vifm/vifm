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


#include<ncurses.h>
#include<unistd.h> /* for chdir */
#include<string.h> /* strncpy */
#include<sys/time.h> /* select() */
#include<sys/types.h> /* select() */
#include<unistd.h> /* select() */

#include "background.h"
#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "file_info.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "menus.h"
#include "registers.h"
#include "search.h"
#include "signals.h"
#include "sort.h"
#include "status.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

void
switch_views(void)
{
	FileView *tmp = curr_view;
	curr_view = other_view;
	other_view = tmp;
}

void
clean_status_bar(FileView *view)
{
	werase(status_bar);
	wnoutrefresh(status_bar);
}

static void
update_num_window(char *text)
{
	werase(num_win);
	mvwaddstr(num_win, 0, 0, text);
	wrefresh(num_win);
}

static void
clear_num_window(void)
{
	werase(num_win);
	wrefresh(num_win);
}

static void
reload_window(FileView *view)
{
	struct stat s;

	stat(view->curr_dir, &s);
	if(view != curr_view)
		change_directory(view, view->curr_dir);

	load_dir_list(view, 1);
	view->dir_mtime = s.st_mtime;

	if(view != curr_view)
	{
		change_directory(curr_view, curr_view->curr_dir);
		mvwaddstr(view->win, view->curr_line, 0, "*");
		wrefresh(view->win);
	}
	else
		moveto_list_pos(view, view->list_pos);

}
/*
 * This checks the modified times of the directories.
 */
static void
check_if_filelists_have_changed(FileView *view)
{
	struct stat s;

	stat(view->curr_dir, &s);
	if(s.st_mtime  != view->dir_mtime)
		reload_window(view);

	if (curr_stats.number_of_windows != 1 && curr_stats.view != 1)
	{
		stat(other_view->curr_dir, &s);
		if(s.st_mtime != other_view->dir_mtime)
			reload_window(other_view);
	}

}

static void
repeat_last_command(FileView *view)
{
	if (0 > cfg.cmd_history_num)
		show_error_msg(" Command Error ", "Command history list is empty. ");
	else
		execute_command(view, cfg.cmd_history[0]);
}


void
rename_file(FileView *view)
{
	char *filename = get_current_file_name(view);
	char command[1024];
	int key;
	int pos = get_utf8_string_length(filename) + 1;
	int index = strlen(filename);
	int done = 0;
	int abort = 0;
	int real_len = pos - 1;
	int len;
	int found = -1;
	char buf[view->window_width -2];

    len = strlen(filename);
    if (filename[len - 1] == '/')
    {
        filename[len - 1] = '\0';
        len--;
        index--;
        pos--;
    }

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR));
	wmove(view->win, view->curr_line, 0);
	wclrtoeol(view->win);
	wmove(view->win, view->curr_line, 1);
	waddstr(view->win, filename);
	memset(buf, '\0', view->window_width -2);
	strncpy(buf, filename, sizeof(buf));
	wmove(view->win, view->curr_line, pos);

	curs_set(1);

  while(!done)
  {
	  if(curr_stats.freeze)
		  continue;
	  curs_set(1);
		flushinp();
		curr_stats.getting_input = 1;
		key = wgetch(view->win);

		switch(key)
		{
			case 27: /* ascii Escape */
			case 3: /* ascii Ctrl C */
				done = 1;
				abort = 1;
				break;
			case 13: /* ascii Return */
				done = 1;
				break;
			/* This needs to be changed to a value that is read from 
			 * the termcap file.
			 */
			case 0x7f: /* This is the one that works on my machine */
			case 8: /* ascii Backspace  ascii Ctrl H */
			case KEY_BACKSPACE: /* ncurses BACKSPACE KEY */
				{
                    size_t prev;
                    if(index == 0)
                        break;

                    pos--;
                    prev = get_utf8_prev_width(buf, index);

					if(index == len)
					{
                        len -= index - prev;
                        index = prev;
						if(pos < 1)
							pos = 1;
						if(index < 0)
							index = 0;
						
						mvwdelch(view->win, view->curr_line, pos);
						buf[index] = '\0';
						buf[len] = '\0';
					}
                    else
                    {
                        memmove(buf + prev, buf + index, len - index + 1);
                        len -= index - prev;
                        index = prev;

						mvwdelch(view->win, view->curr_line,
                                 get_utf8_string_length(buf) + 1);
                        mvwaddstr(view->win, view->curr_line, pos, buf + index);
                        wmove(view->win, view->curr_line, pos);
                    }
				}
				break;
            case KEY_DC: /* Delete character */
                {
                    size_t cwidth;
                    if(index == len)
                        break;

                    cwidth = get_char_width(buf + index);
                    memmove(buf + index, buf + index + cwidth,
                            len - index + cwidth + 1);
                    len -= cwidth;

                    mvwdelch(view->win, view->curr_line,
                             get_utf8_string_length(buf) + 1);
                    mvwaddstr(view->win, view->curr_line, pos, buf + index);
                    wmove(view->win, view->curr_line, pos);
                }
                break;
			case KEY_LEFT:
				{
                    index = get_utf8_prev_width(buf, index);
					pos--;

					if(index < 0)
						index = 0;

					if(pos < 1)
						pos = 1;

					wmove(view->win, view->curr_line, pos);

				}
				break;
			case KEY_RIGHT:
				{
					index += get_char_width(buf + index);
					pos++;

					if(index > real_len)
						index = len;

					if(pos > len + 1)
						pos = len + 1;

					wmove(view->win, view->curr_line, pos);
				}
				break;
            case KEY_HOME:
                if(index == 0)
                    break;

                pos = 1;
                index = 0;
                wmove(view->win, view->curr_line, pos);
                break;
            case KEY_END:
                if(index == len)
                    break;

                pos = get_utf8_string_length(buf) + 1;
                index = len;
                wmove(view->win, view->curr_line, pos);
                break;
            case ERR: /* timeout */
                break;
			default:
				{
                    size_t i, width;

                    if(key < 0x20) /* not a printable character */
                        break;

                    width = guess_char_width(key);
                    memmove(buf + index + width, buf + index, len - index + 1);
                    len += width;
                    for(i = 0; i < width; ++i)
                    {
                        buf[index++] = key;
                        if(index > 62)
                        {
                            abort = 1;
                            done = 1;
                            break;
                        }

                        len++;

                        if (i != width - 1) {
                            key = wgetch(view->win);
                        }
                    }
                    if(!done) {
                        mvwaddstr(view->win, view->curr_line, pos,
                                buf + index - width);
                        pos++;
                        wmove(view->win, view->curr_line, pos);
                    }
				}
				break;
		}
		curr_stats.getting_input = 0;
  }
	curs_set(0);

	if(abort)
	{
		load_dir_list(view, 1);
		moveto_list_pos(view, view->list_pos);
		return;
	}

	if(access(buf, F_OK) == 0 && strncmp(filename, buf, len) != 0)
	{
		show_error_msg("File exists", "That file already exists. Will not overwrite.");

		load_dir_list(view, 1);
		moveto_list_pos(view, view->list_pos);
		return;
	}
	snprintf(command, sizeof(command), "mv -f \'%s\' \'%s\'", filename, buf);

	my_system(command);

	load_dir_list(view, 0);
	found = find_file_pos_in_list(view, buf);
	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

void
remove_filename_filter(FileView *view)
{
	int found;
	char file[NAME_MAX];

	snprintf(file, sizeof(file), "%s",
			view->dir_entry[view->list_pos].name);
	view->prev_filter = (char *)realloc(view->prev_filter,
			strlen(view->filename_filter) +1);
	snprintf(view->prev_filter, 
		sizeof(view->prev_filter), "%s", view->filename_filter);
	view->filename_filter = (char *)realloc(view->filename_filter,
			strlen("*") +1);
	snprintf(view->filename_filter, 
			sizeof(view->filename_filter), "*");
	view->prev_invert = view->invert;
	view->invert = 0;
	load_dir_list(view, 0);
	found = find_file_pos_in_list(view, file);
	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

static void
restore_filename_filter(FileView *view)
{
	int found;
	char file[NAME_MAX];

	snprintf(file, sizeof(file), "%s", 
			view->dir_entry[view->list_pos].name);

	view->filename_filter = (char *)realloc(view->filename_filter,
			strlen(view->prev_filter) +1);
	snprintf(view->filename_filter, sizeof(view->filename_filter), 
			"%s", view->prev_filter);
	view->invert = view->prev_invert;
	load_dir_list(view, 0);
	found = find_file_pos_in_list(view, file); 
			

	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

static void
yank_files(FileView *view, int count, char *count_buf)
{
	int x;
	char buf[32];

	if(count)
	{
		int y = view->list_pos;

		for(x = 0; x < view->list_rows; x++)
			view->dir_entry[x].selected = 0;

		for(x = 0; x < atoi(count_buf); x++)
		{
			view->dir_entry[y].selected = 1;
			y++;
			if (y >= view->list_rows)
				break;
		}
		view->selected_files = y - view->list_pos;
	}
	else if(!view->selected_files)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	get_all_selected_files(view);
	yank_selected_files(view);
	free_selected_file_array(view);
	count = view->selected_files;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;

	view->selected_files = 0;

	draw_dir_list(view, view->top_line, view->list_pos); 
	moveto_list_pos(view, view->list_pos);
	snprintf(buf, sizeof(buf), " %d %s yanked.", count, 
			count == 1 ? "file" : "files");
	status_bar_message(buf);
}

static void
tag_file(FileView *view)
{
	if(view->dir_entry[view->list_pos].selected == 0)
	{
		/* The ../ dir cannot be selected */
		if (!strcmp(view->dir_entry[view->list_pos].name, "../"))
				return;

		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files++;
	}
	else
	{
		view->dir_entry[view->list_pos].selected = 0;
		view->selected_files--;
	}

	draw_dir_list(view, view->top_line, view->list_pos);
	wattron(view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
		mvwaddstr(view->win, view->curr_line, 0, " ");
	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR));
		wmove(view->win, view->curr_line, 0);
}

int
put_files_from_register(FileView *view)
{
	int x;
	int i = -1;
	int y = 0;
	char buf[PATH_MAX + (NAME_MAX * 2) + 4];

	for (x = 0; x < NUM_REGISTERS; x++)
	{
		if (reg[x].name == curr_stats.curr_register)
		{
			i = x;
			break;
		}
	}

	if ((i < 0) || (reg[i].num_files < 1))
	{
		status_bar_message("Register is empty");
		wrefresh(status_bar);
		return 1;
	}

	for (x = 0; x < reg[i].num_files; x++)
	{
		char *temp = NULL;
		char *temp1 = NULL;
		snprintf(buf, sizeof(buf), "%s/%s", cfg.trash_dir, reg[i].files[x]);
		temp = escape_filename(buf, strlen(buf), 1);
		temp1 = escape_filename(view->curr_dir, 
			  strlen(view->curr_dir), 1);
		if (!access(buf, F_OK))
		{

			snprintf(buf, sizeof(buf), "mv %s %s",
					temp, temp1);
			/*
			snprintf(buf, sizeof(buf), "mv \"%s/%s\" %s",
					cfg.trash_dir, temp, temp1);
					*/
			/*
			snprintf(buf, sizeof(buf), "mv \"%s/%s\" %s",
					cfg.trash_dir, reg[i].files[x], view->curr_dir);
					*/
			if ( background_and_wait_for_errors(buf))
				y++;
		}
		my_free(temp);
		my_free(temp1);
	}

	clear_register(curr_stats.curr_register);
	curr_stats.use_register  = 0;
	curr_stats.register_saved = 0;

	if (y)
	{
		snprintf(buf, sizeof(buf), " %d %s inserted", y,
				y==1 ? "file" : "files");

		load_dir_list(view, 0);
		moveto_list_pos(view, view->curr_line);

		status_bar_message(buf);
		return 1;
	}

	return 0;
}

int 
put_files(FileView *view)
{
	char command[NAME_MAX];
	char directory[PATH_MAX];
	int x;
	char buf[PATH_MAX * 2 + (NAME_MAX * 2) + 4];
	int y = 0;

	if (curr_stats.use_register && curr_stats.register_saved)
		return put_files_from_register(view);

	if (!curr_stats.num_yanked_files)
		return 0;

	if (!strcmp(curr_stats.yanked_files_dir, cfg.trash_dir))
	{
		snprintf(command, sizeof(command), "mv");
		snprintf(directory, sizeof(directory), "%s",
				cfg.trash_dir);
	}
	else
	{
		snprintf(command, sizeof(command), "cp -pR");
		snprintf(directory, sizeof(directory), "%s",
				curr_stats.yanked_files_dir);
	}

	for(x = 0; x < curr_stats.num_yanked_files; x++)
	{
		char *temp1 = NULL;
		char *temp2 = NULL;

		snprintf(buf, sizeof(buf), "%s/%s", directory, 
				curr_stats.yanked_files[x]);

		temp1 = escape_filename(buf, strlen(buf), 1);
		temp2 = escape_filename(view->curr_dir, 
				strlen(view->curr_dir), 1);

		if (!access(directory, F_OK))
		{
			snprintf(buf, sizeof(buf), "%s %s %s",
					command, temp1, temp2);
			if ( background_and_wait_for_errors(buf))
				y++;
		}

		my_free(temp1);
		my_free(temp2);
	}

	if (y)
	{
		snprintf(buf, sizeof(buf), " %d %s inserted", y,
				y==1 ? "file" : "files");

		load_dir_list(view, 0);
		moveto_list_pos(view, view->curr_line);

		status_bar_message(buf);

		return 1;
	}

	return 0;
}

void
show_dot_files(FileView *view)
{
	int found;
	char file[256];

	snprintf(file, sizeof(file), "%s", 
			view->dir_entry[view->list_pos].name);
	view->hide_dot = 0;
	load_dir_list(view, 1);
	found = find_file_pos_in_list(view, file);

	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

static void
hide_dot_files(FileView *view)
{
	int found;
	char file[NAME_MAX];

	snprintf(file, sizeof(file), "%s",
			view->dir_entry[view->list_pos].name);
	view->hide_dot = 1;
	load_dir_list(view, 1);
	found = find_file_pos_in_list(view, file);

	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

static void
toggle_dot_files(FileView *view)
{
	int found;
	char file[NAME_MAX];

	snprintf(file, sizeof(file), "%s",
			view->dir_entry[view->list_pos].name);
	if(view->hide_dot)
		view->hide_dot = 0;
	else
		view->hide_dot = 1;
	load_dir_list(view, 1);
	found = find_file_pos_in_list(view, file);

	if(found >= 0)
		moveto_list_pos(view, found);
	else
		moveto_list_pos(view, view->list_pos);
}

void
change_window(FileView **view)
{
	switch_views();
	*view = curr_view;

	if (curr_stats.number_of_windows != 1)
	{
		wattroff(other_view->title, A_BOLD);
		wattroff(other_view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
		mvwaddstr(other_view->win, other_view->curr_line, 0, "*");
		erase_current_line_bar(other_view);
		werase(other_view->title);
		wprintw(other_view->title, "%s", other_view->curr_dir);
		wnoutrefresh(other_view->title);
	}

	if (curr_stats.view)
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
	wprintw(curr_view->title,  "%s", curr_view->curr_dir);
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
filter_selected_files(FileView *view)
{
	size_t buf_size = 0;
	int x;

	if(!view->selected_files)
		view->dir_entry[view->list_pos].selected = 1;

	for(x = 0; x < view->list_rows; x++)
	{
		if(view->dir_entry[x].selected)
		{
			if(view->filtered)
			{
				char *buf = NULL;

				buf_size = strlen(view->dir_entry[x].name) +7;
				buf = (char *)realloc(buf, strlen(view->dir_entry[x].name) +7);
				snprintf(buf, buf_size,
						"|\\<%s\\>$", view->dir_entry[x].name);
				view->filename_filter = (char *)
					realloc(view->filename_filter, strlen(view->filename_filter) +
							strlen(buf) +1);
				strcat(view->filename_filter, buf);
				view->filtered++;
				my_free(buf);
			}
			else
			{
				buf_size = strlen(view->dir_entry[x].name) +6;
				view->filename_filter = (char *)
					realloc(view->filename_filter, strlen(view->dir_entry[x].name) +6);
				snprintf(view->filename_filter, buf_size,
						"\\<%s\\>$", view->dir_entry[x].name);
				view->filtered = 1;
			}
		}
	}
	view->invert = 1;
	clean_status_bar(view);
	load_dir_list(view, 1);
	moveto_list_pos(view, 0);
}

void
update_all_windows(void)
{
	/* In One window view */
	if (curr_stats.number_of_windows == 1)
	{
		if (curr_view == &lwin)
		{
			touchwin(lwin.title);
			touchwin(lwin.win);
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
			redrawwin(lwin.title);
			redrawwin(lwin.win);
			redrawwin(num_win);
			redrawwin(rborder);

			wnoutrefresh(lwin.title);
			wnoutrefresh(lwin.win);
		}
		else
		{
			touchwin(rwin.title);
			touchwin(rwin.win);
			touchwin(lborder);
			touchwin(stat_win);
			touchwin(status_bar);
			touchwin(pos_win);
			touchwin(num_win);
			touchwin(rborder);

			redrawwin(rwin.title);
			redrawwin(rwin.win);
			redrawwin(lborder);
			redrawwin(stat_win);
			redrawwin(status_bar);
			redrawwin(pos_win);
			redrawwin(num_win);
			redrawwin(rborder);

			wnoutrefresh(rwin.title);
			wnoutrefresh(rwin.win);
		}
	}
	/* Two Pane View */
	else
	{
		touchwin(lwin.title);
		touchwin(lwin.win);
		touchwin(mborder);
		touchwin(rwin.title);
		touchwin(rwin.win);
		touchwin(lborder);
		touchwin(stat_win);
		touchwin(status_bar);
		touchwin(pos_win);
		touchwin(num_win);
		touchwin(rborder);

		redrawwin(lwin.title);
		redrawwin(lwin.win);
		redrawwin(mborder);
		redrawwin(rwin.title);
		redrawwin(rwin.win);
		redrawwin(lborder);
		redrawwin(stat_win);
		redrawwin(status_bar);
		redrawwin(pos_win);
		redrawwin(num_win);
		redrawwin(rborder);

		wnoutrefresh(lwin.title);
		wnoutrefresh(lwin.win);
		wnoutrefresh(mborder);
		wnoutrefresh(rwin.title);
		wnoutrefresh(rwin.win);
	}

	wnoutrefresh(lborder);
	wnoutrefresh(stat_win);
	wnoutrefresh(status_bar);
	wnoutrefresh(pos_win);
	wnoutrefresh(num_win);
	wnoutrefresh(rborder);

	doupdate();
}



/*
 *  Main Loop 
 * 	Everything is driven from this function with the exception of 
 * 	signals which are handled in signals.c
 */
void
main_key_press_cb(FileView *view)
{
	int done = 0;
	int reset_last_char = 0;
	int save_count = 0;
	int count = 0;
	int key = 0;
	char count_buf[64] = "";
	char status_buf[64] = "";
	int save_reg = 0;

	curs_set(0);

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR));
	
	wtimeout(curr_view->win, KEYPRESS_TIMEOUT);
	wtimeout(other_view->win, KEYPRESS_TIMEOUT);

	update_stat_window(view);

	if (view->selected_files)
	{
		snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
				view->selected_files, view->selected_files == 1 ? "File" :
				"Files");
		status_bar_message(status_buf);
	}

	while (!done)
	{
		if (curr_stats.freeze)
			continue;

		/* Everything from here to if (key == ERR) gets called once a second */

		check_if_filelists_have_changed(view);
		check_background_jobs();

		if (!curr_stats.save_msg)
		{
			clean_status_bar(view);
			wrefresh(status_bar);
		}

		/* This waits for 1 second then skips if no keypress. */
		key = wgetch(view->win);


		if (key == ERR)
			continue;
		else /* Puts the cursor at the start of the line for speakup */
		{
			/*
			int x, y;
			char buf[256];

			getyx(view->win, y, x);
			
			snprintf(buf, sizeof(buf), "x is %d y is %d ", x, y);
			show_error_msg("cursor curr_win", buf);

			wmove(stdscr, y + 2, 0);
			wrefresh(stdscr);

			getyx(stdscr, y, x);
			snprintf(buf, sizeof(buf), "x is %d y is %d ", x, y);
			show_error_msg("stdscr win", buf);
			*/
		}

		/* This point down gets called only when a key is actually pressed */

		curr_stats.save_msg = 0;

		if (curr_stats.use_register && !curr_stats.register_saved)
		{
			if (is_valid_register(key))
			{
				curr_stats.curr_register = key;
				curr_stats.register_saved = 1;
				save_reg = 1;
				continue;
			}
			else
			{
				status_bar_message("Invalid Register Key");
				curr_stats.save_msg = 1;
				wrefresh(status_bar);
				curr_stats.use_register = 0;
				curr_stats.curr_register = -1;
				curr_stats.register_saved = 0;
				save_reg = 0;
				continue;
			}
		}
		else if((key > 47) && (key < 58)) /* ascii 0 - 9 */
		{
			if (count > 62)
			{
				show_error_msg(" Number is too large ", 
						"Vifm cannot handle that large of a number as a count. ");
				clear_num_window();
				continue;
			}

			count_buf[count] = key;
			count++;
			count_buf[count] = '\0';
			update_num_window(count_buf);
			continue;
		}
		else
			clear_num_window();
	  
		switch(key)
		{
			case '"': /* "register */
				curr_stats.use_register = 1;
				break;
			case 2: /* ascii Ctrl B */
			case KEY_PPAGE:
				view->list_pos = view->list_pos - view->window_rows;
				moveto_list_pos(view, view->list_pos);
				break;
			case 3: /* ascii Ctrl C */
			case 27: /* ascii Escape */
				{
					int x;

					for(x = 0; x < view->list_rows; x++)
						view->dir_entry[x].selected = 0;

					view->selected_files = 0;
					redraw_window();
					curs_set(0);
				}
				break;
			case 4: /* ascii Ctrl D */
				view->list_pos = view->list_pos + view->window_rows/2;
				moveto_list_pos(view, view->list_pos);
                break;
			case 6: /* ascii Ctrl F */
			case KEY_NPAGE:
				view->list_pos = view->list_pos + view->window_rows;
				moveto_list_pos(view, view->list_pos);
				break;
			case 7: /* ascii Ctrl G */
				if(!curr_stats.show_full)
					curr_stats.show_full = 1;
				break;
			case 9: /* ascii Tab */
			case 32:  /* ascii Spacebar */
				change_window(&view);
				break;
			case 12: /* ascii Ctrl L - clear screen and redraw */
				redraw_window();
				curs_set(0);
				break;
			case 13: /* ascii Return */
				handle_file(view, 0);
				break;
			case 21: /* ascii Ctrl U */
				view->list_pos = view->list_pos - view->window_rows/2;
				moveto_list_pos(view, view->list_pos);
				break;
			case 23: /* ascii Ctrl W - change windows */
				{
					int letter;
					curr_stats.getting_input = 1;
					letter = wgetch(view->win);
					curr_stats.getting_input = 0;

					if((letter == 'h') && (view->win == rwin.win))
						change_window(&view);
					else if((letter == 'l') && (view->win == lwin.win))
						change_window(&view);
				}
				break;
			case '.': /* repeat last change */
				repeat_last_command(view);
				break;
			case ':': /* command */
				curr_stats.save_msg = get_command(view, GET_COMMAND, NULL);
				break;
			case '/': /* search */
				curr_stats.save_msg = get_command(view, GET_SEARCH_PATTERN, NULL);
				break;
			case '?': /* search backwards */
				break;
			case '\'': /* mark */
                update_num_window("'");
				curr_stats.save_msg = get_bookmark(view);
                clear_num_window();
				break;
			case '%': /* Jump to percent of file. */
				if(count)
				{
					int percent = atoi(count_buf);
					int line =  (percent * (view->list_rows)/100);
					moveto_list_pos(view, line -1);
					reset_last_char = 1;
				}
				break;
			case 'G': /* Jump to bottom of list. */
				{
					if(count)
						moveto_list_pos(view, atoi(count_buf) -1);
					else
						moveto_list_pos(view, view->list_rows - 1);
					reset_last_char = 1;
				}
				break;
   		/* tbrown */
			case 'H': /* go to first file in window */
					view->list_pos = view->top_line;
					moveto_list_pos(view, view->list_pos);
					reset_last_char =1;
					break;
			/* tbrown */
			case 'L': /* go to last file in window */
					view->list_pos = view->top_line + view->window_rows;
					moveto_list_pos(view, view->list_pos);
					reset_last_char =1;
					break;
			case 'M': /* zM Restore filename filter and hide dot files. */
				if(curr_stats.last_char == 'z')
				{
					restore_filename_filter(view);
					hide_dot_files(view);
					reset_last_char = 1;
				}
				else 
				{  /* tbrown go to middle of window */
           if (view->list_rows<view->window_rows)
					 {
               view->list_pos = view->list_rows/2;
           } 
					 else 
					 {
               view->list_pos = view->top_line + (view->window_rows/2);
           }
					moveto_list_pos(view, view->list_pos);
					reset_last_char = 1;
        }
				break;
			case 'N':
				find_previous_pattern(view);
				break;
			case 'O': /* zO Remove filename filter. */
				if(curr_stats.last_char == 'z')
					remove_filename_filter(view);
				reset_last_char = 1;
				break;
			case 'R': /* zR Show all hidden files */
				{
					if(curr_stats.last_char == 'z')
					{
						remove_filename_filter(view);
						show_dot_files(view);
					}
					reset_last_char = 1;
				}
				break;
			case 'a': /* zo Show dot files */
				if(curr_stats.last_char == 'z')
					toggle_dot_files(view);
				reset_last_char = 1;
				break;
			case 'c': /* cw change word */
				{
					save_count = 1;
					update_num_window("c");
				}
				break;
			case 'd': /* dd  delete file */
				{
					save_count = 1;
					update_num_window("d");
					if(curr_stats.last_char == 'd')
					{
						clear_num_window();
						if(view->selected_files)
							delete_file(view);
						else if(count)
						{
							int x;
							int y = view->list_pos;
							for(x = 0; x < atoi(count_buf); x++)
							{
								view->dir_entry[y].selected = 1;
								y++;
							}
							delete_file(view);
						}
						else
							delete_file(view);
						reset_last_char = 1;
					}
				}
				break;
			case 'f': /* zf filter selected files */
					if(curr_stats.last_char == 'z')
						filter_selected_files(view);
					break;
			case 'g': /* gg   Jump to top of the list. */
				{
					save_count = 1;
					if(curr_stats.last_char == 'g')
					{
						if(count)
							moveto_list_pos(view, atoi(count_buf) -1);
						else
							moveto_list_pos(view, 0);

						reset_last_char = 1;
					}
				}
				break;
			case KEY_LEFT:
			case 'h': /* updir */
				{
					change_directory(view, "../");
					load_dir_list(view, 0);
					moveto_list_pos(view, view->list_pos);
				}
				break;
            case 'i': /* edit file even thought it's executable */
				handle_file(view, 1);
                break;
			case KEY_DOWN:
			case 'j': /* Move down one line */
				{
					if(count)
						view->list_pos += atoi(count_buf);
					else
						view->list_pos++;

					moveto_list_pos(view, view->list_pos);
					reset_last_char =1;
				}
				break;
			case KEY_UP:
			case 'k': /* Move up one line */
				{
					if(count)
						view->list_pos -= atoi(count_buf);
					else
						view->list_pos--;

					moveto_list_pos(view, view->list_pos);
					reset_last_char = 1;
				}
				break;
			case KEY_RIGHT:
			case 'l':
				handle_file(view, 0);
				break;
			case 'm': /*  'm' set mark  and 'zm' hide dot files */
				{
					if(curr_stats.last_char == 'z')
					{
						hide_dot_files(view);
						reset_last_char = 1;
					}
					else
					{
						int mark;
						curr_stats.getting_input = 1;

                        update_num_window("m");

						wtimeout(curr_view->win, -1);
						mark = wgetch(view->win);
						wtimeout(curr_view->win, KEYPRESS_TIMEOUT);
						curr_stats.getting_input = 0;
                        clear_num_window();
						if(key == ERR)
							continue;
						add_bookmark(mark, view->curr_dir, 
							get_current_file_name(view));
					}
				}
				break;
			case 'n':
				find_next_pattern(view);
				break;
			case 'o': /* zo Show dot files */
				if(curr_stats.last_char == 'z')
					show_dot_files(view);
				reset_last_char = 1;
				break;
			case 'p': /* put files */
				curr_stats.save_msg = put_files(view);
/*_SZ_BEGIN*/
/*after pasting files inside a FUSE mounted dir the view was not picking up the change*/
				redraw_window();
/*_SZ_END*/
				break;
			case 's': /* tmp shellout **** This should be done with key mapping */
				shellout(NULL, 0);
				break;
			case 't': /* Tag file. */
				tag_file(view);
				break;
				/* tbrown */
			case 'V':
			case 'v': /* Visual selection of files. */
				curr_stats.save_msg = start_visual_mode(view);
				break;
			case 'w': /* cw change word */
				{
					if (curr_stats.last_char == 'c')
						rename_file(view);
				}
				break;
          /* tbrown */
			case 'Y': /* Y yank file */
					yank_files(view, count, count_buf);
					reset_last_char++;
					curr_stats.save_msg = 1;
					break;

			case 'y': /* yy yank file */
				{
					if(curr_stats.last_char == 'y')
					{
						yank_files(view, count, count_buf);
						reset_last_char++;
						curr_stats.save_msg = 1;
						save_reg = 0;
					}
					else
					{
						update_num_window("y");
						save_reg = 1;
					}
					save_count = 1;
				}
				break;
			case 'z': /* zz redraw with file in center of list */
				if(curr_stats.last_char == 'z')
				{

				}
                else
                    update_num_window("z");
				break;
			case 'Q': /* ZQ quit */
			case 'Z': /* ZZ quit */
				{
					if(curr_stats.last_char == 'Z')
					{
                        comm_quit();
                    }
                    else
                        if(key == 'Z')
                            update_num_window("Z");
                }
                break;
			default:
				break;
		} /* end of switch(key) */

		curr_stats.last_char = key;

		if(!save_count)
			count = 0;

		if(reset_last_char)
		{
			curr_stats.last_char = 0;
			reset_last_char = 0;
			count = 0;
		}

		if(curr_stats.show_full)
			show_full_file_properties(view);
		else
			update_stat_window(view);

		if(view->selected_files)
		{
			static int number = 0;
			if(number != view->selected_files)
			{
				snprintf(status_buf, sizeof(status_buf), "%d %s Selected",
						view->selected_files, view->selected_files == 1 ? "File" :
						"Files");
				status_bar_message(status_buf);
				curr_stats.save_msg = 1;
			}
		}

		else if(!curr_stats.save_msg)
			clean_status_bar(view);

		if (curr_stats.use_register && curr_stats.register_saved)
		{
			if (!save_reg)
			{
				curr_stats.use_register = 0;
				curr_stats.curr_register = -1;
				curr_stats.register_saved = 0;
			}
		}

		if(curr_stats.need_redraw)
			redraw_window();

		update_all_windows();
	
	} /* end of while(!done)  */
}
