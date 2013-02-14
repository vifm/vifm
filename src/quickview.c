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

#include <ctype.h> /* isdigit() */
#include <stddef.h> /* size_t */
#include <string.h> /* memset() strcpy() strlen() strcat() */

#include "cfg/config.h"
#include "modes/modes.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/utf8.h"
#include "color_manager.h"
#include "filelist.h"
#include "filetype.h"
#include "status.h"
#include "ui.h"

#include "quickview.h"

static void view_wraped(FILE *fp, int x);
static void view_not_wraped(FILE *fp, int x);
static size_t get_esc_overhead(const char str[]);
static size_t get_char_width_esc(const char str[]);
static void print_char_esc(WINDOW *win, const char str[]);
static const char * strchar2str(const char str[], int pos,
		size_t *char_width_esc);

static int attrs = 0;
static int fg = -1;
static int bg = -1;

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
			if(get_link_target_abs(buf, view->curr_dir, buf, sizeof(buf)) != 0)
			{
				mvwaddstr(other_view->win, ++x, y, "Cannot resolve Link");
				break;
			}
			if(!ends_with_slash(buf) && is_dir(buf))
			{
				strncat(buf, "/", sizeof(buf) - 1);
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

				colmgr_reset();
				wattrset(other_view->win, 0);
				attrs = 0;
				fg = -1;
				bg = -1;
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

		x++;
		wmove(other_view->win, x, y);
		i = 0;
		k = other_view->window_width - 1;
		len = 0;
		while(k-- > 0 && line[i] != '\0')
		{
			size_t char_width_esc;
			wprint(other_view->win, strchar2str(line + i, len, &char_width_esc));
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
		size_t n_len = get_normal_utf8_string_length(line) - get_esc_overhead(line);
		size_t len = strlen(line);
		while(n_len < other_view->window_width - 1 && line[len - 1] != '\n'
				&& !feof(fp))
		{
			if(get_line(fp, line + len, other_view->window_width - n_len) == NULL)
				break;
			n_len = get_normal_utf8_string_length(line) - get_esc_overhead(line);
			len = strlen(line);
		}

		if(line[len - 1] != '\n')
			skip_until_eol(fp);

		x++;
		wmove(other_view->win, x, y);
		i = 0;
		len = 0;
		while(len <= other_view->window_width - 1 && line[i] != '\0')
		{
			size_t char_width_esc;
			const char *const char_str = strchar2str(line + i, len, &char_width_esc);
			print_char_esc(other_view->win, char_str);
			len += char_width_esc;
			i += get_char_width_esc(line + i);
		}
	}
}

/* Returns number of characters in the str taken by terminal escape
 * sequences. */
static size_t
get_esc_overhead(const char str[])
{
	size_t overhead = 0U;
	while(*str != '\0')
	{
		const size_t char_width_esc = get_char_width_esc(str);
		if(*str == '\033')
		{
			overhead += char_width_esc;
		}
		str += char_width_esc;
	}
	return overhead;
}

/* Returns number of characters at the beginning of the str which form one
 * logical symbol.  Takes UTF-8 encoding and terminal escape sequences into
 * account. */
static size_t
get_char_width_esc(const char str[])
{
	if(*str != '\033')
	{
		return get_char_width(str);
	}
	else
	{
		const char *pos = strchr(str, 'm');
		pos = (pos == NULL) ? (str + strlen(str)) : (pos + 1);
		return pos - str;
	}
}

/* Prints the leading character of the str to the win window parsing terminal
 * escape sequences. */
static void
print_char_esc(WINDOW *win, const char str[])
{
	if(str[0] == '\033')
	{
		int next_pair;

		/* Handle escape sequence. */
		str++;
		do
		{
			int n = 0;
			if(isdigit(str[1]))
			{
				char *end;
				n = strtol(str + 1, &end, 10);
				str = end;
			}
			else
			{
				str++;
			}
			if(n == 0)
			{
				attrs = A_NORMAL;
				fg = -1;
				bg = -1;
			}
			else if(n == 1)
			{
				attrs |= A_BOLD;
			}
			else if(n == 1)
			{
				attrs |= A_DIM;
			}
			else if(n == 3 || n == 7)
			{
				attrs |= A_REVERSE;
			}
			else if(n == 4)
			{
				attrs |= A_UNDERLINE;
			}
			else if(n == 5 || n == 6)
			{
				attrs |= A_BLINK;
			}
			else if(n == 22)
			{
				attrs &= ~(A_BOLD | A_UNDERLINE | A_BLINK | A_REVERSE | A_DIM);
			}
			else if(n == 24)
			{
				attrs &= ~A_UNDERLINE;
			}
			else if(n == 25)
			{
				attrs &= ~A_BLINK;
			}
			else if(n == 27)
			{
				attrs &= ~A_REVERSE;
			}
			else if(n >= 30 && n <= 37)
			{
				fg = n - 30;
			}
			else if(n == 39)
			{
				fg = -1;
			}
			else if(n >= 40 && n <= 47)
			{
				bg = n - 40;
			}
			else if(n == 49)
			{
				bg = -1;
			}
		}
		while(str[0] == ';');

		next_pair = colmgr_alloc_pair(fg, bg);
		wattrset(win, COLOR_PAIR(next_pair) | attrs);
	}
	else
	{
		/* Print symbol. */
		wprint(win, str);
	}
}

/* Converts first the leading character of the str string to a printable string.
 * Puts number of screen character positions taken by the resulting string
 * representation of a character.  Returns pointer to a statically allocated
 * buffer. */
static const char *
strchar2str(const char str[], int pos, size_t *char_width_esc)
{
	static char buf[32];

	const size_t char_width = get_char_width(str);
	*char_width_esc = char_width;
	if(char_width != 1 || str[0] >= ' ' || str[0] == '\n')
	{
		memcpy(buf, str, char_width);
		buf[char_width] = '\0';
	}
	else if(str[0] == '\r')
	{
		strcpy(buf, "<cr>");
	}
	else if(str[0] == '\t')
	{
		const size_t space_count = cfg.tab_stop - pos%cfg.tab_stop;
		*char_width_esc = space_count;
		memset(buf, ' ', space_count);
		buf[space_count] = '\0';
	}
	else if(str[0] == '\033')
	{
		char *dst = buf;
		*char_width_esc = 0;
		while(str[0] != 'm' && str[0] != '\0' && dst - buf < sizeof(buf) - 1)
		{
			*dst++ = *str++;
		}
		*dst = '\0';
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
