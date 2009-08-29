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
#include<string.h> /* strrchr */

#include "color_scheme.h"
#include "config.h"
#include "filelist.h"
#include "keys.h"
#include "status.h"
#include "ui.h"

/*
 * This function is from the git program written by Tudor Hulubei and
 *  Andrei Pitis
 */
int
sort_dir_list(const void *one, const void *two)
{
	int retval;
	char *pfirst, *psecond;
	const dir_entry_t *first = (const dir_entry_t *) one;
	const dir_entry_t *second = (const dir_entry_t *) two;
	int first_is_dir = false;
	int second_is_dir = false;

	if(first->type == DIRECTORY)
		first_is_dir = true;
	else if (first->type == LINK)
		first_is_dir = is_link_dir(one);
	
	if(second->type == DIRECTORY)
		second_is_dir = true;
	else if (second->type == LINK)
		second_is_dir = is_link_dir(two);

	if(first_is_dir != second_is_dir)
		return first_is_dir ? -1 : 1;
	switch(curr_view->sort_type)
	{
		 case SORT_BY_NAME:
				 break;

		 case SORT_BY_EXTENSION:
				 pfirst  = strrchr(first->name,  '.');
				 psecond = strrchr(second->name, '.');

				 if (pfirst && psecond)
				 {
					 retval = strcmp(++pfirst, ++psecond);
					 if (retval != 0)
							 return retval;
				 }
				 else
					 if (pfirst || psecond)
							 return (pfirst ? -1 : 1);
				 break;

		 case SORT_BY_SIZE_ASCENDING:
				 if (first->size == second->size)
						break;
				 return first->size > second->size;

		 case SORT_BY_SIZE_DESCENDING:
				 if (first->size == second->size)
					 break;
				 return first->size < second->size;

		 case SORT_BY_TIME_MODIFIED:
				 if (first->mtime == second->mtime)
						break;
				 return first->mtime - second->mtime;

		 case SORT_BY_TIME_ACCESSED:
				 if (first->atime == second->atime)
						break;
				 return first->atime - second->atime;

		 case SORT_BY_TIME_CHANGED:
				 if (first->ctime == second->ctime)
						break;
				 return first->ctime - second->ctime;

		 case SORT_BY_MODE:
				 if (first->mode == second->mode)
						break;
				 return first->mode - second->mode;

		 case SORT_BY_OWNER_ID:
				 if (first->uid == second->uid)
						break;
				 return first->uid - second->uid;

		 case SORT_BY_GROUP_ID:
				 if (first->gid == second->gid)
						break;
				 return first->gid - second->gid;

		 case SORT_BY_OWNER_NAME:
				 if (first->uid == second->uid)
						break;
				 return first->uid - second->uid;

		 case SORT_BY_GROUP_NAME:
				 if (first->gid == second->gid)
						break;
				 return first->gid - second->gid;
		 default:
				 break;
	}

	return strcmp(first->name, second->name);
}

static void
reset_sort_menu(void)
{
	curs_set(0);
	werase(sort_win);
	update_all_windows();
	if(curr_stats.need_redraw)
		redraw_window();
}

static void
sort_key_cb(FileView *view)
{
	int done = 0;
	int abort = 0;
	int top = 2;
	int bottom = top + NUM_SORT_OPTIONS - 1;
	int curr = view->sort_type + 2;
	int col = 6;
	char filename[NAME_MAX];

	snprintf(filename, sizeof(filename), "%s", 
			view->dir_entry[view->list_pos].name);

	curs_set(0);
	wmove(sort_win, curr, col);
	wrefresh(sort_win);

	while(!done)
	{
		int key = wgetch(sort_win);

		switch(key)
		{
			case 'j':
				{
					mvwaddch(sort_win, curr, col, ' ');
					curr++;
					if(curr > bottom)
						curr--;

					mvwaddch(sort_win, curr, col, '*');
					wmove(sort_win, curr, col);
					wrefresh(sort_win);
				}
				break;
			case 'k':
				{

					mvwaddch(sort_win, curr, col, ' ');
					curr--;
					if(curr < top)
						curr++;

					mvwaddch(sort_win, curr, col, '*');
					wmove(sort_win, curr, col);
					wrefresh(sort_win);
				}
				break;
			case 3: /* ascii Ctrl C */
			case 27: /* ascii Escape */
				done = 1;
				abort = 1;
				break;
			case 'l': 
			case 13: /* ascii Return */
				view->sort_type = curr - 2;
				done = 1;
				break;
			default:
				break;
		}
	}

	if(abort)
	{
		reset_sort_menu();
		moveto_list_pos(view, find_file_pos_in_list(view, filename));
		return;
	}
	curr_stats.setting_change = 1;

	reset_sort_menu();
	load_dir_list(view, 1);
	moveto_list_pos(view, find_file_pos_in_list(view, filename));
}


void
show_sort_menu(FileView *view)
{
	int x, y;

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	mvwaddstr(view->win, view->curr_line, 0, "  ");
	curs_set(0);
	update_all_windows();
	//doupdate();
	werase(sort_win);
	box(sort_win, ACS_VLINE, ACS_HLINE);


	getmaxyx(sort_win, y, x);
	curs_set(1);
	mvwaddstr(sort_win, 0, (x - 6)/2, " Sort ");
	mvwaddstr(sort_win, 1, 2, " Sort files by:");
	mvwaddstr(sort_win, 2, 4, " [ ] File Extenstion");
	mvwaddstr(sort_win, 3, 4, " [ ] File Name");
	mvwaddstr(sort_win, 4, 4, " [ ] Group ID");
	mvwaddstr(sort_win, 5, 4, " [ ] Group Name");
	mvwaddstr(sort_win, 6, 4, " [ ] Mode");
	mvwaddstr(sort_win, 7, 4, " [ ] Owner ID");
	mvwaddstr(sort_win, 8, 4, " [ ] Owner Name");
	mvwaddstr(sort_win, 9, 4, " [ ] Size (Ascending)");
	mvwaddstr(sort_win, 10, 4, " [ ] Size (Descending)");
	mvwaddstr(sort_win, 11, 4, " [ ] Time Accessed");
	mvwaddstr(sort_win, 12, 4, " [ ] Time Changed");
	mvwaddstr(sort_win, 13, 4, " [ ] Time Modified");
	mvwaddch(sort_win, view->sort_type + 2, 6, '*');
	sort_key_cb(view);
}


