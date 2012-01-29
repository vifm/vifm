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

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../config.h"

#include "config.h"
#include "filelist.h"
#include "file_magic.h"
#include "keys.h"
#include "menus.h"
#include "modes.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

#include "file_info.h"

static void leave_file_info_mode(void);
static int show_file_type(FileView *view, int curr_y);
static int show_mime_type(FileView *view, int curr_y);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);

static int *mode;
static FileView *view;

static keys_add_info_t builtin_cmds[] = {
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* return */
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
};

void
init_file_info_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), FILE_INFO_MODE);
	assert(ret_code == 0);
}

void
enter_file_info_mode(FileView *v)
{
	*mode = FILE_INFO_MODE;
	view = v;
	curr_stats.show_full = 1;
	redraw_file_info_dialog();
}

static void
leave_file_info_mode(void)
{
	curr_stats.show_full = 0;
	curr_stats.need_redraw = 1;
	*mode = NORMAL_MODE;
}

void
redraw_file_info_dialog(void)
{
	char name_buf[NAME_MAX];
	char perm_buf[26];
	char size_buf[56];
	char buf[256];
#ifndef _WIN32
	char uid_buf[26];
	struct passwd *pwd_buf;
	struct group *grp_buf;
#endif
	struct tm *tm_ptr;
	int x;
	int curr_y;
	unsigned long long size;

	assert(view != NULL);

	setup_menu();

	x = getmaxx(menu_win);
	wclear(menu_win);

	snprintf(name_buf, sizeof(name_buf), "%s",
			view->dir_entry[view->list_pos].name);

	size = 0;
	if(view->dir_entry[view->list_pos].type == DIRECTORY)
		tree_get_data(curr_stats.dirsize_cache,
				view->dir_entry[view->list_pos].name, &size);

	if(size == 0)
		size = view->dir_entry[view->list_pos].size;

	friendly_size_notation(size, sizeof(size_buf), size_buf);

#ifndef _WIN32
	if((pwd_buf = getpwuid(view->dir_entry[view->list_pos].uid)) == NULL)
	{
		snprintf(uid_buf, sizeof(uid_buf), "%d",
				(int) view->dir_entry[view->list_pos].uid);
	}
	else
	{
		snprintf(uid_buf, sizeof(uid_buf), "%s", pwd_buf->pw_name);
	}
	get_perm_string(perm_buf, sizeof(perm_buf),
			view->dir_entry[view->list_pos].mode);
#else
	snprintf(perm_buf, sizeof(perm_buf), "%s",
			attr_str_long(view->dir_entry[view->list_pos].attrs));
#endif

	curr_y = 2;
	mvwaddstr(menu_win, curr_y, 2, "File: ");
	name_buf[x - 8] = '\0';
	wmove(menu_win, curr_y, 8);
	wprint(menu_win, name_buf);
	curr_y += 2;
	mvwaddstr(menu_win, curr_y, 2, "Size: ");
	mvwaddstr(menu_win, curr_y, 8, size_buf);
	curr_y += 2;

	curr_y += show_file_type(view, curr_y);
	curr_y += show_mime_type(view, curr_y);

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Permissions: ");
#else
	mvwaddstr(menu_win, curr_y, 2, "Attributes: ");
#endif
	mvwaddstr(menu_win, curr_y, 15, perm_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Modified: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].mtime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Accessed: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].atime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Changed: ");
#else
	mvwaddstr(menu_win, curr_y, 2, "Created: ");
#endif
	tm_ptr = localtime(&view->dir_entry[view->list_pos].ctime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Owner: ");
	mvwaddstr(menu_win, curr_y, 10, uid_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Group: ");
	if((grp_buf = getgrgid(view->dir_entry[view->list_pos].gid)) != NULL)
		mvwaddstr(menu_win, curr_y, 10, grp_buf->gr_name);
#endif

	wnoutrefresh(menu_win);

	box(menu_win, 0, 0);
	wmove(menu_win, 0, 3);
	wprint(menu_win, " File Information ");
	wrefresh(menu_win);
}

/* Returns increment for curr_y */
static int
show_file_type(FileView *view, int curr_y)
{
	int x;
	int old_curr_y = curr_y;
	x = getmaxx(menu_win);

	mvwaddstr(menu_win, curr_y, 2, "Type: ");
	if(view->dir_entry[view->list_pos].type == LINK)
	{
		char linkto[PATH_MAX + NAME_MAX];
		char *filename = view->dir_entry[view->list_pos].name;

		mvwaddstr(menu_win, curr_y, 8, "Link");
		curr_y += 2;
		mvwaddstr(menu_win, curr_y, 2, "Link To: ");

		if(get_link_target(filename, linkto, sizeof(linkto)) == 0)
		{
			mvwaddnstr(menu_win, curr_y, 11, linkto, x - 11);

			if(access(linkto, F_OK) != 0)
				mvwaddstr(menu_win, curr_y - 2, 12, " (BROKEN)");
		}
		else
		{
			mvwaddstr(menu_win, curr_y, 11, "Couldn't Resolve Link");
		}
	}
	else if(view->dir_entry[view->list_pos].type == EXECUTABLE ||
			view->dir_entry[view->list_pos].type == REGULAR)
	{
#ifdef HAVE_FILE_PROG
		FILE *pipe;
		char command[1024];
		char buf[NAME_MAX];

		/* Use the file command to get file information */
		snprintf(command, sizeof(command), "file \"%s\" -b",
				view->dir_entry[view->list_pos].name);

		if((pipe = popen(command, "r")) == NULL)
		{
			mvwaddstr(menu_win, curr_y, 8, "Unable to open pipe to read file");
			return 2;
		}

		if(fgets(buf, sizeof(buf), pipe) != buf)
			strcpy(buf, "Pipe read error");

		pclose(pipe);

		mvwaddnstr(menu_win, curr_y, 8, buf, x - 9);
		if(strlen(buf) > x - 9)
			mvwaddnstr(menu_win, curr_y + 1, 8, buf + x - 9, x - 9);
#else /* #ifdef HAVE_FILE_PROG */
		if(view->dir_entry[view->list_pos].type == EXECUTABLE)
			mvwaddstr(menu_win, curr_y, 8, "Executable");
		else
			mvwaddstr(menu_win, curr_y, 8, "Regular File");
#endif /* #ifdef HAVE_FILE_PROG */
	}
	else if(view->dir_entry[view->list_pos].type == DIRECTORY)
	{
	  mvwaddstr(menu_win, curr_y, 8, "Directory");
	}
#ifndef _WIN32
	else if(S_ISCHR(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Character Device");
	}
	else if(S_ISBLK(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Block Device");
	}
	else if(view->dir_entry[view->list_pos].type == FIFO)
	{
	  mvwaddstr(menu_win, curr_y, 8, "Fifo Pipe");
	}
	else if(S_ISSOCK(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Socket");
	}
#endif
	else
	{
	  mvwaddstr(menu_win, curr_y, 8, "Unknown");
	}
	curr_y += 2;

	return curr_y - old_curr_y;
}

/* Returns increment for curr_y */
static int
show_mime_type(FileView *view, int curr_y)
{
	const char* mimetype = NULL;

	mimetype = get_mimetype(get_current_file_name(view));

	mvwaddstr(menu_win, curr_y, 2, "Mime Type: ");

	if(mimetype == NULL)
		mimetype = "Unknown";

	mvwaddstr(menu_win, curr_y, 13, mimetype);

	return 2;
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_file_info_mode();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
