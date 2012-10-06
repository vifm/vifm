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

#include <limits.h> /* PATH_MAX */
#include <string.h> /* strcpy() strlen() strcat() */

#include "cfg/config.h"
#include "modes/modes.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/utf8.h"
#include "filelist.h"
#include "filetype.h"
#include "status.h"
#include "ui.h"

#include "quickview.h"

static void view_wraped(FILE *fp, int x);
static void view_not_wraped(FILE *fp, int x);
static char * strchar2str(const char *str, int pos);

void
toggle_quick_view(void)
{
	if(curr_stats.view)
	{
		curr_stats.view = 0;

		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}
	else
	{
		curr_stats.view = 1;
		quick_view_file(curr_view);
	}
}

void
quick_view_file(FileView *view)
{
	int x = 0;
	int y = 1;
	char buf[PATH_MAX];
	char link[PATH_MAX];

	if(curr_stats.load_stage < 2)
		return;

	if(get_mode() == VIEW_MODE)
		return;

	if(curr_stats.number_of_windows == 1)
		return;

	werase(other_view->win);
	werase(other_view->title);
	mvwaddstr(other_view->title, 0, 0, "File: ");
	wprint(other_view->title, view->dir_entry[view->list_pos].name);

	snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	switch(view->dir_entry[view->list_pos].type)
	{
		case CHARACTER_DEVICE:
			mvwaddstr(other_view->win, ++x, y, "File is a Character Device");
			break;
		case BLOCK_DEVICE:
			mvwaddstr(other_view->win, ++x, y, "File is a Block Device");
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
			if(get_link_target(buf, link, sizeof(link)) != 0)
			{
				mvwaddstr(other_view->win, ++x, y, "Cannot resolve Link");
				break;
			}
			if(is_path_absolute(link))
				strcpy(buf, link);
			else
				snprintf(buf, sizeof(buf), "%s/%s", view->curr_dir, link);
			if(!ends_with_slash(buf) && is_dir(buf))
			{
				strcat(buf, "/");
			}
			/* break intensionally omitted */
		case UNKNOWN:
		default:
			{
				const char *viewer;
				FILE *fp;

				viewer = get_viewer_for_file(buf);
				if(viewer == NULL && is_dir(buf))
				{
					mvwaddstr(other_view->win, ++x, y, "File is a Directory");
					break;
				}
				if(viewer != NULL && viewer[0] != '\0')
					fp = use_info_prog(viewer);
				else
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
	refresh_view_win(other_view);
	wrefresh(other_view->title);
}

static void
view_wraped(FILE *fp, int x)
{
	char line[1024];
	int y = 1;
	int offset = 0;
	char *res = get_line(fp, line + offset, other_view->window_width);
	while(res != NULL && x <= other_view->window_rows - 2)
	{
		int i, k;
		size_t n_len = get_normal_utf8_string_length(line);
		size_t len = strlen(line);
		while(n_len < other_view->window_width - 1 && line[len - 1] != '\n'
				&& !feof(fp))
		{
			if(get_line(fp, line + len, other_view->window_width - n_len) == NULL)
				break;
			n_len = get_normal_utf8_string_length(line);
			len = strlen(line);
		}

		if(len > 0 && line[len - 1] != '\n')
			remove_eol(fp);

		++x;
		wmove(other_view->win, x, y);
		i = 0;
		k = other_view->window_width - 1;
		len = 0;
		while(k-- > 0 && line[i] != '\0')
		{
			wprint(other_view->win, strchar2str(line + i, len));
			if(line[i] == '\t')
			{
				int tab_width = cfg.tab_stop - len%cfg.tab_stop;
				len += tab_width;
				k -= tab_width - 1;
			}
			else
			{
				len++;
			}
			i += get_char_width(line + i);
		}

		offset = strlen(line) - i;
		if(offset != 0)
		{
			memmove(line, line + i, offset + 1);
			if(offset == 1 && line[0] == '\n')
			{
				offset = 0;
				res = get_line(fp, line + offset, other_view->window_width);
			}
		}
		else
		{
			res = get_line(fp, line + offset, other_view->window_width);
		}
	}
}

static void
view_not_wraped(FILE *fp, int x)
{
	char line[1024];
	int y = 1;

	while(get_line(fp, line, other_view->window_width - 2) == line &&
			x <= other_view->window_rows - 2)
	{
		int i;
		size_t n_len = get_normal_utf8_string_length(line);
		size_t len = strlen(line);
		while(n_len < other_view->window_width - 1 && line[len - 1] != '\n'
				&& !feof(fp))
		{
			if(get_line(fp, line + len, other_view->window_width - n_len) == NULL)
				break;
			n_len = get_normal_utf8_string_length(line);
			len = strlen(line);
		}

		if(line[len - 1] != '\n')
			skip_until_eol(fp);

		len = 0;
		n_len = 0;
		for(i = 0; line[i] != '\0' && len <= other_view->window_width - 1;)
		{
			size_t cl = get_char_width(line + i);
			n_len++;

			if(cl == 1 && (unsigned char)line[i] == '\t')
				len += cfg.tab_stop - len%cfg.tab_stop;
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
		len = 0;
		while(n_len--)
		{
			wprint(other_view->win, strchar2str(line + i, len));
			if(line[i] == '\t')
				len += cfg.tab_stop - len%cfg.tab_stop;
			else
				len++;
			i += get_char_width(line + i);
		}
	}
}

static char *
strchar2str(const char *str, int pos)
{
	static char buf[16];

	size_t len = get_char_width(str);
	if(len != 1 || str[0] >= ' ' || str[0] == '\n')
	{
		memcpy(buf, str, len);
		buf[len] = '\0';
	}
	else if(str[0] == '\r')
	{
		strcpy(buf, "<cr>");
	}
	else if(str[0] == '\t')
	{
		len = cfg.tab_stop - pos%cfg.tab_stop;
		buf[0] = '\0';
		while(len-- > 0)
			strcat(buf, " ");
	}
	else if((unsigned char)str[0] < (unsigned char)' ')
	{
		buf[0] = '^';
		buf[1] = ('A' - 1) + str[0];
		buf[2] = '\0';
	}
	else
	{
		buf[0] = str[0];
		buf[1] = '\0';
	}
	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
