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

#if(defined(BSD) && (BSD>=199103))
	#include<sys/types.h> /* required for regex.h on FreeBSD 4.2 */
#endif

#include<ncurses.h>
#include<unistd.h> /* chdir() */
#include<stdlib.h> /* malloc  qsort */
#include<sys/stat.h> /* stat */
#include<sys/time.h> /* localtime */
#include<sys/wait.h> /* WEXITSTATUS */
#include<time.h>
#include<regex.h>
#include<dirent.h> /* DIR */
#include<string.h> /* strcat() */
#include<pwd.h>
#include<grp.h>

#include "background.h"
#include "color_scheme.h"
#include "config.h" /* menu colors */
#include "filelist.h"
#include "fileops.h"
#include "menus.h"
#include "sort.h"
#include "status.h"
#include "ui.h"
#include "utils.h" /* update_term_title() */
#include "fileops.h"
#include "file_info.h"

void
friendly_size_notation(int num, int str_size, char *str)
{
	const char* iec_units[] = { "  B", "KiB", "MiB", "GiB", "TiB", "PiB" };
	const char* si_units[] = { " B", "KB", "MB", "GB", "TB", "PB" };
	const char** units;
	int u = 0;
	double d = num;

	if (cfg.use_iec_prefixes)
		units = iec_units;
	else
		units = si_units;

	while(d >= 1024.0 && u < (sizeof(iec_units)/sizeof(iec_units[0])))
	{
		d /= 1024.0;
		++u;
	}
	snprintf(str, str_size, "%.1f %s", d, units[u]);

}

static void
add_sort_type_info(FileView *view, int y, int x, int current_line)
{
	char buf[24];
	struct passwd *pwd_buf;
	struct group *grp_buf;
	struct tm *tm_ptr;

	switch(view->sort_type)
	{
		 case SORT_BY_OWNER_NAME:
			 if((pwd_buf = getpwuid(view->dir_entry[x].uid)) != NULL)
			 {
				 snprintf(buf, sizeof(buf), " %s", pwd_buf->pw_name);
				 break;
			 }
		 case SORT_BY_OWNER_ID:
			 snprintf(buf, sizeof(buf), " %d", (int) view->dir_entry[x].uid);
			 break;
		 case SORT_BY_GROUP_NAME:
			 if((grp_buf = getgrgid(view->dir_entry[x].gid)) != NULL)
			 {
				 snprintf(buf, sizeof(buf), " %s", grp_buf->gr_name);
				 break;
			 }
		 case SORT_BY_GROUP_ID:
			 snprintf(buf, sizeof(buf), " %d", (int) view->dir_entry[x].gid);
			 break;
		case SORT_BY_MODE:
			 {
				  if (S_ISREG (view->dir_entry[x].mode))
					{
						if((S_IXUSR &view->dir_entry[x].mode)
								|| (S_IXGRP &view->dir_entry[x].mode)
								|| (S_IXOTH &view->dir_entry[x].mode))

							snprintf(buf, sizeof(buf), " exe");
						else
							snprintf(buf, sizeof(buf), " reg");
					}
					else if(S_ISLNK(view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " link");
					else if (S_ISDIR (view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " dir");
					else if (S_ISCHR (view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " char");
					else if (S_ISBLK (view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " block");
					else if (S_ISFIFO (view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " fifo");
					else if (S_ISSOCK (view->dir_entry[x].mode))
						 snprintf(buf, sizeof(buf), " sock");
					else
						 snprintf(buf, sizeof(buf), "  ?  ");
				break;
			 }
		case SORT_BY_TIME_MODIFIED:
			 tm_ptr = localtime(&view->dir_entry[x].mtime);
			 strftime(buf, sizeof(buf), " %m/%d-%H:%M", tm_ptr);
			 break;
		case SORT_BY_TIME_ACCESSED:
			 tm_ptr = localtime(&view->dir_entry[x].atime);
			 strftime(buf, sizeof(buf), " %m/%d-%H:%M", tm_ptr);
			 break;
		case SORT_BY_TIME_CHANGED:
			 tm_ptr = localtime(&view->dir_entry[x].ctime);
			 strftime(buf, sizeof(buf), " %m/%d-%H:%M", tm_ptr);
			 break;
		case SORT_BY_NAME:
		case SORT_BY_EXTENSION:
		case SORT_BY_SIZE_ASCENDING:
		case SORT_BY_SIZE_DESCENDING:
		default:
			 {
				 char str[24] = "";

				 friendly_size_notation(view->dir_entry[x].size,
						 sizeof(str), str);

				 snprintf(buf, sizeof(buf), " %s", str);
			 }
			 break;
	}

	if (current_line)
		wattron(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme)
				| A_BOLD);

	mvwaddstr(view->win, y,
				view->window_width - strlen(buf), buf);

	if (current_line)
		wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme)
				| A_BOLD);
}

void
quick_view_file(FileView *view)
{
	FILE *fp;
	char line[1024];
	char buf[NAME_MAX];
	int x = 1;
	int y = 1;
	size_t print_width;

	print_width = get_real_string_width(view->dir_entry[view->list_pos].name,
			view->window_width - 6) + 6;
	snprintf(buf, print_width + 1, "File: %s",
			view->dir_entry[view->list_pos].name);

	wbkgdset(other_view->title, COLOR_PAIR(BORDER_COLOR + view->color_scheme));
	wbkgdset(other_view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme));
	wclear(other_view->win);
	wclear(other_view->title);
	wattron(other_view->win,  A_BOLD);
	mvwaddstr(other_view->win, x, y,  buf);
	wattroff(other_view->win, A_BOLD);
	x++;

	switch (view->dir_entry[view->list_pos].type)
	{
		case DIRECTORY:
				mvwaddstr(other_view->win, ++x, y, "File is a Directory");
			break;
		case DEVICE:
				mvwaddstr(other_view->win, ++x, y, "File is a Device");
			break;
		case SOCKET:
				mvwaddstr(other_view->win, ++x, y, "File is a Socket");
			break;
		default:
			{
				if((fp = fopen(view->dir_entry[view->list_pos].name, "r"))
						== NULL)
				{
					mvwaddstr(other_view->win, ++x, y,	"Cannot open file");
					return;
				}

				while(fgets(line, other_view->window_width, fp)
						&&	(x < other_view->window_rows - 2))
				{
					mvwaddstr(other_view->win, ++x, y,	line);
				}


				fclose(fp);
			}
			break;
	}
}

char *
get_current_file_name(FileView *view)
{
		return view->dir_entry[view->list_pos].name;
}

void
free_selected_file_array(FileView *view)
{
	if(view->selected_filelist)
	{
		int x;
		for(x = 0; x < view->selected_files; x++)
		{
			free(view->selected_filelist[x]);
		}
		free(view->selected_filelist);
		view->selected_filelist = NULL;
		view->selected_files = 0;
	}
}

/* If you use this function using the free_selected_file_array()
 * will clean up the allocated memory
 */
void
get_all_selected_files(FileView *view)
{
	size_t namelen;
	int x;
	int y = 0;

	for(x = 0; x < view->list_rows; x++)
	{
		y += view->dir_entry[x].selected;
	}
	view->selected_files = y;

	/* No selected files so just use the current file */
	if(view->selected_files == 0)
	{
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
	}

	view->selected_filelist =
		(char **)calloc(view->selected_files, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < view->list_rows; x++)
	{
		if(view->dir_entry[x].selected)
		{
			namelen = strlen(view->dir_entry[x].name);
			view->selected_filelist[y] = malloc(namelen +1);
			if(view->selected_filelist[y] == NULL)
			{
				show_error_msg(" Memory Error ", "Unable to allocate enough memory");
				return;
			}
			strcpy(view->selected_filelist[y],
					view->dir_entry[x].name);
			y++;
		}
	}
}

/* If you use this function using the free_selected_file_array()
 * will clean up the allocated memory
 */
void
get_selected_files(FileView *view, int count, int *indexes)
{
	size_t namelen;
	int x;
	int y = 0;

	view->selected_filelist = (char **)calloc(count, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < count; x++)
	{
		namelen = strlen(view->dir_entry[indexes[x]].name);
		view->selected_filelist[y] = malloc(namelen +1);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg(" Memory Error ", "Unable to allocate enough memory");
			return;
		}
		strcpy(view->selected_filelist[y], view->dir_entry[indexes[x]].name);
		y++;
	}

	view->selected_files = count;
}

int
find_file_pos_in_list(FileView *view, const char *file)
{
	int x;
	for(x = 0; x < view->list_rows; x++)
	{
		if(strcmp(view->dir_entry[x].name, file) == 0)
		{
			return x;
		}
	}
	return -1;
}

static void
update_view_title(FileView *view)
{
	size_t len;

	werase(view->title);

	len = get_utf8_string_length(view->curr_dir);
	if(view->window_width < len + 1)
	{ /* Truncate long directory names */
		char *ptr;

		ptr = view->curr_dir;
		while(view->window_width < len + 4)
		{
			len--;
			ptr += get_char_width(ptr);
		}

		wprintw(view->title, "...%s", ptr);
	}
	else
		wprintw(view->title, "%s", view->curr_dir);

	wnoutrefresh(view->title);
}

/* WARNING: unused parameter pos */
void
draw_dir_list(FileView *view, int top, int pos)
{
	int x;
	int y = 0;
	char file_name[view->window_width*2 -2];
	int LINE_COLOR;
	int bold = 1;
	int color_scheme = 0;

	color_scheme = check_directory_for_color_scheme(view->curr_dir);

	/*
	wattrset(view->title, COLOR_PAIR(BORDER_COLOR + color_scheme));
	wattron(view->title, COLOR_PAIR(BORDER_COLOR + color_scheme));
	*/
	if(view->color_scheme != color_scheme)
	{
		view->color_scheme = color_scheme;
		wbkgdset(view->title, COLOR_PAIR(BORDER_COLOR + color_scheme));
		wbkgdset(view->win, COLOR_PAIR(WIN_COLOR + color_scheme));
	}

	werase(view->win);

	update_view_title(view);

	/* This is needed for reloading a list that has had files deleted */
	while((view->list_rows - view->list_pos) <= 0)
	{
		view->list_pos--;
		view->curr_line--;
	}

	/* Show as much of the directory as possible. */
	if(view->window_rows >= view->list_rows)
		top = 0;
	else if((view->list_rows - top) <= view->window_rows)
	{
		top = view->list_rows - view->window_rows -1;
		view->curr_line++;
	}

	/* Colorize the files */

	for(x = top; x < view->list_rows; x++)
	{
		size_t print_width;
		/* Extra long file names are truncated to fit */

		print_width = get_real_string_width(view->dir_entry[x].name,
				view->window_width - 2) + 2;
		snprintf(file_name, print_width, "%s", view->dir_entry[x].name);

		wmove(view->win, y, 1);
		if(view->dir_entry[x].selected)
		{
			LINE_COLOR = SELECTED_COLOR + color_scheme;
		}
		else
		{
			switch(view->dir_entry[x].type)
			{
				case DIRECTORY:
					LINE_COLOR = DIRECTORY_COLOR + color_scheme;
					break;
				case LINK:
					LINE_COLOR = LINK_COLOR + color_scheme;
					break;
				case SOCKET:
					LINE_COLOR = SOCKET_COLOR + color_scheme;
					break;
				case DEVICE:
					LINE_COLOR = DEVICE_COLOR + color_scheme;
					break;
				case EXECUTABLE:
					LINE_COLOR = EXECUTABLE_COLOR + color_scheme;
					break;
				default:
					LINE_COLOR = WIN_COLOR + color_scheme;
					bold = 0;
					break;
			}
		}
		if(bold)
		{
			wattrset(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);
			wattron(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);
			wprintw(view->win, "%s", file_name);
			wattroff(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);

			add_sort_type_info(view, y, x, 0);
		}
		else
		{
			wattrset(view->win, COLOR_PAIR(LINE_COLOR));
			wattron(view->win, COLOR_PAIR(LINE_COLOR));
			wprintw(view->win, "%s", file_name);
			wattroff(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);

			add_sort_type_info(view, y, x, 0);
			bold = 1;
		}
		y++;
		if(y > view->window_rows)
			break;
	}

	if(view != curr_view)
		mvwaddstr(view->win, view->curr_line, 0, "*");

	view->top_line = top;
}

int
S_ISEXE(mode_t mode)
{
	if((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode))
		return 1;

	return 0;
}

void
erase_current_line_bar(FileView *view)
{
	int old_cursor = view->curr_line;
	int old_pos = view->top_line + old_cursor;
	char file_name[view->window_width*2 -2];
	int bold = 1;
	int LINE_COLOR;
	size_t print_width;

	/* Extra long file names are truncated to fit */

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme) | A_BOLD);
	if((old_pos > -1)  && (old_pos < view->list_rows))
	{
		print_width = get_real_string_width(view->dir_entry[old_pos].name,
				view->window_width - 2) + 2;
		snprintf(file_name, print_width, "%s",
				view->dir_entry[old_pos].name);
	}
	else /* The entire list is going to be redrawn so just return. */
		return;


	wmove(view->win, old_cursor, 1);

	wclrtoeol(view->win);

	if(view->dir_entry[old_pos].selected)
	{
		LINE_COLOR = SELECTED_COLOR + view->color_scheme;
	}
	else
	{
		switch(view->dir_entry[old_pos].type)
		{
			case DIRECTORY:
				LINE_COLOR = DIRECTORY_COLOR + view->color_scheme;
				break;
			case LINK:
				LINE_COLOR = LINK_COLOR + view->color_scheme;
				break;
			case SOCKET:
				LINE_COLOR = SOCKET_COLOR + view->color_scheme;
				break;
			case DEVICE:
				LINE_COLOR = DEVICE_COLOR + view->color_scheme;
				break;
			case EXECUTABLE:
				LINE_COLOR = EXECUTABLE_COLOR + view->color_scheme;
				break;
			default:
				LINE_COLOR = WIN_COLOR + view->color_scheme;
				bold = 0;
				break;
		}
	}
	if(bold)
	{
		wattrset(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);
		wattron(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);
		mvwaddnstr(view->win, old_cursor, 1, file_name, print_width);
		wattroff(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);

		add_sort_type_info(view, old_cursor, old_pos, 0);
	}
	else
	{
		wattrset(view->win, COLOR_PAIR(LINE_COLOR));
		wattron(view->win, COLOR_PAIR(LINE_COLOR));
		mvwaddnstr(view->win, old_cursor, 1, file_name, print_width);
		wattroff(view->win, COLOR_PAIR(LINE_COLOR) | A_BOLD);
		bold = 1;
		add_sort_type_info(view, old_cursor, old_pos, 0);
	}
}

void
moveto_list_pos(FileView *view, int pos)
{
	int redraw = 0;
	int old_cursor = view->curr_line;
	char file_name[view->window_width*2 + 1];
	size_t print_width;

	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows -1)
		pos = (view->list_rows -1);


	if(view->curr_line > view->list_rows -1)
		view->curr_line = view->list_rows -1;

	erase_current_line_bar(view);

	if((view->top_line <=  pos) && (pos <= (view->top_line + view->window_rows)))
	{
		view->curr_line = pos - view->top_line;
	}
	else if((pos > (view->top_line + view->window_rows)))
	{
		while(pos > (view->top_line + view->window_rows))
			view->top_line++;

		view->curr_line = view->window_rows;
		redraw = 1;
	}
	else if(pos < view->top_line)
	{
		while(pos < view->top_line)
			view->top_line--;

		view->curr_line = 0;
		redraw = 1;
	}

	view->list_pos = pos;



	if(redraw)
		draw_dir_list(view, view->top_line, view->curr_line);


	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme));
	mvwaddstr(view->win, old_cursor, 0, " ");

	wattron(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme) | A_BOLD);

	/* Blank the current line and
	 * print out the current line bar
	 */

	memset(file_name, ' ', view->window_width);

	file_name[view->window_width] = '\0';

	mvwaddstr(view->win, view->curr_line, 1, file_name);

	print_width = get_real_string_width(view->dir_entry[pos].name,
			view->window_width - 2) + 2;
	snprintf(file_name, print_width, " %s", view->dir_entry[pos].name);

	mvwaddstr(view->win, view->curr_line, 0, file_name);
	add_sort_type_info(view, view->curr_line, pos, 1);

	if(curr_stats.view)
		quick_view_file(view);

}


static int
regexp_filter_match(FileView *view,  char *filename)
{
	regex_t re;

	if(regcomp(&re, view->filename_filter, REG_EXTENDED) == 0)
	{
		if(regexec(&re, filename, 0, NULL, 0) == 0)
		{
			regfree(&re);
			return !view->invert;
		}
		regfree(&re);
		return view->invert;
	}
	regfree(&re);
	return 1;
}

void
save_view_history(FileView *view)
{
	int x;

	x = view->history_num;
	while(x >= 0 && view->history[x].dir[0] == '\0')
		x--;
	if(x != -1 && strcmp(view->history[x].dir, view->curr_dir) == 0)
	{
		snprintf(view->history[x].file, sizeof(view->history[x].file),
				"%s", view->dir_entry[view->list_pos].name);
		return;
	}

	x = view->history_num;

	if(x == cfg.history_len)
	{
		int y;
		for(y = 0; y < cfg.history_len - 1; y++)
		{
			strcpy(view->history[y].file, view->history[y + 1].file);
			strcpy(view->history[y].dir, view->history[y + 1].dir);
		}
		x--;
	}
	snprintf(view->history[x].dir, sizeof(view->history[x].dir), "%s",
			view->curr_dir);
	snprintf(view->history[x].file, sizeof(view->history[x].file), "%s",
			view->dir_entry[view->list_pos].name);
	view->history_num++;
}

static void
check_view_dir_history(FileView *view)
{
	int x = 0;
	int found = 0;
	int pos = 0;

	if(curr_stats.is_updir)
	{
		pos = find_file_pos_in_list(view, curr_stats.updir_file);
	}
	else
	{
		for(x = 0; x < view->history_num ; x++)
		{
			if(strlen(view->history[x].dir) < 1)
				break;
			if(!strcmp(view->history[x].dir, view->curr_dir))
			{
				found = 1;
				break;
			}
		}
		if(found)
		{
			pos = find_file_pos_in_list(view, view->history[x].file);
		}
		else
		{
			view->list_pos = 0;
			view->curr_line = 0;
			view->top_line = 0;
			return;
		}
	}

	if(pos < 0)
		pos = 0;
	view->list_pos = pos;
	if(view->list_pos <= view->window_rows)
	{
		view->top_line = 0;
		view->curr_line = view->list_pos;
	}
	else if(view->list_pos > (view->top_line + view->window_rows))
	{
		while(view->list_pos > (view->top_line + view->window_rows))
			view->top_line++;

		view->curr_line = view->window_rows;
	}
	return;
}

void
clean_selected_files(FileView *view)
{
	int x;
	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;
	view->selected_files = 0;
}

/*
 * The directory can either be relative to the current
 * directory - ../
 * or an absolute path - /usr/local/share
 * The *directory passed to change_directory() cannot be modified.
 * Symlink directories require an absolute path
 */
void
change_directory(FileView *view, const char *directory)
{
	DIR *dir;
	struct stat s;
	char newdir[PATH_MAX];
	char dir_dup[PATH_MAX];

	curr_stats.is_updir = 0;

	save_view_history(view);

	snprintf(dir_dup, PATH_MAX, "%s", directory);

	snprintf(view->last_dir, sizeof(view->last_dir), "%s", view->curr_dir);

 /*_SZ_BEGIN_*/
/* check if we're exiting from a FUSE mounted top level dir. If so, unmount & let FUSE serialize */
	if(!strcmp(directory, "../") && !memcmp(view->curr_dir, cfg.fuse_home, strlen(cfg.fuse_home)))
	{
		Fuse_List *runner = fuse_mounts, *trailer = NULL, *sniffer = NULL;
		int found = 0;
		char buf[8192];
		while(runner)
		{
			if(!strcmp(runner->mount_point, view->curr_dir))
			{
				found = 1;
				break;
			}
			trailer = runner;
			runner = runner->next;
		}
		if(found) /*true if we ARE exiting a top level dir*/
		{
			status_bar_message("FUSE unmounting selected file, please stand by..");
			snprintf(buf, sizeof(buf), "sh -c \"fusermount -u '%s'\"", runner->mount_point);
			//snprintf(buf, sizeof(buf), "sh -c \"pauseme PAUSE_ON_ERROR_ONLY fusermount -u '%s'\"", runner->mount_point);
			/*have to chdir to parent temporarily, so that this DIR can be unmounted*/
			chdir(cfg.fuse_home);
			/*
			def_prog_mode();
			endwin();
			my_system("clear");
			int status = my_system(buf);
			*/
			int status = background_and_wait_for_status(buf);
			/*check child status*/
			if( !WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)) )
			{
				werase(status_bar);
				show_error_msg("FUSE UMOUNT ERROR", runner->source_file_name);
				chdir(view->curr_dir);
				return;
			}
			/*remove the DIR we created for the mount*/
			if(!access(runner->mount_point, F_OK))
				rmdir(runner->mount_point);
			status_bar_message("FUSE mount success.");
			/*remove mount point from Fuse_List*/
			sniffer = runner->next;
			if(trailer)
			{
				if(sniffer)
					trailer->next = sniffer;
				else
					trailer->next = NULL;
			}
			else
				fuse_mounts = runner->next;

			change_directory(view, runner->source_file_dir);
			char *filen = runner->source_file_name;
			filen += strlen(runner->source_file_dir) + 1;
			found = find_file_pos_in_list(view, filen);
			moveto_list_pos(view, found);
			free(runner);
			return;
		}
	}
 /*_SZ_END_*/

	/* Moving up a directory */
	if(!strcmp(dir_dup, "../"))
	{
		char *str1, *str2;
		char * value = NULL;
		char * tok;

		curr_stats.is_updir = 1;
		str2 = str1 = view->curr_dir;

		while((str1 = strstr(str1, "/")) != NULL)
		{
			str1++;
			str2 = str1;
		}

		snprintf(curr_stats.updir_file, sizeof(curr_stats.updir_file),
				"%s/", str2);

		strcpy(newdir,"");

		tok =  strtok(view->curr_dir,"/");

		while (tok != NULL)
		{
			if (tok != NULL && value != NULL)
			{
				strcat(newdir,"/");
				strcat(newdir,value);
			}
			value = tok;
			tok = strtok(NULL, "/");
		}

		snprintf(dir_dup, PATH_MAX, "%s", newdir);

		if(!strcmp(dir_dup,""))
			strcpy(dir_dup,"/");
	}
	/* Moving into a directory	or bookmarked dir or :cd directory*/
	else if(strcmp(dir_dup, view->curr_dir))
	{
		/* directory is a relative path */
		if(dir_dup[0] != '/')
		{
			char * symlink_dir = "/";
			char * tok = strtok(dir_dup,"/");

			while (tok != NULL)
			{
				symlink_dir = tok;
				tok = strtok(NULL,"/");
			}

			strcpy(dir_dup, symlink_dir);
			strcat(dir_dup, "/");

			if (view->curr_dir[strlen(view->curr_dir)-1] != '/')
				strcat(view->curr_dir,"/");

			snprintf(newdir, PATH_MAX, "%s", view->curr_dir);
			strncat(newdir, dir_dup, strlen(dir_dup));

			//view->curr_dir[strlen(view->curr_dir) -1] = '\0';

			snprintf(dir_dup, PATH_MAX, "%s", newdir);
		}
		/* else  It is an absolute path and does not need to be modified
		 */

	}
	/* else -  should only happen when reloading a directory, changing views,
	 * or when starting up and should already be an absolute path.
	 */

	/* Clean up any excess separators */
	if((view->curr_dir[strlen(view->curr_dir) -1] == '/') &&
			(strcmp(view->curr_dir, "/")))
		view->curr_dir[strlen(view->curr_dir) - 1] = '\0';

	if((view->last_dir[strlen(view->last_dir) -1] == '/') &&
			(strcmp(view->last_dir, "/")))
		view->last_dir[strlen(view->last_dir) - 1] = '\0';


	if(access(dir_dup, F_OK) != 0)
	{
		show_error_msg(" Directory Access Error ",
				"Cannot open that directory ");
		change_directory(view, getenv("HOME"));
		clean_selected_files(view);
		return;
	}

	if(access(dir_dup, R_OK) != 0)
	{
		show_error_msg(" Directory Access Error ",
				"You do not have read access on that directory");

		clean_selected_files(view);
		return;
	}

	dir = opendir(dir_dup);

	if(dir == NULL)
	{
		show_error_msg("Dir is null", "Could not open directory. ");
		clean_selected_files(view);
		return;
	}

	if(chdir(dir_dup) == -1)
	{
		closedir(dir);
		status_bar_message("Couldn't open directory");
		return;
	}

	clean_selected_files(view);
	draw_dir_list(view, view->top_line, view->list_pos);

	/* Need to use setenv instead of getcwd for a symlink directory */
	setenv("PWD", dir_dup, 1);

	if(dir_dup[strlen(dir_dup) -1] == '/' && strcmp(dir_dup, "/"))
		dir_dup[strlen(dir_dup) - 1] = '\0';

	if(strcmp(dir_dup, view->curr_dir))
		snprintf(view->curr_dir, PATH_MAX, "%s", dir_dup);

	/* Save the directory modified time to check for file changes */
	stat(view->curr_dir, &s);
	view->dir_mtime = s.st_mtime;
	closedir(dir);

	save_view_history(view);
}

static void
reset_selected_files(FileView *view)
{
	int x;
	for(x = 0; x < view->selected_files; x++)
	{
		if(view->selected_filelist[x])
		{
			int pos = find_file_pos_in_list(view, view->selected_filelist[x]);
			if(pos >= 0 && pos < view->list_rows)
				view->dir_entry[pos].selected = 1;
		}
	}
	free_selected_file_array(view);
}


void
load_dir_list(FileView *view, int reload)
{
	DIR *dir;
	struct dirent *d;
	struct stat s;
	int x;
	int namelen = 0;
	int old_list = view->list_rows;

	dir = opendir(view->curr_dir);

	if(dir == NULL)
		return;


	view->filtered = 0;

	lstat(view->curr_dir, &s);
	view->dir_mtime = s.st_mtime;

	if(!reload && s.st_size > 2048)
	{
		status_bar_message("Reading Directory...");
	}

	update_all_windows();

	if(reload && view->selected_files)
		get_all_selected_files(view);


	if(view->dir_entry)
	{
		for(x = 0; x < old_list; x++)
		{
			free(view->dir_entry[x].name);
		}

		free(view->dir_entry);
		view->dir_entry = NULL;
	}
	view->dir_entry = (dir_entry_t *)malloc(sizeof(dir_entry_t));
	if(view->dir_entry == NULL)
	{
		show_error_msg(" Memory Error ", "Unable to allocate enough memory.");
		return;
	}

	for(view->list_rows = 0; (d = readdir(dir)); view->list_rows++)
	{
		/* Ignore the "." directory. */
		if(strcmp(d->d_name, ".") == 0)
		{
			view->list_rows--;
			continue;
		}
		/* Always include the ../ directory unless it is the root directory. */
		if(strcmp(d->d_name, "..") == 0)
		{
			if(!strcmp("/", view->curr_dir))
			{
				view->list_rows--;
				continue;
			}
		}
		if(!regexp_filter_match(view, d->d_name) && strcmp("..", d->d_name))
		{
			view->filtered++;
			view->list_rows--;
			continue;
		}

		if(d->d_name[0] == '.')
		{
			if((strcmp(d->d_name, "..")) && (view->hide_dot))
			{
				view->filtered++;
				view->list_rows--;
				continue;
			}
		}

		view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
				(view->list_rows + 1) * sizeof(dir_entry_t));
		if(view->dir_entry == NULL)
		{
			show_error_msg(" Memory Error ", "Unable to allocate enough memory");
			return ;
		}

		namelen = strlen(d->d_name);
		/* Allocate extra for adding / to directories. */
		view->dir_entry[view->list_rows].name = malloc(namelen + 2);
		if(view->dir_entry[view->list_rows].name == NULL)
		{
			show_error_msg(" Memory Error ", "Unable to allocate enough memory");
			return;
		}

		strcpy(view->dir_entry[view->list_rows].name, d->d_name);

		/* All files start as unselected */
		view->dir_entry[view->list_rows].selected = 0;

		/* Load the inode info */
		lstat(view->dir_entry[view->list_rows].name, &s);

		view->dir_entry[view->list_rows].size = (uintmax_t)s.st_size;
		view->dir_entry[view->list_rows].mode = s.st_mode;
		view->dir_entry[view->list_rows].uid = s.st_uid;
		view->dir_entry[view->list_rows].gid = s.st_gid;
		view->dir_entry[view->list_rows].mtime = s.st_mtime;
		view->dir_entry[view->list_rows].atime = s.st_atime;
		view->dir_entry[view->list_rows].ctime = s.st_ctime;

		if(s.st_ino)
		{
			switch(s.st_mode & S_IFMT)
			{
				case S_IFLNK:
					{
						if(check_link_is_dir(view, view->list_rows))
						{
							namelen = sizeof(view->dir_entry[view->list_rows].name);
							strcat(view->dir_entry[view->list_rows].name, "/");

						}
					view->dir_entry[view->list_rows].type = LINK;
					}
					break;
				case S_IFDIR:
					namelen = sizeof(view->dir_entry[view->list_rows].name);
					strcat(view->dir_entry[view->list_rows].name, "/");
					view->dir_entry[view->list_rows].type = DIRECTORY;
					break;
				case S_IFCHR:
				case S_IFBLK:
					view->dir_entry[view->list_rows].type = DEVICE;
					break;
				case S_IFSOCK:
					view->dir_entry[view->list_rows].type = SOCKET;
					break;
				case S_IFREG:
					if(S_ISEXE(s.st_mode))
					{
						view->dir_entry[view->list_rows].type = EXECUTABLE;
						break;
					}
					view->dir_entry[view->list_rows].type = REGULAR;
					break;
				default:
					view->dir_entry[view->list_rows].type = UNKNOWN;
				break;
			}
		}
	}

	closedir(dir);

	if(!reload && s.st_size > 2048)
	{
		status_bar_message("Sorting Directory...");
	}
	qsort(view->dir_entry, view->list_rows, sizeof(dir_entry_t),
				sort_dir_list);

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].list_num = x;

	/* If reloading the same directory don't jump to
	 * history position.  Stay at the current line
	 */
	if(!reload)
		check_view_dir_history(view);

	/*
	 * It is possible to set the file name filter so that no files are showing
	 * in the / directory.	All other directorys will always show at least the
	 * ../ file.  This resets the filter and reloads the directory.
	 */
	if(view->list_rows < 1)
	{
		char msg[64];
		snprintf(msg, sizeof(msg),
				"The %s pattern %s did not match any files. It was reset.",
				view->filename_filter, view->invert==1 ? "inverted" : "");
		status_bar_message(msg);
		view->filename_filter = (char *)realloc(view->filename_filter,
				strlen("*") +1);
		if(view->filename_filter == NULL)
		{
			show_error_msg(" Memory Error ", "Unable to allocate enough memory");
			return;
		}
		snprintf(view->filename_filter, sizeof(view->filename_filter), "*");
		if(view->invert)
			view->invert = 0;

		load_dir_list(view, 1);

		draw_dir_list(view, view->top_line, view->list_pos);
		return;
	}
	if(reload && view->selected_files)
		reset_selected_files(view);

	draw_dir_list(view, view->top_line, view->list_pos);

	return;
}


bool
is_link_dir(const dir_entry_t * path)
{
	struct stat s;
	stat(path->name, &s);

	//if(s.st_mode & S_IFMT == S_IFDIR) return true;
	if(s.st_mode & S_IFDIR)
		return true;
	else
		return false;
}

void
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
				free(buf);
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
	clean_status_bar();
	load_dir_list(view, 1);
	moveto_list_pos(view, 0);
}

void
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

void
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

void
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

void
scroll_view(FileView *view)
{
	draw_dir_list(view, view->top_line, view->curr_line);
	moveto_list_pos(view, view->list_pos);
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
void
check_if_filelists_have_changed(FileView *view)
{
	struct stat s;

	stat(view->curr_dir, &s);
	if(s.st_mtime  != view->dir_mtime)
		reload_window(view);

	if(curr_stats.number_of_windows != 1 && curr_stats.view != 1)
	{
		stat(other_view->curr_dir, &s);
		if(s.st_mtime != other_view->dir_mtime)
			reload_window(other_view);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
