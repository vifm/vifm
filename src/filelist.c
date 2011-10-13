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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#if(defined(BSD) && (BSD>=199103))
	#include <sys/types.h> /* required for regex.h on FreeBSD 4.2 */
#endif

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <lm.h>
#endif

#include <curses.h>

#include <regex.h>

#include <dirent.h> /* DIR */
#include <sys/stat.h> /* stat */
#include <sys/time.h> /* localtime */
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS */
#include <pwd.h>
#include <grp.h>
#endif

#include <errno.h>
#include <stdlib.h> /* malloc  qsort */
#include <string.h> /* strcat() */
#include <time.h>

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
#include "opt_handlers.h"
#include "sort.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "utils.h"

static int
get_line_color(FileView* view, int pos)
{
	switch(view->dir_entry[pos].type)
	{
		case DIRECTORY:
			return DIRECTORY_COLOR;
		case FIFO:
			return FIFO_COLOR;
		case LINK:
			if(is_on_slow_fs(view->curr_dir))
			{
				return LINK_COLOR;
			}
			else
			{
				char full[PATH_MAX];
				char linkto[PATH_MAX];
				snprintf(full, sizeof(full), "%s/%s", view->curr_dir,
						view->dir_entry[pos].name);
				if(get_link_target(full, linkto, sizeof(linkto)) != 0)
					return BROKEN_LINK_COLOR;

				if(is_path_absolute(linkto))
					strcpy(full, linkto);
				else
					snprintf(full, sizeof(full), "%s/%s", view->curr_dir, linkto);

				if(access(full, F_OK) == 0)
					return LINK_COLOR;
				else
					return BROKEN_LINK_COLOR;
			}
#ifndef _WIN32
		case SOCKET:
			return SOCKET_COLOR;
#endif
		case DEVICE:
			return DEVICE_COLOR;
		case EXECUTABLE:
			return EXECUTABLE_COLOR;
		default:
			return WIN_COLOR;
	}
}

static void
add_sort_type_info(FileView *view, int y, int x, int is_current_line)
{
	char buf[24];
#ifndef _WIN32
	struct passwd *pwd_buf;
	struct group *grp_buf;
#endif
	struct tm *tm_ptr;
	Col_attr col;
	int type;

	switch(abs(view->sort[0]))
	{
		case SORT_BY_OWNER_NAME:
#ifndef _WIN32
			if((pwd_buf = getpwuid(view->dir_entry[x].uid)) != NULL)
			{
				snprintf(buf, sizeof(buf), " %s", pwd_buf->pw_name);
				break;
			}
			/* break skipped */
#else
			snprintf(buf, sizeof(buf), " UNKNOWN");
			break;
#endif
		case SORT_BY_OWNER_ID:
#ifndef _WIN32
			snprintf(buf, sizeof(buf), " %d", (int) view->dir_entry[x].uid);
#else
			snprintf(buf, sizeof(buf), " UNKNOWN");
#endif
			break;
		case SORT_BY_GROUP_NAME:
#ifndef _WIN32
			if((grp_buf = getgrgid(view->dir_entry[x].gid)) != NULL)
			{
				snprintf(buf, sizeof(buf), " %s", grp_buf->gr_name);
				break;
			}
			/* break skipped */
#else
			snprintf(buf, sizeof(buf), " UNKNOWN");
			break;
#endif
		case SORT_BY_GROUP_ID:
#ifndef _WIN32
			snprintf(buf, sizeof(buf), " %d", (int) view->dir_entry[x].gid);
#else
			snprintf(buf, sizeof(buf), " UNKNOWN");
#endif
			break;
		case SORT_BY_MODE:
			snprintf(buf, sizeof(buf), " %s", get_mode_str(view->dir_entry[x].mode));
			break;
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

				if(view->dir_entry[x].type == DIRECTORY)
				{
					char buf[PATH_MAX];
					snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
							view->dir_entry[x].name);
					tree_get_data(curr_stats.dirsize_cache, buf, &size);
				}

				if(size == 0)
					size = view->dir_entry[x].size;

				friendly_size_notation(size, sizeof(str), str);
				snprintf(buf, sizeof(buf), " %s", str);
			}
			break;
	}

	type = WIN_COLOR;
	col = view->cs.color[WIN_COLOR];

	if(view->dir_entry[x].selected)
	{
		mix_colors(&col, &view->cs.color[SELECTED_COLOR]);
		type = SELECTED_COLOR;
	}

	if(is_current_line)
	{
		mix_colors(&col, &view->cs.color[CURR_LINE_COLOR]);
		type = CURRENT_COLOR;
	}
	else
	{
		init_pair(view->color_scheme + type, col.fg, col.bg);
	}

	wattron(view->win, COLOR_PAIR(type + view->color_scheme) | col.attr);

	mvwaddstr(view->win, y, view->window_width - strlen(buf), buf);

	wattroff(view->win, COLOR_PAIR(type + view->color_scheme) | col.attr);
}

#ifndef _WIN32
static FILE *
use_info_prog(char *cmd)
{
	pid_t pid;
	int error_pipe[2];
	int use_menu = 0, split = 0;

	if(strchr(cmd, '%') == NULL)
	{
		char *escaped = escape_filename(
				curr_view->dir_entry[curr_view->list_pos].name, 0);
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
		(void)show_error_msg("File pipe error", "Error creating pipe");
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
#endif

static void
view_not_wraped(FILE *fp, int x)
{
	char line[1024];
	int y = 1;

	while(fgets(line, other_view->window_width - 2, fp) == line &&
			x <= other_view->window_rows - 2)
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
			wprint(other_view->win, strchar2str(line + i));
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
				&& x <= other_view->window_rows - 2)
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
			wprint(other_view->win, strchar2str(line + i));
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
	char buf[PATH_MAX];

	wclear(other_view->win);
	wclear(other_view->title);
	mvwaddstr(other_view->title, 0, 0, "File: ");
	wprint(other_view->title, view->dir_entry[view->list_pos].name);

	strcpy(buf, view->dir_entry[view->list_pos].name);
	switch(view->dir_entry[view->list_pos].type)
	{
		case DEVICE:
			mvwaddstr(other_view->win, ++x, y, "File is a Device");
			break;
#ifndef _WIN32
		case SOCKET:
			mvwaddstr(other_view->win, ++x, y, "File is a Socket");
			break;
#endif
		case FIFO:
			mvwaddstr(other_view->win, ++x, y, "File is a Named Pipe");
			break;
		case LINK:
			if(get_link_target(view->dir_entry[view->list_pos].name, buf,
					sizeof(buf)) != 0)
			{
				mvwaddstr(other_view->win, ++x, y, "Cannot resolve Link");
				break;
			}
			/* break intensionally omitted */
		case UNKNOWN:
		default:
			{
				char *viewer;

				viewer = get_viewer_for_file(buf);
				if(viewer == NULL && is_dir(buf))
				{
					mvwaddstr(other_view->win, ++x, y, "File is a Directory");
					break;
				}
#ifndef _WIN32
				if(viewer != NULL && viewer[0] != '\0')
					fp = use_info_prog(viewer);
				else
#endif
					fp = fopen(buf, "r");

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
	wrefresh(other_view->win);
	wrefresh(other_view->title);
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
	if(view->selected_filelist != NULL)
	{
		free_string_array(view->selected_filelist, view->selected_files);
		view->selected_filelist = NULL;
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

	count_selected(view);

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
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
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
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
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
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
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
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		y++;
	}

	view->selected_files = y;
}

void
count_selected(FileView *view)
{
	int i;
	view->selected_files = 0;
	for(i = 0; i < view->list_rows; i++)
		view->selected_files += (view->dir_entry[i].selected != 0);
}

int
find_file_pos_in_list(FileView *view, const char *file)
{
	int x;
	for(x = 0; x < view->list_rows; x++)
	{
#ifndef _WIN32
		if(strcmp(view->dir_entry[x].name, file) == 0)
#else
		if(strcasecmp(view->dir_entry[x].name, file) == 0)
#endif
			return x;
	}
	return -1;
}

void
update_view_title(FileView *view)
{
	char *buf;
	size_t len;

	if(view == curr_view)
	{
		Col_attr col;

		col = cfg.cs.color[TOP_LINE_COLOR];
		mix_colors(&col, &cfg.cs.color[TOP_LINE_SEL_COLOR]);
		init_pair(DCOLOR_BASE + TOP_LINE_SEL_COLOR, col.fg, col.bg);

		wbkgdset(view->title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_SEL_COLOR) |
				(col.attr & A_REVERSE));
		wattrset(view->title, col.attr & ~A_REVERSE);

		set_term_title(view->curr_dir);
	}
	else
	{
		wbkgdset(view->title, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
				(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
		wattrset(view->title, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
		wbkgdset(top_line, COLOR_PAIR(DCOLOR_BASE + TOP_LINE_COLOR) |
				(cfg.cs.color[TOP_LINE_COLOR].attr & A_REVERSE));
		wattrset(top_line, cfg.cs.color[TOP_LINE_COLOR].attr & ~A_REVERSE);
		werase(top_line);
	}
	werase(view->title);

	if(curr_stats.vifm_started < 2)
		return;

	buf = replace_home_part(view->curr_dir);

	len = get_utf8_string_length(buf);
	if(len + 1 > view->window_width && view == curr_view)
	{ /* Truncate long directory names */
		const char *ptr;

		ptr = buf;
		while(len > view->window_width - 2)
		{
			len--;
			ptr += get_char_width(ptr);
		}

		wprintw(view->title, "...");
		wprint(view->title, ptr);
	}
	else if(len + 1 > view->window_width && curr_view != view)
	{
		size_t len = get_normal_utf8_string_widthn(buf, view->window_width - 3 + 1);
		buf[len] = '\0';
		wprint(view->title, buf);
	}
	else
	{
		wprint(view->title, buf);
	}

	wnoutrefresh(view->title);
}

void
draw_dir_list(FileView *view, int top)
{
	int attr;
	int x;
	int y = 0;

	if(curr_stats.vifm_started < 2)
		return;

	if(top + view->window_rows > view->list_rows)
		top = view->list_rows - view->window_rows;
	if(top < 0)
		top = 0;

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

	if(view->color_scheme == DCOLOR_BASE)
		attr = cfg.cs.color[WIN_COLOR].attr;
	else
		attr = view->cs.color[WIN_COLOR].attr;
	wbkgdset(view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme) | attr);
	werase(view->win);

	for(x = top; x < view->list_rows; x++)
	{
		size_t print_width;
		int LINE_COLOR;
		char file_name[view->window_width*2 - 2];
		Col_attr col;
		/* Extra long file names are truncated to fit */

		print_width = get_real_string_width(view->dir_entry[x].name,
				view->window_width - 2) + 2;
		snprintf(file_name, print_width, "%s", view->dir_entry[x].name);

		wmove(view->win, y, 1);

		LINE_COLOR = get_line_color(view, x);
		col = view->cs.color[WIN_COLOR];
		mix_colors(&col, &view->cs.color[LINE_COLOR]);

		if(view->dir_entry[x].selected)
		{
			mix_colors(&col, &view->cs.color[SELECTED_COLOR]);
			LINE_COLOR = SELECTED_COLOR;
		}

		init_pair(view->color_scheme + LINE_COLOR, col.fg, col.bg);

		wattrset(view->win, COLOR_PAIR(LINE_COLOR + view->color_scheme) | col.attr);
		wprint(view->win, file_name);
		wattroff(view->win, COLOR_PAIR(LINE_COLOR + view->color_scheme) | col.attr);

		add_sort_type_info(view, y, x, 0);

		y++;
		if(y > view->window_rows)
			break;
	}

	if(view != curr_view)
		mvwaddstr(view->win, view->curr_line, 0, "*");

	view->top_line = top;

	if(view == curr_view && cfg.scroll_bind)
	{
		FileView *other = (view == &lwin) ? &rwin : &lwin;
		if(view == &lwin)
			other->top_line = view->top_line + curr_stats.scroll_bind_off;
		else
			other->top_line = view->top_line - curr_stats.scroll_bind_off;

		if(other->top_line + other->window_rows >= other->list_rows)
			other->top_line = other->list_rows - other->window_rows;
		if(other->top_line < 0)
			other->top_line = 0;

		if(other->top_line > 0)
			(void)correct_list_pos_on_scroll_down(other, 0);
		if(other->top_line < other->list_rows - other->window_rows - 1)
			(void)correct_list_pos_on_scroll_up(other, 0);
  	other->curr_line = other->list_pos - other->top_line;

		draw_dir_list(other, other->top_line);
		wrefresh(other->win);
	}
}

/* returns non-zero if doing something makes sense */
int
correct_list_pos_on_scroll_down(FileView *view, int pos_delta)
{
	int off;
	if(view->list_rows <= view->window_rows + 1)
		return 0;
	if(view->top_line == view->list_rows - view->window_rows - 1)
		return 0;

	off = MAX(cfg.scroll_off, 0);
	if(view->list_pos <= view->top_line + off)
		view->list_pos = view->top_line + pos_delta + off;
	return 1;
}

/* returns non-zero if doing something makes sense */
int
correct_list_pos_on_scroll_up(FileView *view, int pos_delta)
{
	int off;
	if(view->list_rows <= view->window_rows + 1 || view->top_line == 0)
		return 0;

	off = MAX(cfg.scroll_off, 0);
	if(view->list_pos >= view->top_line + view->window_rows - off)
		view->list_pos = view->top_line + pos_delta + view->window_rows - off;
	return 1;
}

void
erase_current_line_bar(FileView *view)
{
	int old_cursor = view->curr_line;
	int old_pos = view->top_line + old_cursor;
	char file_name[view->window_width*2 - 2];
	int LINE_COLOR;
	size_t print_width;
	Col_attr col;

	if(curr_stats.vifm_started < 2)
		return;

	/* Extra long file names are truncated to fit */

	if(old_pos >= 0 && old_pos < view->list_rows)
	{
		print_width = get_real_string_width(view->dir_entry[old_pos].name,
				view->window_width - 2) + 2;
		snprintf(file_name, print_width, "%s", view->dir_entry[old_pos].name);
	}
	else /* The entire list is going to be redrawn so just return. */
		return;

	wmove(view->win, old_cursor, 1);

	wclrtoeol(view->win);

	LINE_COLOR = get_line_color(view, old_pos);
	col = view->cs.color[WIN_COLOR];
	mix_colors(&col, &view->cs.color[LINE_COLOR]);

	if(view->dir_entry[old_pos].selected)
	{
		mix_colors(&col, &view->cs.color[SELECTED_COLOR]);
		LINE_COLOR = SELECTED_COLOR;
	}

	init_pair(view->color_scheme + LINE_COLOR, col.fg, col.bg);

	wattrset(view->win, COLOR_PAIR(LINE_COLOR + view->color_scheme) | col.attr);
	file_name[print_width] = '\0';
	wprint(view->win, file_name);
	wattroff(view->win, COLOR_PAIR(LINE_COLOR + view->color_scheme) | col.attr);

	add_sort_type_info(view, old_cursor, old_pos, 0);
}

/* Returns non-zero if redraw is needed */
static int
move_curr_line(FileView *view, int pos)
{
	int redraw = 0;

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
		redraw = 1;
	}
	else if(pos < view->top_line)
	{
		while(pos < view->top_line)
			view->top_line--;

		view->curr_line = 0;
		redraw = 1;
	}

	if(cfg.scroll_off > 0)
	{
		int s = MIN((view->window_rows + 1)/2, cfg.scroll_off);
		if(pos - view->top_line < s && view->top_line > 0)
		{
			view->top_line -= s - (pos - view->top_line);
			if(view->top_line < 0)
				view->top_line = 0;
			view->curr_line = s;
			redraw = 1;
		}
		if((view->top_line + view->window_rows) - pos < s &&
				pos + s < view->list_rows)
		{
			view->top_line += s - ((view->top_line + view->window_rows) - pos);
			view->curr_line = view->window_rows - s;
			redraw = 1;
		}
	}

	return redraw;
}

void
move_to_list_pos(FileView *view, int pos)
{
	int redraw = 0;
	int old_cursor = view->curr_line;
	char file_name[view->window_width*2 + 1];
	size_t print_width;
	int LINE_COLOR;
	Col_attr col;

	if(pos < 1)
		pos = 0;

	if(pos > view->list_rows - 1)
		pos = view->list_rows - 1;

	if(pos == -1)
		return;

	if(view->curr_line > view->list_rows - 1)
		view->curr_line = view->list_rows - 1;

	view->list_pos = pos;

	erase_current_line_bar(view);

	redraw = move_curr_line(view, pos);

	if(curr_stats.vifm_started < 2)
		return;

	if(redraw)
		draw_dir_list(view, view->top_line);

	wattrset(view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme));
	mvwaddstr(view->win, old_cursor, 0, " ");
	wrefresh(view->win);
	wattroff(view->win, COLOR_PAIR(WIN_COLOR + view->color_scheme));

	LINE_COLOR = get_line_color(view, pos);
	col = view->cs.color[WIN_COLOR];
	mix_colors(&col, &view->cs.color[LINE_COLOR]);

	if(view->dir_entry[pos].selected)
		mix_colors(&col, &view->cs.color[SELECTED_COLOR]);

	mix_colors(&col, &view->cs.color[CURR_LINE_COLOR]);

	init_pair(view->color_scheme + CURRENT_COLOR, col.fg, col.bg);
	wattron(view->win,
			COLOR_PAIR(CURRENT_COLOR + view->color_scheme) | col.attr);

	/* Blank the current line and
	 * print out the current line bar
	 */

	memset(file_name, ' ', view->window_width);

	file_name[view->window_width] = '\0';

	mvwaddstr(view->win, view->curr_line, 1, file_name);

	print_width = get_real_string_width(view->dir_entry[pos].name,
			view->window_width - 2) + 2;
	snprintf(file_name, print_width, " %s", view->dir_entry[pos].name);

	wmove(view->win, view->curr_line, 0);
	wprint(view->win, file_name);

	wattroff(view->win,
			COLOR_PAIR(CURRENT_COLOR + view->color_scheme) | col.attr);

	add_sort_type_info(view, view->curr_line, pos, 1);

	if(curr_stats.view)
		quick_view_file(view);

	wrefresh(view->win);
	update_stat_window(view);
}

void
goto_history_pos(FileView *view, int pos)
{
	curr_stats.skip_history = 1;
	if(change_directory(view, view->history[pos].dir) < 0)
	{
		curr_stats.skip_history = 0;
		return;
	}
	curr_stats.skip_history = 0;

	load_dir_list(view, 1);
	move_to_list_pos(view, find_file_pos_in_list(view, view->history[pos].file));

	view->history_pos = pos;
}

void
clean_positions_in_history(FileView *view)
{
	int i;
	for(i = 0; i <= view->history_pos; i++)
		view->history[i].file[0] = '\0';
}

void
save_view_history(FileView *view, const char *path, const char *file, int pos)
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
	if(pos < 0)
		pos = view->list_pos;

	if(view->history_num > 0 &&
			strcmp(view->history[view->history_pos].dir, path) == 0)
	{
		if(curr_stats.vifm_started < 2)
			return;
		x = view->history_pos;
		snprintf(view->history[x].file, sizeof(view->history[x].file), "%s", file);
		view->history[x].rel_pos = pos - view->top_line;
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
			view->history[y].rel_pos = view->history[y + 1].rel_pos;
		}
		x--;
		view->history_num = x;
	}
	snprintf(view->history[x].dir, sizeof(view->history[x].dir), "%s", path);
	snprintf(view->history[x].file, sizeof(view->history[x].file), "%s", file);
	view->history[x].rel_pos = pos - view->top_line;
	view->history_num++;
	view->history_pos = view->history_num - 1;
}

int
is_in_view_history(FileView *view, const char *path)
{
	int i;
	if(view->history == NULL)
		return 0;
	for(i = view->history_pos; i >= 0; i--)
	{
		if(strlen(view->history[i].dir) < 1)
			break;
		if(strcmp(view->history[i].dir, path) == 0)
			return 1;
	}
	return 0;
}

static void
check_view_dir_history(FileView *view)
{
	int pos = 0;
	int rel_pos = -1;

	if(cfg.history_len > 0 && curr_stats.ch_pos)
	{
		int x;
		int found = 0;
		x = view->history_pos;
		if(strcmp(view->history[x].dir, view->curr_dir) == 0 &&
				strcmp(view->history[x].file, "") == 0)
			x--;
		for(; x >= 0; x--)
		{
			if(strlen(view->history[x].dir) < 1)
				break;
			if(strcmp(view->history[x].dir, view->curr_dir) == 0)
			{
				found = 1;
				break;
			}
		}
		if(found)
		{
			pos = find_file_pos_in_list(view, view->history[x].file);
			rel_pos = view->history[x].rel_pos;
		}
		else if(path_starts_with(view->last_dir, view->curr_dir) &&
				strcmp(view->last_dir, view->curr_dir) != 0 &&
				strchr(view->last_dir + strlen(view->curr_dir) + 1, '/') == NULL)
		{
			char buf[NAME_MAX];
			strcpy(buf, view->last_dir + strlen(view->curr_dir));
			strcat(buf, "/");

			pos = find_file_pos_in_list(view, buf);
			rel_pos = -1;
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
	if(rel_pos >= 0)
	{
		view->top_line = pos - rel_pos;
		if(view->top_line < 0)
			view->top_line = 0;
		view->curr_line = pos - view->top_line;
	}
	else
	{
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
	}
}

void
clean_selected_files(FileView *view)
{
	int x;

	if(view->selected_files != 0)
	{
		char **tmp;

		free_string_array(view->saved_selection, view->nsaved_selection);

		tmp = view->selected_filelist;
		view->selected_filelist = NULL;

		get_all_selected_files(view);
		view->nsaved_selection = view->selected_files;
		view->saved_selection = view->selected_filelist;

		view->selected_filelist = tmp;
	}

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;
	view->selected_files = 0;
}

static void
clean_selection(FileView *view)
{
	int x;

	free_string_array(view->saved_selection, view->nsaved_selection);
	view->nsaved_selection = 0;
	view->saved_selection = NULL;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;
	view->selected_files = 0;
}

static void
updir_from_mount(FileView *view, Fuse_List *runner)
{
	char *file;
	int pos;

	if(change_directory(view, runner->source_file_dir) < 0)
		return;

	load_dir_list(view, 0);

	file = runner->source_file_name;
	file += strlen(runner->source_file_dir) + 1;
	pos = find_file_pos_in_list(view, file);
	move_to_list_pos(view, pos);
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

	if(strchr(path, '/') == NULL)
#ifndef _WIN32
		strcpy(path, "/");
#else
		strcpy(path, "c:/");
#endif
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
	escaped_mount_point = escape_filename(runner->mount_point, 0);
	snprintf(buf, sizeof(buf), "fusermount -u %s 2> /dev/null",
			escaped_mount_point);
	free(escaped_mount_point);

	/* have to chdir to parent temporarily, so that this DIR can be unmounted */
	if(my_chdir(cfg.fuse_home) != 0)
	{
		(void)show_error_msg("FUSE UMOUNT ERROR", "Can't chdir to FUSE home");
		return -1;
	}

	status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		werase(status_bar);
		(void)show_error_msgf("FUSE UMOUNT ERROR",
				"Can't unmount %s.  It may be busy.", runner->source_file_name);
		(void)my_chdir(view->curr_dir);
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

#ifdef _WIN32
static int
get_dir_mtime(const char *path, FILETIME *ft)
{
	char buf[PATH_MAX];
	HANDLE hfile;

	snprintf(buf, sizeof(buf), "%s/.", path);

	hfile = CreateFileA(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return -1;

	if(!GetFileTime(hfile, NULL, NULL, ft))
	{
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);

	return 0;
}
#endif

static int
update_dir_mtime(FileView *view)
{
#ifndef _WIN32
	struct stat s;

	if(stat(view->curr_dir, &s) != 0)
		return -1;
	view->dir_mtime = s.st_mtime;
	return 0;
#else
	return get_dir_mtime(view->curr_dir, &view->dir_mtime);
#endif
}

#ifdef _WIN32
static const char *
handle_mount_points(const char *path)
{
	static char buf[PATH_MAX];

	DWORD attr;
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	REPARSE_DATA_BUFFER *rdbp;

	attr = GetFileAttributes(path);
	if(attr == INVALID_FILE_ATTRIBUTES)
		return path;

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return path;

	snprintf(buf, sizeof(buf), "%s", path);
	chosp(buf);
	hfind = FindFirstFileA(buf, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
		return path;

	FindClose(hfind);

	if(ffd.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT)
		return path;

	hfile = CreateFileA(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return path;

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb,
				sizeof(rdb), &attr, NULL))
	{
		CloseHandle(hfile);
		return path;
	}
	CloseHandle(hfile);
	
	rdbp = (REPARSE_DATA_BUFFER *)rdb;
	t = to_multibyte(rdbp->MountPointReparseBuffer.PathBuffer);
	if(strncmp(t, "\\??\\", 4) == 0)
		strcpy(buf, t + 4);
	else
		strcpy(buf, t);
	free(t);
	return buf;
}
#endif

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
	char newdir[PATH_MAX];
	char dir_dup[PATH_MAX];
#ifdef _WIN32
	int i;
#endif

	save_view_history(view, NULL, NULL, -1);

#ifdef _WIN32
	directory = handle_mount_points(directory);
#endif

	if(is_path_absolute(directory))
	{
		canonicalize_path(directory, dir_dup, sizeof(dir_dup));
	}
	else
	{
#ifdef _WIN32
		if(directory[0] == '/')
			snprintf(newdir, sizeof(newdir), "%c:%s", view->curr_dir[0], directory);
		else
#endif
			snprintf(newdir, sizeof(newdir), "%s/%s", view->curr_dir, directory);
		canonicalize_path(newdir, dir_dup, sizeof(dir_dup));
	}

#ifdef _WIN32
	for(i = 0; dir_dup[i] != '\0'; i++)
	{
		if(dir_dup[i] == '\\')
			dir_dup[i] = '/';
	}
#endif

	if(!is_root_dir(dir_dup))
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
	if(!is_root_dir(view->curr_dir))
		chosp(view->curr_dir);
	if(!is_root_dir(view->last_dir))
		chosp(view->last_dir);

#ifndef _WIN32
	if(access(dir_dup, F_OK) != 0)
#else
	if(!is_dir(dir_dup) != 0 && !is_unc_root(dir_dup))
#endif
	{
		LOG_SERROR_MSG(errno, "Can't access(, F_OK) \"%s\"", dir_dup);
		log_cwd();

		(void)show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);

		clean_selected_files(view);
		return -1;
	}

	if(access(dir_dup, X_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", dir_dup);
		log_cwd();

		(void)show_error_msgf("Directory Access Error",
				"You do not have execute access on %s", dir_dup);

		clean_selected_files(view);
		return -1;
	}

	if(access(dir_dup, R_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, R_OK) \"%s\"", dir_dup);
		log_cwd();

		if(strcmp(view->curr_dir, dir_dup) != 0)
		{
			(void)show_error_msgf("Directory Access Error",
					"You do not have read access on %s", dir_dup);
		}
	}

	if(my_chdir(dir_dup) == -1 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() \"%s\"", dir_dup);
		log_cwd();

		(void)show_error_msgf("Change Directory Error", "Couldn't open %s",
				dir_dup);
		return -1;
	}

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	if(strcmp(dir_dup, view->curr_dir) != 0)
		clean_selection(view);
	else
		clean_selected_files(view);

#ifndef _WIN32
	/* Need to use setenv instead of getcwd for a symlink directory */
	setenv("PWD", dir_dup, 1);
#endif

	snprintf(view->curr_dir, PATH_MAX, "%s", dir_dup);

	/* Save the directory modified time to check for file changes */
	update_dir_mtime(view);

	save_view_history(view, NULL, "", -1);
	return 0;
}

static void
reset_selected_files(FileView *view, int need_free)
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

	if(need_free)
	{
		x = view->selected_files;
		free_selected_file_array(view);
		view->selected_files = x;
	}
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

#ifndef _WIN32
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
			return FIFO;

		case DT_UNKNOWN:
		default:
			return UNKNOWN;
	}
}
#endif

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
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	dir_entry = view->dir_entry;

	/* Allocate extra for adding / to directories. */
	dir_entry->name = strdup("../");
	if(dir_entry->name == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
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
#ifndef _WIN32
		dir_entry->uid = -1;
		dir_entry->gid = -1;
#endif
		dir_entry->mtime = 0;
		dir_entry->atime = 0;
		dir_entry->ctime = 0;
	}
	else
	{
		dir_entry->size = (uintmax_t)s.st_size;
		dir_entry->mode = s.st_mode;
#ifndef _WIN32
		dir_entry->uid = s.st_uid;
		dir_entry->gid = s.st_gid;
#endif
		dir_entry->mtime = s.st_mtime;
		dir_entry->atime = s.st_atime;
		dir_entry->ctime = s.st_ctime;
	}
}

static int
is_executable(dir_entry_t *d)
{
#ifndef _WIN32
	return S_ISEXE(d->mode);
#else
	return is_win_executable(d->name);
#endif
}

#ifdef _WIN32
static void
fill_with_shared(FileView *view)
{
	NET_API_STATUS res;
	wchar_t *wserver;
	
	wserver = to_wide(view->curr_dir + 2);
	if(wserver == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	view->list_rows = 0;
	do
	{
		PSHARE_INFO_0 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;

		res = NetShareEnum(wserver, 0, (LPBYTE *)&buf_ptr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			PSHARE_INFO_0 p;
			DWORD i;

			p = buf_ptr;
			for(i = 1; i <= er; i++)
			{
				char buf[512];
				WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)p->shi0_netname, -1, buf,
						sizeof(buf), NULL, NULL);
				if(!ends_with(buf, "$"))
				{
					dir_entry_t *dir_entry;

					view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
							(view->list_rows + 1)*sizeof(dir_entry_t));
					if(view->dir_entry == NULL)
					{
						(void)show_error_msg("Memory Error",
								"Unable to allocate enough memory");
						p++;
						continue;
					}

					dir_entry = view->dir_entry + view->list_rows;

					/* Allocate extra for adding / to directories. */
					dir_entry->name = malloc(strlen(buf) + 1 + 1);
					if(dir_entry->name == NULL)
					{
						(void)show_error_msg("Memory Error",
								"Unable to allocate enough memory");
						p++;
						continue;
					}

					strcpy(dir_entry->name, buf);

					/* All files start as unselected and unmatched */
					dir_entry->selected = 0;
					dir_entry->search_match = 0;

					dir_entry->size = 0;
					dir_entry->mode = 0777;
					dir_entry->mtime = 0;
					dir_entry->atime = 0;
					dir_entry->ctime = 0;

					strcat(dir_entry->name, "/");
					dir_entry->type = DIRECTORY;
					view->list_rows++;
				}
				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);

	free(wserver);
}
#endif

#ifdef _WIN32
static int
is_win_symlink(DWORD attr, DWORD tag)
{
	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return 0;

	return (tag == IO_REPARSE_TAG_SYMLINK);
}

static time_t
win_to_unix_time(FILETIME ft)
{
	const unsigned long long WINDOWS_TICK = 10000000;
	const unsigned long long SEC_TO_UNIX_EPOCH = 11644473600LL;
	unsigned long long win_time;

	win_time = ft.dwHighDateTime;
	win_time = (win_time << 32) | ft.dwLowDateTime;

	return win_time/WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}
#endif

static int
fill_dir_list(FileView *view)
{
	view->matches = 0;

#ifndef _WIN32
	DIR *dir;
	struct dirent *d;

	if((dir = opendir(view->curr_dir)) == NULL)
		return -1;

	for(view->list_rows = 0; (d = readdir(dir)); view->list_rows++)
	{
		dir_entry_t *dir_entry;
		size_t name_len;
		struct stat s;

		/* Ignore the "." directory. */
		if(strcmp(d->d_name, ".") == 0)
		{
			view->list_rows--;
			continue;
		}
		/* Always include the ../ directory unless it is the root directory. */
		if(strcmp(d->d_name, "..") == 0)
		{
			if(is_root_dir(view->curr_dir))
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
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			return -1;
		}

		dir_entry = view->dir_entry + view->list_rows;

		name_len = strlen(d->d_name);
		/* Allocate extra for adding / to directories. */
		dir_entry->name = malloc(name_len + 1 + 1);
		if(dir_entry->name == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			return -1;
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
					{
						struct stat st;
						if(check_link_is_dir(dir_entry->name))
							strcat(dir_entry->name, "/");
						if(stat(dir_entry->name, &st) == 0)
							dir_entry->mode = st.st_mode;
						dir_entry->type = LINK;
					}
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
					dir_entry->type = is_executable(dir_entry) ? EXECUTABLE : REGULAR;
					break;
				case S_IFIFO:
					dir_entry->type = FIFO;
					break;
				default:
					dir_entry->type = UNKNOWN;
					break;
			}
		}
	}

	closedir(dir);
	return 0;
#else
	char buf[PATH_MAX];
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;

	if(is_unc_root(view->curr_dir))
	{
		fill_with_shared(view);
		if(view->list_rows > 0)
			return 0;
		else
			return -1;
	}

	snprintf(buf, sizeof(buf), "%s/*", view->curr_dir);

	hfind = FindFirstFileA(buf, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
		return -1;

	view->list_rows = 0;
	do
	{
		dir_entry_t *dir_entry;
		size_t name_len;

		/* Ignore the "." directory. */
		if(strcmp(ffd.cFileName, ".") == 0)
			continue;
		/* Always include the ../ directory unless it is the root directory. */
		if(strcmp(ffd.cFileName, "..") == 0)
		{
			if(is_root_dir(view->curr_dir))
				continue;
		}
		else if(regexp_filter_match(view, ffd.cFileName) == 0)
		{
			view->filtered++;
			continue;
		}
		else if(view->hide_dot && ffd.cFileName[0] == '.')
		{
			view->filtered++;
			continue;
		}

		view->dir_entry = (dir_entry_t *)realloc(view->dir_entry,
				(view->list_rows + 1) * sizeof(dir_entry_t));
		if(view->dir_entry == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			FindClose(hfind);
			return -1;
		}

		dir_entry = view->dir_entry + view->list_rows;

		name_len = strlen(ffd.cFileName);
		/* Allocate extra for adding / to directories. */
		dir_entry->name = malloc(name_len + 1 + 1);
		if(dir_entry->name == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			FindClose(hfind);
			return -1;
		}

		strcpy(dir_entry->name, ffd.cFileName);

		/* All files start as unselected and unmatched */
		dir_entry->selected = 0;
		dir_entry->search_match = 0;

		dir_entry->size = ((uintmax_t)ffd.nFileSizeHigh << 32) + ffd.nFileSizeLow;
		dir_entry->mode = 0777;
		dir_entry->mtime = win_to_unix_time(ffd.ftLastWriteTime);
		dir_entry->atime = win_to_unix_time(ffd.ftLastAccessTime);
		dir_entry->ctime = win_to_unix_time(ffd.ftCreationTime);

		if(is_win_symlink(ffd.dwFileAttributes, ffd.dwReserved0))
		{
			if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				strcat(dir_entry->name, "/");
			dir_entry->type = LINK;
		}
		else if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			strcat(dir_entry->name, "/");
			dir_entry->type = DIRECTORY;
		}
		else if(is_executable(dir_entry))
		{
			dir_entry->type = EXECUTABLE;
		}
		else
		{
			dir_entry->type = REGULAR;
		}
		view->list_rows++;
	} while(FindNextFileA(hfind, &ffd));

	FindClose(hfind);
	return 0;
#endif
}

void
load_dir_list(FileView *view, int reload)
{
#ifndef _WIN32
	struct stat s;
#endif
	int x;
	int old_list = view->list_rows;
	int need_free = (view->selected_filelist == NULL);

	view->filtered = 0;

#ifndef _WIN32
	if(stat(view->curr_dir, &s) != 0)
		return;
#endif

	if(update_dir_mtime(view) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		return;
	}

#ifndef _WIN32
	if(!reload && s.st_size > s.st_blksize)
#else
	if(!reload)
#endif
		status_bar_message("Reading directory...");

	update_all_windows();

	/* this is needed for lstat() below */
	if(my_chdir(view->curr_dir) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() into \"%s\"", view->curr_dir);
		return;
	}

	if(reload && view->selected_files > 0 && view->selected_filelist == NULL)
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
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory.");
		return;
	}

	if(fill_dir_list(view) != 0)
	{
		/* we don't have read access, only execute, or there are other problems */
		load_parent_dir_only(view);
	}

#ifndef _WIN32
	if(!reload && s.st_size > s.st_blksize)
#else
	if(!reload)
#endif
		status_bar_message("Sorting directory...");
	sort_view(view);

	/* If reloading the same directory don't jump to
	 * history position.  Stay at the current line
	 */
	if(!reload)
		check_view_dir_history(view);

	view->color_scheme = check_directory_for_color_scheme(view == &lwin,
			view->curr_dir);

	/*
	 * It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directorys will always show at least the
	 * ../ file.  This resets the filter and reloads the directory.
	 */
	if(view->list_rows < 1)
	{
		(void)show_error_msgf("Filter error",
				"The %s pattern %s did not match any files. It was reset.",
				view->filename_filter, view->invert ? "inverted" : "");
		view->filename_filter = (char *)realloc(view->filename_filter, 1);
		if(view->filename_filter == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			return;
		}
		strcpy(view->filename_filter, "");
		view->invert = !view->invert;

		load_dir_list(view, 1);

		if(curr_stats.vifm_started >= 2)
			draw_dir_list(view, view->top_line);
		return;
	}
	if(reload && view->selected_files)
		reset_selected_files(view, need_free);
	else if(view->selected_files)
		view->selected_files = 0;

	if(curr_stats.vifm_started >= 2)
		draw_dir_list(view, view->top_line);

	if(view == curr_view)
	{
		if(strncmp(view->curr_dir, cfg.fuse_home, strlen(cfg.fuse_home)) == 0 &&
				strcmp(other_view->curr_dir, view->curr_dir) == 0)
			load_dir_list(other_view, 1);
	}
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
	move_to_list_pos(view, view->list_pos);
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
	move_to_list_pos(view, view->list_pos);
}

static void
reload_window(FileView *view)
{
	curr_stats.skip_history = 1;

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
	if(is_on_slow_fs(view->curr_dir))
		return;

#ifndef _WIN32
	struct stat s;
	if(stat(view->curr_dir, &s) != 0)
#else
	FILETIME ft;
	if(is_unc_root(view->curr_dir))
		return;
	if(get_dir_mtime(view->curr_dir, &ft) != 0)
#endif
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		log_cwd();

		(void)show_error_msgf("Directory Change Check", "Cannot open %s",
				view->curr_dir);

		leave_invalid_dir(view, view->curr_dir);
		(void)change_directory(view, view->curr_dir);
		clean_selected_files(view);
		reload_window(view);
		return;
	}

#ifndef _WIN32
	if(s.st_mtime != view->dir_mtime)
#else
	if(CompareFileTime(&ft, &view->dir_mtime) != 0)
#endif
		reload_window(view);
}

void
load_saving_pos(FileView *view, int reload)
{
	char filename[NAME_MAX];
	int pos;

	if(curr_stats.vifm_started < 2)
		return;

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	load_dir_list(view, reload);
	pos = find_file_pos_in_list(view, filename);
	pos = (pos >= 0) ? pos : view->list_pos;
	if(view == curr_view)
	{
		move_to_list_pos(view, pos);
	}
	else
	{
		mvwaddstr(view->win, view->curr_line, 0, " ");
		view->list_pos = pos;
		if(move_curr_line(view, pos))
			draw_dir_list(view, view->top_line);
		mvwaddstr(view->win, view->curr_line, 0, "*");
	}

	if(curr_stats.number_of_windows != 1 || view == curr_view)
		wrefresh(view->win);
}

void
change_sort_type(FileView *view, char type, char descending)
{
	int i;

	view->sort[0] = descending ? -type : type;
	for(i = 1; i < NUM_SORT_OPTIONS; i++)
		view->sort[i] = NUM_SORT_OPTIONS + 1;

	load_sort(view);

	load_saving_pos(view, 1);
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
