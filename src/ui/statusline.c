/* vifm
 * Copyright (C) 2014 xaizek.
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

#include "statusline.h"

#include <curses.h> /* mvwin() wbkgdset() werase() */

#include <ctype.h> /* isdigit() */
#ifndef _WIN32
#include <grp.h> /* getgrgid_r() */
#include <pwd.h> /* getpwuid_r() */
#endif
#include <string.h> /* strcat() strdup() strlen() */
#include <unistd.h> /* _SC_* sysconf() */

#include "../cfg/config.h"
#include "../engine/mode.h"
#include "../modes/modes.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "ui.h"

#include "../utils/str.h"

static void update_stat_window_old(FileView *view);
TSTATIC char * expand_status_line_macros(FileView *view, const char format[]);
static void get_uid_string(FileView *view, size_t len, char *out_buf);
static void get_gid_string(FileView *view, size_t len, char *out_buf);

void
update_stat_window(FileView *view)
{
	int x;
	char *buf;

	if(!cfg.last_status)
		return;

	/* Don't redraw anything until :restart command is finished. */
	if(curr_stats.restart_in_progress)
	{
		return;
	}

	if(cfg.status_line[0] == '\0')
	{
		update_stat_window_old(view);
		return;
	}

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	buf = expand_status_line_macros(view, cfg.status_line);
	buf = break_in_two(buf, getmaxx(stdscr));

	werase(stat_win);
	checked_wmove(stat_win, 0, 0);
	wprint(stat_win, buf);
	wrefresh(stat_win);

	free(buf);
}

/* Formats status line in the "old way" (before introduction of 'statusline'
 * option). */
static void
update_stat_window_old(FileView *view)
{
	char name_buf[160*2 + 1];
	char perm_buf[26];
	char size_buf[56];
	char id_buf[52];
	int x;
	int cur_x;
	size_t print_width;
	char *filename;

	if(!cfg.last_status)
		return;

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(DCOLOR_BASE + STATUS_LINE_COLOR) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	filename = get_current_file_name(view);
	print_width = get_real_string_width(filename, 20 + MAX(0, x - 83));
	snprintf(name_buf, MIN(sizeof(name_buf), print_width + 1), "%s", filename);
	friendly_size_notation(view->dir_entry[view->list_pos].size, sizeof(size_buf),
			size_buf);

	get_uid_string(view, sizeof(id_buf), id_buf);
	if(id_buf[0] != '\0')
		strcat(id_buf, ":");
	get_gid_string(view, sizeof(id_buf) - strlen(id_buf),
			id_buf + strlen(id_buf));
#ifndef _WIN32
	get_perm_string(perm_buf, sizeof(perm_buf),
			view->dir_entry[view->list_pos].mode);
#else
	snprintf(perm_buf, sizeof(perm_buf), "%s",
			attr_str_long(view->dir_entry[view->list_pos].attrs));
#endif

	werase(stat_win);
	cur_x = 2;
	checked_wmove(stat_win, 0, cur_x);
	wprint(stat_win, name_buf);
	cur_x += 22;
	if(x > 83)
		cur_x += x - 83;
	mvwaddstr(stat_win, 0, cur_x, size_buf);
	cur_x += 12;
	mvwaddstr(stat_win, 0, cur_x, perm_buf);
	cur_x += 11;

	snprintf(name_buf, sizeof(name_buf), "%d %s filtered", view->filtered,
			(view->filtered == 1) ? "file" : "files");
	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) + 2), name_buf);

	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		break_at(id_buf, ':');
	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		id_buf[0] = '\0';
	mvwaddstr(stat_win, 0, cur_x, id_buf);

	wrefresh(stat_win);
}

/* Returns newly allocated string, which should be freed by the caller, or NULL
 * if there is not enough memory. */
TSTATIC char *
expand_status_line_macros(FileView *view, const char format[])
{
	static const char STATUS_CHARS[] = "tAugsEd-lLS%0123456789";

	char *result = strdup("");
	size_t len = 0;
	char c;

	while((c = *format++) != '\0')
	{
		size_t width = 0;
		int left_align = 0;
		char buf[PATH_MAX];
		const char *const next = format;
		int ok;

		if(c != '%' || !char_is_one_of(STATUS_CHARS, *format))
		{
			if(strappendch(&result, &len, c) != 0)
			{
				break;
			}
			continue;
		}

		if(*format == '-')
		{
			left_align = 1;
			format++;
		}

		while(isdigit(*format))
		{
			width = width*10 + *format++ - '0';
		}
		c = *format++;

		ok = 1;
		switch(c)
		{
			case 't':
				format_entry_name(curr_view, view->list_pos, sizeof(buf), buf);
				break;
			case 'A':
#ifndef _WIN32
				get_perm_string(buf, sizeof(buf), view->dir_entry[view->list_pos].mode);
#else
				snprintf(buf, sizeof(buf), "%s",
						attr_str_long(view->dir_entry[view->list_pos].attrs));
#endif
				break;
			case 'u':
				get_uid_string(view, sizeof(buf), buf);
				break;
			case 'g':
				get_gid_string(view, sizeof(buf), buf);
				break;
			case 's':
				friendly_size_notation(view->dir_entry[view->list_pos].size,
						sizeof(buf), buf);
				break;
			case 'E':
				{
					uint64_t size = 0;
					if(view->selected_files > 0)
					{
						int i;
						for(i = 0; i < view->list_rows; i++)
						{
							if(view->dir_entry[i].selected)
							{
								size += get_file_size_by_entry(view, i);
							}
						}
					}
					/* Make exception for VISUAL_MODE, since it can contain empty
					 * selection when cursor is on ../ directory. */
					else if(!vle_mode_is(VISUAL_MODE))
					{
						size = get_file_size_by_entry(view, view->list_pos);
					}
					friendly_size_notation(size, sizeof(buf), buf);
				}
				break;
			case 'd':
				{
					struct tm *tm_ptr = localtime(&view->dir_entry[view->list_pos].mtime);
					strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
				}
				break;
			case '-':
				snprintf(buf, sizeof(buf), "%d", view->filtered);
				break;
			case 'l':
				snprintf(buf, sizeof(buf), "%d", view->list_pos + 1);
				break;
			case 'L':
				snprintf(buf, sizeof(buf), "%d", view->list_rows + view->filtered);
				break;
			case 'S':
				snprintf(buf, sizeof(buf), "%d", view->list_rows);
				break;
			case '%':
				snprintf(buf, sizeof(buf), "%%");
				break;

			default:
				LOG_INFO_MSG("Unexpected %%-sequence: %%%c", c);
				ok = 0;
				break;
		}

		if(!ok)
		{
			format = next;
			if(strappendch(&result, &len, '%') != 0)
			{
				break;
			}
			continue;
		}

		stralign(buf, width, ' ', left_align);

		if(strappend(&result, &len, buf) != 0)
		{
			break;
		}
	}

	return result;
}

static void
get_uid_string(FileView *view, size_t len, char *out_buf)
{
#ifndef _WIN32
	char buf[sysconf(_SC_GETPW_R_SIZE_MAX) + 1];
	char uid_buf[26];
	struct passwd pwd_b;
	struct passwd *pwd_buf;

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

	snprintf(out_buf, len, "%s", uid_buf);
#else
	out_buf[0] = '\0';
#endif
}

static void
get_gid_string(FileView *view, size_t len, char *out_buf)
{
#ifndef _WIN32
	char buf[sysconf(_SC_GETGR_R_SIZE_MAX) + 1];
	char gid_buf[26];
	struct group group_b;
	struct group *group_buf;

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

	snprintf(out_buf, len, "%s", gid_buf);
#else
	out_buf[0] = '\0';
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
