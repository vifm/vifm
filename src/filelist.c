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
	#include <sys/types.h> /* required for regex.h on FreeBSD 4.2 */
#endif

#include <curses.h>
#include <unistd.h> /* chdir() */
#include <stdlib.h> /* malloc  qsort */
#include <sys/stat.h> /* stat */
#include <sys/time.h> /* localtime */
#include <sys/wait.h> /* WEXITSTATUS */
#include <time.h>
#include <regex.h>
#include <dirent.h> /* DIR */
#include <string.h> /* strcat() */
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#include "background.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h" /* menu colors */
#include "file_info.h"
#include "filelist.h"
#include "fileops.h"
#include "fileops.h"
#include "filetype.h"
#include "log.h"
#include "menus.h"
#include "options.h"
#include "sort.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "utils.h"

static void
add_sort_type_info(FileView *view, int y, int x, int is_current_line)
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
				if(S_ISREG(view->dir_entry[x].mode))
				{
					if((S_IXUSR & view->dir_entry[x].mode)
							|| (S_IXGRP & view->dir_entry[x].mode)
							|| (S_IXOTH & view->dir_entry[x].mode))
						snprintf(buf, sizeof(buf), " exe");
					else
						snprintf(buf, sizeof(buf), " reg");
				}
				else if(S_ISLNK(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " link");
				else if(S_ISDIR(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " dir");
				else if(S_ISCHR(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " char");
				else if(S_ISBLK(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " block");
				else if(S_ISFIFO(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " fifo");
				else if(S_ISSOCK(view->dir_entry[x].mode))
					snprintf(buf, sizeof(buf), " sock");
				else
					snprintf(buf, sizeof(buf), "  ?  ");
				break;
			}
		case SORT_BY_TIME_MODIFIED:
			tm_ptr = localtime(&view->dir_entry[x].mtime);
			strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
			break;
		case SORT_BY_TIME_ACCESSED:
			tm_ptr = localtime(&view->dir_entry[x].atime);
			strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
			break;
		case SORT_BY_TIME_CHANGED:
			tm_ptr = localtime(&view->dir_entry[x].ctime);
			strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
			break;
		case SORT_BY_NAME:
		case SORT_BY_EXTENSION:
		case SORT_BY_SIZE:
		default:
			{
				unsigned long long size = 0;
				char str[24] = "";

				if(view->dir_entry[x].type == DIRECTORY && chdir(view->curr_dir) == 0)
					tree_get_data(curr_stats.dirsize_cache, view->dir_entry[x].name,
							&size);

				if(size == 0)
					size = view->dir_entry[x].size;

				friendly_size_notation(size, sizeof(str), str);
				snprintf(buf, sizeof(buf), " %s", str);
			}
			break;
	}

	if(is_current_line)
	{
		if(cfg.invert_cur_line)
			wattron(view->win, COLOR_PAIR(CURRENT_COLOR + view->color_scheme)
					| A_BOLD | A_REVERSE);
		else
			wattron(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme)
					| A_BOLD);
	}

	mvwaddstr(view->win, y, view->window_width - strlen(buf), buf);

	if(is_current_line)
	{
		if(cfg.invert_cur_line)
			wattroff(view->win, COLOR_PAIR(CURRENT_COLOR + view->color_scheme)
					| A_BOLD | A_REVERSE);
		else
			wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme)
					| A_BOLD);
	}
}

static FILE *
use_info_prog(char *cmd)
{
	pid_t pid;
	int error_pipe[2];
	int use_menu = 0, split = 0;

	if(strchr(cmd, '%') == NULL)
	{
		char *escaped = escape_filename(
				curr_view->dir_entry[curr_view->list_pos].name, 0, 0);
		char *t = malloc(strlen(cmd) + 1 + strlen(escaped) + 1);
		sprintf(t, "%s %s", cmd, escaped);
		cmd = t;
	}
	else
	{
		cmd = expand_macros(curr_view, cmd, NULL, &use_menu, &split);
	}

	if(pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		return NULL;
	}

	if((pid = fork()) == -1)
		return NULL;

	if(pid == 0)
	{
		run_from_fork(error_pipe, 0, cmd);
		free(cmd);
		return NULL;
	}
	else
	{
		FILE * f;
		close(error_pipe[1]); /* Close write end of pipe. */
		free(cmd);
		f = fdopen(error_pipe[0], "r");
		if(f == NULL)
			close(error_pipe[0]);
		return f;
	}
}

static void
view_not_wraped(FILE *fp, int x)
{
	char line[1024];
	int y = 1;

	while(fgets(line, other_view->window_width - 2, fp) == line &&
			x < other_view->window_rows - 2)
	{
		int i;
		size_t n_len = get_normal_utf8_string_length(line);
		size_t len = strlen(line);
		while(n_len < other_view->window_width - 1 && line[len - 1] != '\n'
					&& !feof(fp))
		{
			if(fgets(line + len, other_view->window_width - n_len, fp) == NULL)
				break;
			n_len = get_normal_utf8_string_length(line);
			len = strlen(line);
		}

		if(line[len - 1] != '\n')
			while(fgetc(fp) != '\n' && !feof(fp));

		len = 0;
		n_len = 0;
		for(i = 0; line[i] != '\0' && len <= other_view->window_width - 1;)
		{
			size_t cl = get_char_width(line + i);
			n_len++;

			if(cl == 1 && (unsigned char)line[i] == '\t')
				len += 8 - (y + len)%8;
			else if(cl == 1 && (unsigned char)line[i] < ' ')
				len += 2;
			else
				len++;

			if(len <= other_view->window_width - 1)
				i += cl;
		}
		line[i] = '\0';

		++x;
		wmove(other_view->win, x, y);
		i = 0;
		while(n_len--)
		{
			waddstr(other_view->win, strchar2str(line + i));
			i += get_char_width(line + i);
		}
	}
}

static void
view_wraped(FILE *fp, int x)
{
	char line[1024];
	int y = 1;
	int offset = 0;
	while(fgets(line + offset, other_view->window_width, fp)
				&& x < other_view->window_rows - 2)
	{
		int i, k;
		size_t width;
		size_t n_len = get_normal_utf8_string_length(line);
		size_t len = strlen(line);
		while(n_len < other_view->window_width - 1 && line[len - 1] != '\n'
				&& !feof(fp))
		{
			if(fgets(line + len, other_view->window_width - n_len, fp) == NULL)
				break;
			n_len = get_normal_utf8_string_length(line);
			len = strlen(line);
		}

		if(line[len - 1] != '\n')
		{
			int c = fgetc(fp);
			if(c != '\n')
				ungetc(c, fp);
		}

		width = get_normal_utf8_string_width(line);
		++x;
		wmove(other_view->win, x, y);
		i = 0;
		k = width;
		while(k--)
		{
			waddstr(other_view->win, strchar2str(line + i));
			i += get_char_width(line + i);
		}

		offset = strlen(line) - width;
		if(offset != 0)
			memmove(line, line + width, offset);
	}
}

void
quick_view_file(FileView *view)
{
	FILE *fp;
	int x = 0;
	int y = 1;
	size_t print_width;

	print_width = get_real_string_width(view->dir_entry[view->list_pos].name,
			view->window_width - 6) + 6;

	wbkgdset(other_view->title, COLOR_PAIR(BORDER_COLOR + view->color_scheme));
	wbkgdset(other_view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme));
	wclear(other_view->win);
	wclear(other_view->title);
	mvwaddstr(other_view->title, 0, 0, "File: ");
	waddstr(other_view->title, view->dir_entry[view->list_pos].name);
	wattron(other_view->win,  A_BOLD);
	wattroff(other_view->win, A_BOLD);

	switch(view->dir_entry[view->list_pos].type)
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
		case UNKNOWN:
			if(S_ISFIFO(view->dir_entry[view->list_pos].mode))
			{
				mvwaddstr(other_view->win, ++x, y, "File is a Named Pipe");
				break;
			}
			/* break omitted */
		default:
			{
				char *viewer;

				viewer = get_viewer_for_file(view->dir_entry[view->list_pos].name);
				if(viewer != NULL && viewer[0] != '\0')
					fp = use_info_prog(viewer);
				else
					fp = fopen(view->dir_entry[view->list_pos].name, "r");

				if(fp == NULL)
				{
					mvwaddstr(other_view->win, x, y, "Cannot open file");
					break;
				}

				if(cfg.wrap_quick_view)
					view_wraped(fp, x);
				else
					view_not_wraped(fp, x);

				fclose(fp);
			}
			break;
	}
}

char *
get_current_file_name(FileView *view)
{
	if(view->list_pos == -1)
		return "";
	return view->dir_entry[view->list_pos].name;
}

void
free_selected_file_array(FileView *view)
{
	if(view->selected_filelist)
	{
		int x;
		for(x = 0; x < view->selected_files; x++)
			free(view->selected_filelist[x]);
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

	if(view->selected_filelist != NULL)
	{
		free_selected_file_array(view);
		 /* restoring this because free_selected_file_array sets it to zero */
		view->selected_files = y;
	}
	view->selected_filelist =
		(char **)calloc(view->selected_files, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < view->list_rows; x++)
	{
		if(!view->dir_entry[x].selected)
			continue;
		if(strcmp(view->dir_entry[x].name, "../") == 0)
		{
			view->dir_entry[x].selected = 0;
			continue;
		}

		view->selected_filelist[y] = strdup(view->dir_entry[x].name);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		y++;
	}
	view->selected_files = y;
}

/* If you use this function using the free_selected_file_array()
 * will clean up the allocated memory
 */
void
get_selected_files(FileView *view, int count, const int *indexes)
{
	int x;
	int y = 0;

	if(view->selected_filelist != NULL)
		free_selected_file_array(view);
	view->selected_filelist = (char **)calloc(count, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	for(x = 0; x < count; x++)
	{
		if(strcmp(view->dir_entry[indexes[x]].name, "../") == 0)
			continue;

		view->selected_filelist[y] = strdup(view->dir_entry[indexes[x]].name);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		y++;
	}

	view->selected_files = y;
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
	const char *buf;
	size_t len;

	if(curr_stats.vifm_started != 2)
		return;

	werase(view->title);

	buf = replace_home_part(view->curr_dir);

	len = get_utf8_string_length(buf);
	if(len + 1 > view->window_width && curr_view == view)
	{ /* Truncate long directory names */
		const char *ptr;

		ptr = buf;
		while(len > view->window_width - 2)
		{
			len--;
			ptr += get_char_width(ptr);
		}

		wprintw(view->title, "...%s", ptr);
	}
	else
		wprintw(view->title, "%s", buf);

	wnoutrefresh(view->title);
}

void
draw_dir_list(FileView *view, int top)
{
	int x;
	int y = 0;
	char file_name[view->window_width*2 -2];
	int LINE_COLOR;
	int bold = 1;
	int color_scheme;

	if(curr_stats.vifm_started != 2)
		return;

	color_scheme = check_directory_for_color_scheme(view->curr_dir);

	/* TODO do we need this code?
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
	{
		top = 0;
	}
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

void
erase_current_line_bar(FileView *view)
{
	int old_cursor = view->curr_line;
	int old_pos = view->top_line + old_cursor;
	char file_name[view->window_width*2 -2];
	int bold = 1;
	int LINE_COLOR;
	size_t print_width;

	if(curr_stats.vifm_started != 2)
		return;

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

/* Returns non-zero if redraw is needed */
static int
move_curr_line(FileView *view, int pos)
{
	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(pos == -1)
		return 0;

	if(view->curr_line > view->list_rows - 1)
		view->curr_line = view->list_rows - 1;

	if(view->top_line <= pos && pos <= view->top_line + view->window_rows)
	{
		view->curr_line = pos - view->top_line;
	}
	else if(pos > view->top_line + view->window_rows)
	{
		while(pos > view->top_line + view->window_rows)
			view->top_line++;

		view->curr_line = view->window_rows;
		return 1;
	}
	else if(pos < view->top_line)
	{
		while(pos < view->top_line)
			view->top_line--;

		view->curr_line = 0;
		return 1;
	}

	return 0;
}

void
moveto_list_pos(FileView *view, int pos)
{
	int redraw = 0;
	int old_cursor = view->curr_line;
	char file_name[view->window_width*2 + 1];
	size_t print_width;
	int LINE_COLOR;
	short f, b;

	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(pos == -1)
		return;

	if(view->curr_line > view->list_rows - 1)
		view->curr_line = view->list_rows - 1;

	erase_current_line_bar(view);

	redraw = move_curr_line(view, pos);

	view->list_pos = pos;

	if(curr_stats.vifm_started != 2)
		return;

	if(redraw)
		draw_dir_list(view, view->top_line);

	if(cfg.invert_cur_line)
	{
		switch(view->dir_entry[pos].type)
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
				break;
		}

		if(view->dir_entry[pos].selected)
			LINE_COLOR = SELECTED_COLOR + view->color_scheme;

		pair_content(LINE_COLOR, &f, &b);
		init_pair(CURRENT_COLOR + view->color_scheme, f, b);
	}

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme));

	mvwaddstr(view->win, old_cursor, 0, " ");

	if(cfg.invert_cur_line)
		wattron(view->win, COLOR_PAIR(CURRENT_COLOR + view->color_scheme) | A_BOLD |
				A_REVERSE);
	else
		wattron(view->win, COLOR_PAIR(CURR_LINE_COLOR + view->color_scheme) |
				A_BOLD);

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

	if(cfg.invert_cur_line)
	{
		wattroff(view->win, COLOR_PAIR(CURRENT_COLOR + view->color_scheme) |
				A_BOLD | A_REVERSE);
	}
}

void
goto_history_pos(FileView *view, int pos)
{
	curr_stats.skip_history = 1;
	change_directory(view, view->history[pos].dir);
	curr_stats.skip_history = 0;

	load_dir_list(view, 1);
	moveto_list_pos(view, find_file_pos_in_list(view, view->history[pos].file));

	view->history_pos = pos;
}

void
save_view_history(FileView *view, const char *path, const char *file)
{
	int x;

	/* this could happen on FUSE error */
	if(view->list_rows <= 0)
		return;

	if(cfg.history_len <= 0)
		return;

	if(path == NULL)
		path = view->curr_dir;
	if(file == NULL)
		file = view->dir_entry[view->list_pos].name;

	if(view->history_num > 0
			&& strcmp(view->history[view->history_pos].dir, path) == 0)
	{
		if(curr_stats.vifm_started < 2)
			return;
		x = view->history_pos;
		snprintf(view->history[x].file, sizeof(view->history[x].file), "%s", file);
		return;
	}

	if(curr_stats.skip_history)
		return;

	if(view->history_num > 0 && view->history_pos != view->history_num - 1)
	{
		x = view->history_num - 1;
		while(x > view->history_pos)
			view->history[x--].dir[0] = '\0';
		view->history_num = view->history_pos + 1;
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
		view->history_num = x;
	}
	snprintf(view->history[x].dir, sizeof(view->history[x].dir), "%s", path);
	snprintf(view->history[x].file, sizeof(view->history[x].file), "%s", file);
	view->history_num++;
	view->history_pos = view->history_num - 1;
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
	else if(cfg.history_len > 0)
	{
		for(x = view->history_pos; x >= 0; x--)
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

static void
updir_from_mount(FileView *view, Fuse_List *runner)
{
	char *file;
	int pos;

	change_directory(view, runner->source_file_dir);

	load_dir_list(view, 0);

	file = runner->source_file_name;
	file += strlen(runner->source_file_dir) + 1;
	pos = find_file_pos_in_list(view, file);
	moveto_list_pos(view, pos);
}

static Fuse_List *
find_fuse_mount(const char *dir)
{
	Fuse_List *runner = fuse_mounts;
	while(runner)
	{
		if(strcmp(runner->mount_point, dir) == 0)
			break;
		runner = runner->next;
	}
	return runner;
}

/* Modifies path */
void
leave_invalid_dir(FileView *view, char *path)
{
	Fuse_List *runner;
	size_t len;
	char *p;

	if((runner = find_fuse_mount(path)) != NULL)
	{
		updir_from_mount(view, runner);
		return;
	}

	while(access(path, F_OK | R_OK) != 0)
	{
		if((runner = find_fuse_mount(path)) != NULL)
		{
			updir_from_mount(view, runner);
			break;
		}

		len = strlen(path);
		if(path[len - 1] == '/')
			path[len - 1] = '\0';

		p = strrchr(path, '/');
		if(p == NULL)
			break;
		p[1] = '\0';
	}
}

static int
in_mounted_dir(const char *path)
{
	Fuse_List *runner;

	runner = fuse_mounts;
	while(runner)
	{
		if(strcmp(runner->mount_point, path) == 0)
			return 1;

		runner = runner->next;
	}
	return 0;
}

/*
 * Return value:
 *   -1 error occurred.
 *   0  not mount point.
 *   1  left FUSE mount directory.
 */
static int
try_unmount_fuse(FileView *view)
{
	char buf[14 + PATH_MAX + 1];
	Fuse_List *runner, *trailer;
	int status;
	Fuse_List *sniffer;
	char *escaped_mount_point;

	runner = fuse_mounts;
	trailer = NULL;
	while(runner)
	{
		if(!strcmp(runner->mount_point, view->curr_dir))
			break;

		trailer = runner;
		runner = runner->next;
	}

	if(runner == NULL)
		return 0;

	/* we are exiting a top level dir */
	status_bar_message("FUSE unmounting selected file, please stand by..");
	escaped_mount_point = escape_filename(runner->mount_point, 0, 0);
	snprintf(buf, sizeof(buf), "fusermount -u %s 2> /dev/null",
			escaped_mount_point);
	free(escaped_mount_point);

	/* have to chdir to parent temporarily, so that this DIR can be unmounted */
	if(chdir(cfg.fuse_home) != 0)
	{
		show_error_msg("FUSE UMOUNT ERROR", "Can't chdir to FUSE home");
		return -1;
	}

	status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		char buf[PATH_MAX*2];
		werase(status_bar);
		snprintf(buf, sizeof(buf), "Can't unmount %s.  It may be busy.",
				runner->source_file_name);
		show_error_msg("FUSE UMOUNT ERROR", buf);
		(void)chdir(view->curr_dir);
		return -1;
	}

	/* remove the DIR we created for the mount */
	if(access(runner->mount_point, F_OK) == 0)
		rmdir(runner->mount_point);

	/* remove mount point from Fuse_List */
	sniffer = runner->next;
	if(trailer)
		trailer->next = sniffer ? sniffer : NULL;
	else
		fuse_mounts = sniffer;

	updir_from_mount(view, runner);
	free(runner);
	return 1;
}

/*
 * The directory can either be relative to the current
 * directory - ../
 * or an absolute path - /usr/local/share
 * The *directory passed to change_directory() cannot be modified.
 * Symlink directories require an absolute path
 *
 * Return value:
 *  -1  if there were errors.
 *   0  if directory successfully changed and we didn't leave FUSE mount
 *      directory.
 *   1  if directory successfully changed and we left FUSE mount directory.
 */
int
change_directory(FileView *view, const char *directory)
{
	DIR *dir;
	struct stat s;
	char newdir[PATH_MAX];
	char dir_dup[PATH_MAX];

	curr_stats.is_updir = 0;

	save_view_history(view, NULL, NULL);

	if(directory[0] == '/')
		canonicalize_path(directory, dir_dup, sizeof(dir_dup));
	else
	{
		snprintf(newdir, sizeof(newdir), "%s/%s", view->curr_dir, directory);
		canonicalize_path(newdir, dir_dup, sizeof(dir_dup));
	}

	if(strcmp(dir_dup, "/") != 0)
		chosp(dir_dup);

	snprintf(view->last_dir, sizeof(view->last_dir), "%s", view->curr_dir);

	/* check if we're exiting from a FUSE mounted top level dir.
	 * If so, unmount & let FUSE serialize */
	if(!strcmp(directory, "../") && in_mounted_dir(view->curr_dir))
	{
		int r = try_unmount_fuse(view);
		if(r != 0)
			return r;
	}

	/* Clean up any excess separators */
	if(strcmp(view->curr_dir, "/") != 0)
		chosp(view->curr_dir);
	if(strcmp(view->last_dir, "/") != 0)
		chosp(view->last_dir);

	if(access(dir_dup, F_OK) != 0)
	{
		char buf[12 + PATH_MAX + 1];

		LOG_SERROR_MSG(errno, "Can't access(, F_OK) \"%s\"", dir_dup);
		log_cwd();

		snprintf(buf, sizeof(buf), "Cannot open %s", dir_dup);
		show_error_msg("Directory Access Error", buf);

		leave_invalid_dir(view, dir_dup);
		change_directory(view, dir_dup);
		clean_selected_files(view);
		moveto_list_pos(view, view->list_pos);
		return -1;
	}

	if(access(dir_dup, R_OK) != 0)
	{
		char buf[31 + PATH_MAX + 1];

		LOG_SERROR_MSG(errno, "Can't access(, R_OK) \"%s\"", dir_dup);
		log_cwd();

		if(strcmp(view->curr_dir, dir_dup) != 0)
		{
			snprintf(buf, sizeof(buf), "You do not have read access on %s", dir_dup);
			show_error_msg("Directory Access Error", buf);
		}
	}

	if(access(dir_dup, X_OK) != 0)
	{
		char buf[32 + PATH_MAX + 1];

		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", dir_dup);
		log_cwd();

		snprintf(buf, sizeof(buf), "You do not have execute access on %s", dir_dup);
		show_error_msg("Directory Access Error", buf);

		clean_selected_files(view);
		leave_invalid_dir(view, view->curr_dir);
		return -1;
	}

	dir = opendir(dir_dup);

	if(dir == NULL)
	{
		LOG_SERROR_MSG(errno, "Can't opendir() \"%s\"", dir_dup);
		log_cwd();
	}

	if(chdir(dir_dup) == -1)
	{
		char buf[14 + PATH_MAX + 1];

		LOG_SERROR_MSG(errno, "Can't chdir() \"%s\"", dir_dup);
		log_cwd();

		closedir(dir);

		snprintf(buf, sizeof(buf), "Couldn't open %s", dir_dup);
		status_bar_message(buf);
		return -1;
	}

	clean_selected_files(view);

	if(strcmp(dir_dup, "/") != 0)
		chosp(dir_dup);

	/* Need to use setenv instead of getcwd for a symlink directory */
	setenv("PWD", dir_dup, 1);

	snprintf(view->curr_dir, PATH_MAX, "%s", dir_dup);
	draw_dir_list(view, view->top_line);

	if(dir != NULL)
		closedir(dir);

	/* Save the directory modified time to check for file changes */
	stat(view->curr_dir, &s);
	view->dir_mtime = s.st_mtime;

	save_view_history(view, NULL, NULL);
	return 0;
}

static void
reset_selected_files(FileView *view)
{
	int x;
	for(x = 0; x < view->selected_files; x++)
	{
		if(view->selected_filelist[x] != NULL)
		{
			int pos = find_file_pos_in_list(view, view->selected_filelist[x]);
			if(pos >= 0 && pos < view->list_rows)
				view->dir_entry[pos].selected = 1;
		}
	}

	x = view->selected_files;
	free_selected_file_array(view);
	view->selected_files = x;
}

static int
S_ISEXE(mode_t mode)
{
	return ((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode));
}

static int
regexp_filter_match(FileView *view, char *filename)
{
	regex_t re;

	if(view->filename_filter[0] == '\0')
		return view->invert;

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

static int
type_from_dir_entry(const struct dirent *d)
{
	switch(d->d_type)
	{
		case DT_BLK:
		case DT_CHR:
			return DEVICE;
		case DT_DIR:
			return DIRECTORY;
		case DT_LNK:
			return LINK;
		case DT_REG:
			return REGULAR;
		case DT_SOCK:
			return SOCKET;

		case DT_FIFO:
		case DT_UNKNOWN:
		default:
			return UNKNOWN;
	}
}

static void
load_parent_dir_only(FileView *view)
{
	dir_entry_t *dir_entry;
	struct stat s;

	view->list_rows = 1;
	view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
			sizeof(dir_entry_t));
	if(view->dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	dir_entry = view->dir_entry;

	/* Allocate extra for adding / to directories. */
	dir_entry->name = strdup("../");
	if(dir_entry->name == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	/* All files start as unselected and unmatched */
	dir_entry->selected = 0;
	dir_entry->search_match = 0;

	dir_entry->type = DIRECTORY;

	/* Load the inode info */
	if(lstat(dir_entry->name, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s/%s\"", view->curr_dir,
				dir_entry->name);
		log_cwd();

		dir_entry->size = 0;
		dir_entry->mode = 0;
		dir_entry->uid = -1;
		dir_entry->gid = -1;
		dir_entry->mtime = 0;
		dir_entry->atime = 0;
		dir_entry->ctime = 0;
	}
	else
	{
		dir_entry->size = (uintmax_t)s.st_size;
		dir_entry->mode = s.st_mode;
		dir_entry->uid = s.st_uid;
		dir_entry->gid = s.st_gid;
		dir_entry->mtime = s.st_mtime;
		dir_entry->atime = s.st_atime;
		dir_entry->ctime = s.st_ctime;
	}
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

	view->filtered = 0;

	if(stat(view->curr_dir, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		return;
	}
	view->dir_mtime = s.st_mtime;

	if(!reload && s.st_size > 2048)
		status_bar_message("Reading Directory...");

	update_all_windows();

	/* this is needed for lstat() below */
	if(chdir(view->curr_dir) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't chdir() into \"%s\"", view->curr_dir);
		return;
	}

	if(reload && view->selected_files)
		get_all_selected_files(view);

	if(view->dir_entry)
	{
		for(x = 0; x < old_list; x++)
			free(view->dir_entry[x].name);

		free(view->dir_entry);
		view->dir_entry = NULL;
	}
	view->dir_entry = (dir_entry_t *)malloc(sizeof(dir_entry_t));
	if(view->dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory.");
		return;
	}

	dir = opendir(view->curr_dir);

	if(dir != NULL)
	{
		for(view->list_rows = 0; (d = readdir(dir)); view->list_rows++)
		{
			dir_entry_t *dir_entry;

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
			else if(regexp_filter_match(view, d->d_name) == 0)
			{
				view->filtered++;
				view->list_rows--;
				continue;
			}
			else if(view->hide_dot && d->d_name[0] == '.')
			{
				view->filtered++;
				view->list_rows--;
				continue;
			}

			view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
					(view->list_rows + 1) * sizeof(dir_entry_t));
			if(view->dir_entry == NULL)
			{
				show_error_msg("Memory Error", "Unable to allocate enough memory");
				return;
			}

			dir_entry = view->dir_entry + view->list_rows;

			namelen = strlen(d->d_name);
			/* Allocate extra for adding / to directories. */
			dir_entry->name = malloc(namelen + 1 + 1);
			if(dir_entry->name == NULL)
			{
				show_error_msg("Memory Error", "Unable to allocate enough memory");
				return;
			}

			strcpy(dir_entry->name, d->d_name);

			/* All files start as unselected and unmatched */
			dir_entry->selected = 0;
			dir_entry->search_match = 0;

			/* Load the inode info */
			if(lstat(dir_entry->name, &s) != 0)
			{
				LOG_SERROR_MSG(errno, "Can't lstat() \"%s/%s\"", view->curr_dir,
						dir_entry->name);
				log_cwd();

				dir_entry->type = type_from_dir_entry(d);
				if(dir_entry->type == DIRECTORY)
					strcat(dir_entry->name, "/");
				dir_entry->size = 0;
				dir_entry->mode = 0;
				dir_entry->uid = -1;
				dir_entry->gid = -1;
				dir_entry->mtime = 0;
				dir_entry->atime = 0;
				dir_entry->ctime = 0;
				continue;
			}

			dir_entry->size = (uintmax_t)s.st_size;
			dir_entry->mode = s.st_mode;
			dir_entry->uid = s.st_uid;
			dir_entry->gid = s.st_gid;
			dir_entry->mtime = s.st_mtime;
			dir_entry->atime = s.st_atime;
			dir_entry->ctime = s.st_ctime;

			if(s.st_ino)
			{
				switch(s.st_mode & S_IFMT)
				{
					case S_IFLNK:
						if(check_link_is_dir(view->dir_entry[view->list_rows].name))
							strcat(dir_entry->name, "/");
						dir_entry->type = LINK;
						break;
					case S_IFDIR:
						strcat(dir_entry->name, "/");
						dir_entry->type = DIRECTORY;
						break;
					case S_IFCHR:
					case S_IFBLK:
						dir_entry->type = DEVICE;
						break;
					case S_IFSOCK:
						dir_entry->type = SOCKET;
						break;
					case S_IFREG:
						if(S_ISEXE(s.st_mode))
							dir_entry->type = EXECUTABLE;
						else
							dir_entry->type = REGULAR;
						break;
					default:
						dir_entry->type = UNKNOWN;
						break;
				}
			}
		}

		closedir(dir);
	}
	else
	{
		/* we don't have read access, only execute */
		load_parent_dir_only(view);
	}

	if(!reload && s.st_size > 2048)
	{
		status_bar_message("Sorting Directory...");
	}
	set_view_to_sort(view);
	qsort(view->dir_entry, view->list_rows, sizeof(dir_entry_t), sort_dir_list);

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].list_num = x;

	/* If reloading the same directory don't jump to
	 * history position.  Stay at the current line
	 */
	if(!reload)
		check_view_dir_history(view);

	/*
	 * It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directorys will always show at least the
	 * ../ file.  This resets the filter and reloads the directory.
	 */
	if(view->list_rows < 1)
	{
		char msg[64];
		snprintf(msg, sizeof(msg),
				"The %s pattern %s did not match any files. It was reset.",
				view->filename_filter, view->invert==1 ? "inverted" : "");
		show_error_msg("Filter error", msg);
		view->filename_filter = (char *)realloc(view->filename_filter, 1);
		if(view->filename_filter == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			return;
		}
		strcpy(view->filename_filter, "");
		view->invert = !view->invert;

		load_dir_list(view, 1);

		draw_dir_list(view, view->top_line);
		return;
	}
	if(reload && view->selected_files)
		reset_selected_files(view);
	else if(view->selected_files)
		view->selected_files = 0;

	draw_dir_list(view, view->top_line);
}

void
filter_selected_files(FileView *view)
{
	int x;

	if(!view->selected_files)
		view->dir_entry[view->list_pos].selected = 1;

	for(x = 0; x < view->list_rows; x++)
	{
		if(!view->dir_entry[x].selected)
			continue;

		if(strcmp(view->dir_entry[x].name, "../") == 0)
			continue;

		if(view->filename_filter == NULL || view->filename_filter[0] == '\0')
		{
			size_t buf_size;

			buf_size = 2 + strlen(view->dir_entry[x].name) + 3 + 1;
			view->filename_filter = (char *)realloc(view->filename_filter, buf_size);
		}
		else
		{
			size_t buf_size;

			buf_size = 3 + strlen(view->dir_entry[x].name) + 3 + 1;
			view->filename_filter = (char *)realloc(view->filename_filter,
					strlen(view->filename_filter) + buf_size);
			strcat(view->filename_filter, "|");
		}

		strcat(view->filename_filter, "\\<");
		strcat(view->filename_filter, view->dir_entry[x].name);
		chosp(view->filename_filter);
		strcat(view->filename_filter, "\\>$");
		view->filtered++;
	}
	view->invert = 1;
	clean_status_bar();
	load_dir_list(view, 1);
	moveto_list_pos(view, view->list_pos);
	view->selected_files = 0;
}

void
set_dot_files_visible(FileView *view, int visible)
{
	view->hide_dot = !visible;
	load_saving_pos(view, 1);
}

void
toggle_dot_files(FileView *view)
{
	set_dot_files_visible(view, view->hide_dot);
}

void
remove_filename_filter(FileView *view)
{
	view->prev_filter = (char *)realloc(view->prev_filter,
			strlen(view->filename_filter) + 1);
	snprintf(view->prev_filter, sizeof(view->prev_filter), "%s",
			view->filename_filter);
	view->filename_filter = (char *)realloc(view->filename_filter, 1);
	strcpy(view->filename_filter, "");
	view->prev_invert = view->invert;
	view->invert = 1;
	load_saving_pos(view, 0);
}

void
restore_filename_filter(FileView *view)
{
	view->filename_filter = (char *)realloc(view->filename_filter,
			strlen(view->prev_filter) + 1);
	snprintf(view->filename_filter, sizeof(view->filename_filter), "%s",
			view->prev_filter);
	view->invert = view->prev_invert;
	load_saving_pos(view, 0);
}

void
scroll_view(FileView *view)
{
	draw_dir_list(view, view->top_line);
	moveto_list_pos(view, view->list_pos);
}

static void
reload_window(FileView *view)
{
	curr_stats.skip_history = 1;

	change_directory(view, view->curr_dir);
	load_saving_pos(view, 1);

	if(view != curr_view)
	{
		mvwaddstr(view->win, view->curr_line, 0, "*");
		wrefresh(view->win);
	}

	curr_stats.skip_history = 0;
}

/*
 * This checks the modified times of the directories.
 */
void
check_if_filelists_have_changed(FileView *view)
{
	struct stat s;

	if(stat(view->curr_dir, &s) != 0)
	{
		char buf[12 + PATH_MAX + 1];

		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		log_cwd();

		snprintf(buf, sizeof(buf), "Cannot open %s", view->curr_dir);
		show_error_msg("Directory Access Error", buf);

		leave_invalid_dir(view, view->curr_dir);
		change_directory(view, view->curr_dir);
		clean_selected_files(view);
		s.st_mtime = 0; /* force window reload */
	}
	if(s.st_mtime != view->dir_mtime)
		reload_window(view);
}

void
load_saving_pos(FileView *view, int reload)
{
	char filename[NAME_MAX];
	int pos;

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	load_dir_list(view, reload);
	pos = find_file_pos_in_list(view, filename);
	pos = (pos >= 0) ? pos : view->list_pos;
	if(view == curr_view)
	{
		moveto_list_pos(view, pos);
	}
	else
	{
		mvwaddstr(view->win, view->curr_line, 0, " ");
		view->list_pos = pos;
		if(move_curr_line(view, pos))
			draw_dir_list(view, view->top_line);
		mvwaddstr(view->win, view->curr_line, 0, "*");
	}
}

void
change_sort_type(FileView *view, char type, char descending)
{
	union optval_t val;

	view->sort_type = type;
	view->sort_descending = descending;

	load_saving_pos(view, 1);

	val.enum_item = type;
	set_option("sort", val);

	val.enum_item = descending;
	set_option("sortorder", val);
}

int
pane_in_dir(FileView *view, const char *path)
{
	char pane_dir[PATH_MAX];
	char dir[PATH_MAX];

	if(realpath(view->curr_dir, pane_dir) != pane_dir)
		return 0;
	if(realpath(path, dir) != dir)
		return 0;
	return strcmp(pane_dir, dir) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
