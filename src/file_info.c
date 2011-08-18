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

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"

#include "config.h"
#include "filelist.h"
#include "file_magic.h"
#include "menus.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

/* Returns increment for curr_y */
static int
show_file_type(FileView *view, int curr_y)
{
	int x, y;
	int old_curr_y = curr_y;
	getmaxyx(menu_win, y, x);

	mvwaddstr(menu_win, curr_y, 2, "Type: ");
	if(S_ISLNK(view->dir_entry[view->list_pos].mode))
	{
		char linkto[PATH_MAX +NAME_MAX];
		int len;
		char *filename = strdup(view->dir_entry[view->list_pos].name);
		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[len - 1] = '\0';

		mvwaddstr(menu_win, curr_y, 8, "Link");
		curr_y += 2;
		len = readlink (filename, linkto, sizeof (linkto));

		mvwaddstr(menu_win, curr_y, 2, "Link To: ");
		if(len > 0)
		{
			linkto[len] = '\0';
			mvwaddnstr(menu_win, curr_y, 11, linkto, x - 11);

			if(access(linkto, F_OK) != 0)
			{
				mvwaddstr(menu_win, curr_y - 2, 12, " (BROKEN)");
			}
		}
		else
			mvwaddstr(menu_win, curr_y, 11, "Couldn't Resolve Link");
	}
	else if(S_ISREG(view->dir_entry[view->list_pos].mode))
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
#else /* #ifdef HAVE_FILE_PROG */
		if((S_IXUSR & view->dir_entry[view->list_pos].mode)
				|| (S_IXGRP & view->dir_entry[view->list_pos].mode)
				|| (S_IXOTH & view->dir_entry[view->list_pos].mode))
			mvwaddstr(menu_win, curr_y, 8, "Executable");
		else
			mvwaddstr(menu_win, curr_y, 8, "Regular File");
#endif /* #ifdef HAVE_FILE_PROG */
	}
	else if(S_ISDIR(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Directory");
	}
	else if(S_ISCHR(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Character Device");
	}
	else if(S_ISBLK(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Block Device");
	}
	else if(S_ISFIFO(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Fifo Pipe");
	}
	else if(S_ISSOCK(view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Socket");
	}
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

void
redraw_full_file_properties(FileView *v)
{
	static FileView *view;

	char name_buf[NAME_MAX];
	char perm_buf[26];
	char size_buf[56];
	char uid_buf[26];
	char buf[256];
	struct passwd *pwd_buf;
	struct group *grp_buf;
	struct tm *tm_ptr;
	int x, y;
	int curr_y;
	unsigned long long size;

	if(v != NULL)
		view = v;

	setup_menu();

	getmaxyx(menu_win, y, x);
	werase(menu_win);

	snprintf(name_buf, sizeof(name_buf), "%s",
			view->dir_entry[view->list_pos].name);

	size = 0;
	if(view->dir_entry[view->list_pos].type == DIRECTORY)
		tree_get_data(curr_stats.dirsize_cache,
				view->dir_entry[view->list_pos].name, &size);

	if(size == 0)
		size = view->dir_entry[view->list_pos].size;

	friendly_size_notation(size, sizeof(size_buf), size_buf);

	if((pwd_buf = getpwuid(view->dir_entry[view->list_pos].uid)) == NULL)
	{
		snprintf (uid_buf, sizeof(uid_buf), "%d",
				(int) view->dir_entry[view->list_pos].uid);
	}
	else
	{
		snprintf(uid_buf, sizeof(uid_buf), "%s", pwd_buf->pw_name);
	}
	get_perm_string(perm_buf, sizeof(perm_buf),
			view->dir_entry[view->list_pos].mode);

	curr_y = 2;
	mvwaddstr(menu_win, curr_y, 2, "File: ");
	mvwaddnstr(menu_win, curr_y, 8, name_buf, x - 8);
	curr_y += 2;
	mvwaddstr(menu_win, curr_y, 2, "Size: ");
	mvwaddstr(menu_win, curr_y, 8, size_buf);
	curr_y += 2;

	curr_y += show_file_type(view, curr_y);
	curr_y += show_mime_type(view, curr_y);

	mvwaddstr(menu_win, curr_y, 2, "Permissions: ");
	mvwaddstr(menu_win, curr_y, 15, perm_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Modified: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].mtime);
	strftime(buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, curr_y, 13, buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Accessed: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].atime);
	strftime (buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, curr_y, 13, buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Changed: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].ctime);
	strftime (buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, curr_y, 13, buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Owner: ");
	mvwaddstr(menu_win, curr_y, 10, uid_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Group: ");
	if((grp_buf = getgrgid(view->dir_entry[view->list_pos].gid)) != NULL)
	{
		mvwaddstr(menu_win, curr_y, 10, grp_buf->gr_name);
	}
	wnoutrefresh(menu_win);

	box(menu_win, 0, 0);
	wrefresh(menu_win);
}

void
show_full_file_properties(FileView *view)
{
	int key;

	curr_stats.show_full = 1;

	redraw_full_file_properties(view);

	keypad(menu_win, TRUE);
  /* wait for Return or Ctrl-c or Esc or error */
	do
		key = wgetch(menu_win);
	while(key != 13 && key != 3 && key != 27 && key != ERR);

	werase(menu_win);

	curr_stats.show_full = 0;

	redraw_window();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
