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

#include<stdio.h>
#include<sys/time.h>
#include<time.h>
#include <unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>
#include<sys/stat.h>
#include<pwd.h>
#include<grp.h>

#include "config.h"
#include "filelist.h"
#include "ui.h"
#include "utils.h"
#include "menus.h"
#include "status.h"

/*
#ifndef OFF_T_MAX
# define OFF_T_MAX TYPE_MAXIMUM(off_t)
#endif

#ifndef OFF_T_MIN
# define OFF_T_MIN TYPE_MINIMUM(off_t)
#endif

uintmax_t
unsigned_file_size(off_t size)
{
	return size + (size < 0) * ((uintmax_t) OFF_T_MAX - OFF_T_MIN + 1);
}
*/

void
get_perm_string (char * buf, int len, mode_t mode)
{
  char *perm_sets[] =
    { "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx" };
  int u, g, o;

  u = (mode & S_IRWXU) >> 6;
  g = (mode & S_IRWXG) >> 3;
  o = (mode & S_IRWXO);

  snprintf (buf, len, "-%s%s%s", perm_sets[u], perm_sets[g], perm_sets[o]);

  if (S_ISLNK (mode))
    buf[0] = 'l';
  else if (S_ISDIR (mode))
    buf[0] = 'd';
  else if (S_ISBLK (mode))
    buf[0] = 'b';
  else if (S_ISCHR (mode))
    buf[0] = 'c';
  else if (S_ISFIFO (mode))
    buf[0] = 'f';
  else if (S_ISSOCK (mode))
    buf[0] = 's';

  if (mode & S_ISVTX)
    buf[9] = (buf[9] == '-') ? 'T' : 't';
  if (mode & S_ISGID)
    buf[6] = (buf[6] == '-') ? 'S' : 's';
  if (mode & S_ISUID)
    buf[3] = (buf[3] == '-') ? 'S' : 's';
}

void
show_full_file_properties(FileView *view)
{
	char name_buf[NAME_MAX];
	char perm_buf[26];
	char size_buf[56];
	char uid_buf[26];
	char buf[256];
	struct passwd *pwd_buf;
  	struct group *grp_buf;
	struct tm *tm_ptr;
	int x, y;
	int key = 0;
	int done = 0;


	curr_stats.show_full = 0;

	setup_menu(view);

	getmaxyx(menu_win, y, x);
	werase(menu_win);

	snprintf(name_buf, sizeof(name_buf), "%s", 
			view->dir_entry[view->list_pos].name);

	friendly_size_notation(view->dir_entry[view->list_pos].size,
            sizeof(size_buf), size_buf);
	
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

	mvwaddstr(menu_win, 2, 2, "File: ");
	mvwaddnstr(menu_win, 2, 8, name_buf, x - 8);
	mvwaddstr(menu_win, 4, 2, "Size: ");
	mvwaddstr(menu_win, 4, 8, size_buf);

	mvwaddstr(menu_win, 6, 2, "Type: ");
	if(S_ISLNK(view->dir_entry[view->list_pos].mode))
	{
		char linkto[PATH_MAX +NAME_MAX];
		int len;
		char *filename = strdup(view->dir_entry[view->list_pos].name);
		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[len - 1] = '\0';

	  	mvwaddstr(menu_win, 6, 8, "Link");
		len = readlink (filename, linkto, sizeof (linkto));

		mvwaddstr(menu_win, 7, 2, "Link To: ");
		if (len > 0)
		{
			linkto[len] = '\0';
			mvwaddnstr(menu_win, 7, 11, linkto, x - 11);
		}
	  	else
			mvwaddstr(menu_win, 7, 11, "Couldn't Resolve Link");
	}
 	else if (S_ISREG (view->dir_entry[view->list_pos].mode))
   	{
		FILE *pipe;
		char command[1024];
		char buf[NAME_MAX];

		/* Use the file command to get file information */
		snprintf(command, sizeof(command), "file \"%s\" -b", 
				view->dir_entry[view->list_pos].name);

		if ((pipe = popen(command, "r")) == NULL)
		{
			mvwaddstr(menu_win, 6, 8, "Unable to open pipe to read file");
			return;
		}

		fgets(buf, sizeof(buf), pipe);

		pclose(pipe);

		mvwaddnstr(menu_win, 6, 8, buf, x - 9);
		/*
		if((S_IXUSR &view->dir_entry[view->list_pos].mode)
				|| (S_IXGRP &view->dir_entry[view->list_pos].mode)
				|| (S_IXOTH &view->dir_entry[view->list_pos].mode))

			mvwaddstr(other_view->win, 6, 8, "Executable");
		else
			mvwaddstr(other_view->win, 6, 8, "Regular File");
			*/
	}
	else if (S_ISDIR (view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, 6, 8, "Directory");
	}
	else if (S_ISCHR (view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, 6, 8, "Character Device");
	}
	else if (S_ISBLK (view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, 6, 8, "Block Device");
	}
	else if (S_ISFIFO (view->dir_entry[view->list_pos].mode))
	{
	  mvwaddstr(menu_win, 6, 8, "Fifo Pipe");
	}
    else if (S_ISSOCK (view->dir_entry[view->list_pos].mode))
    {
	  mvwaddstr(menu_win, 6, 8, "Socket");
    }
  	else
    {
	  mvwaddstr(menu_win, 6, 8, "Unknown");
    }

	mvwaddstr(menu_win, 8, 2, "Permissions: ");
	mvwaddstr(menu_win, 8, 15, perm_buf);
	mvwaddstr(menu_win, 10, 2, "Modified: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].mtime);
  	strftime (buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, 10, 13, buf);
	mvwaddstr(menu_win, 12, 2, "Accessed: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].atime);
  	strftime (buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, 12, 13, buf);
	mvwaddstr(menu_win, 14, 2, "Changed: ");
	tm_ptr = localtime(&view->dir_entry[view->list_pos].ctime);
  	strftime (buf, sizeof (buf), "%a %b %d %I:%M %p", tm_ptr);
	mvwaddstr(menu_win, 14, 13, buf);
	mvwaddstr(menu_win, 16, 2, "Owner: ");
	mvwaddstr(menu_win, 16, 10, uid_buf);
	mvwaddstr(menu_win, 18, 2, "Group: ");
	if((grp_buf = getgrgid(view->dir_entry[view->list_pos].gid)) != NULL)
	{
		mvwaddstr(menu_win, 18, 10, grp_buf->gr_name);
	}
	wnoutrefresh(menu_win);

	box(menu_win, 0, 0);
	wrefresh(menu_win);
	while(!done)
	{
		key = wgetch(menu_win);
	
		/* ascii Return - Ctrl-c  - Escape */
		if(key == 13 || key == 3 || key == 27)
			done = 1;
	}
	werase(menu_win);
	curr_stats.menu = 0;
	redraw_window();
}


