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

#include <ncurses.h>
#include <string.h> /* strrchr */

#include "filelist.h"
#include "ui.h"

/*
 * This function is from the git program written by Tudor Hulubei and
 * Andrei Pitis
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
				 if(first->name[0] == '.' && second->name[0] != '.')
					 return -1;
				 else
					 if(first->name[0] != '.' && second->name[0] == '.')
						 return 1;
				 break;

		 case SORT_BY_EXTENSION:
				 pfirst  = strrchr(first->name,  '.');
				 psecond = strrchr(second->name, '.');

				 if(pfirst && psecond)
				 {
					 retval = strcmp(++pfirst, ++psecond);
					 if(retval != 0)
							 return retval;
				 }
				 else
					 if(pfirst || psecond)
							 return (pfirst ? -1 : 1);
				 break;

		 case SORT_BY_SIZE_ASCENDING:
				 if(first->size == second->size)
						break;
				 return first->size > second->size;

		 case SORT_BY_SIZE_DESCENDING:
				 if(first->size == second->size)
					 break;
				 return first->size < second->size;

		 case SORT_BY_TIME_MODIFIED:
				 if(first->mtime == second->mtime)
						break;
				 return first->mtime - second->mtime;

		 case SORT_BY_TIME_ACCESSED:
				 if(first->atime == second->atime)
						break;
				 return first->atime - second->atime;

		 case SORT_BY_TIME_CHANGED:
				 if(first->ctime == second->ctime)
						break;
				 return first->ctime - second->ctime;

		 case SORT_BY_MODE:
				 if(first->mode == second->mode)
						break;
				 return first->mode - second->mode;

		 case SORT_BY_OWNER_ID:
				 if(first->uid == second->uid)
						break;
				 return first->uid - second->uid;

		 case SORT_BY_GROUP_ID:
				 if(first->gid == second->gid)
						break;
				 return first->gid - second->gid;

		 case SORT_BY_OWNER_NAME:
				 if(first->uid == second->uid)
						break;
				 return first->uid - second->uid;

		 case SORT_BY_GROUP_NAME:
				 if(first->gid == second->gid)
						break;
				 return first->gid - second->gid;
		 default:
				 break;
	}

	return strcmp(first->name, second->name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
