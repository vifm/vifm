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

#include <fcntl.h> /* access */
#include <sys/stat.h>

#include <string.h> /* strrchr */

#include "filelist.h"
#include "status.h"
#include "tree.h"
#include "ui.h"

static FileView* view;

void
set_view_to_sort(FileView *view_to_sort)
{
	view = view_to_sort;
}

static bool
is_link_dir(const dir_entry_t * path)
{
	struct stat s;
	stat(path->name, &s);

	return (s.st_mode & S_IFDIR);
}

/*
 * This function is from the git program written by Tudor Hulubei and
 * Andrei Pitis
 */
int
sort_dir_list(const void *one, const void *two)
{
	int retval;
	char *pfirst, *psecond;
	dir_entry_t *first = (dir_entry_t *) one;
	dir_entry_t *second = (dir_entry_t *) two;
	int first_is_dir = false;
	int second_is_dir = false;

	if(first->type == DIRECTORY)
		first_is_dir = true;
	else if(first->type == LINK)
		first_is_dir = is_link_dir(one);

	if(second->type == DIRECTORY)
		second_is_dir = true;
	else if(second->type == LINK)
		second_is_dir = is_link_dir(two);

	if(first_is_dir != second_is_dir)
		return first_is_dir ? -1 : 1;

	if(strcmp(first->name, "../") == 0)
		return -1;
	else if(strcmp(second->name, "../") == 0)
		return 1;

	retval = 0;
	switch(view->sort_type)
	{
		case SORT_BY_NAME:
			if(first->name[0] == '.' && second->name[0] != '.')
				retval = -1;
			else if(first->name[0] != '.' && second->name[0] == '.')
				retval = 1;
			else
				retval = strcmp(first->name, second->name);
			break;

		case SORT_BY_EXTENSION:
			pfirst  = strrchr(first->name,  '.');
			psecond = strrchr(second->name, '.');

			if(pfirst && psecond)
				retval = strcmp(++pfirst, ++psecond);
			else if(pfirst || psecond)
				retval = pfirst ? -1 : 1;
			else
				retval = strcmp(first->name, second->name);
			break;

		case SORT_BY_SIZE:
			{
				size_t size;

				size = 0;
				if(first_is_dir)
					size = (size_t)tree_get_data(curr_stats.dirsize_cache, first->name);
				if(size != 0)
					first->size = size;

				size = 0;
				if(second_is_dir)
					size = (size_t)tree_get_data(curr_stats.dirsize_cache, second->name);
				if(size != 0)
					second->size = size;

				retval = first->size - second->size;
			}
			break;

		case SORT_BY_TIME_MODIFIED:
			retval = first->mtime - second->mtime;
			break;

		case SORT_BY_TIME_ACCESSED:
			retval = first->atime - second->atime;
			break;

		case SORT_BY_TIME_CHANGED:
			retval = first->ctime - second->ctime;
			break;

		case SORT_BY_MODE:
			retval = first->mode - second->mode;
			break;

		case SORT_BY_OWNER_ID:
			retval = first->uid - second->uid;
			break;

		case SORT_BY_GROUP_ID:
			retval = first->gid - second->gid;
			break;

		case SORT_BY_OWNER_NAME:
			retval = first->uid - second->uid;
			break;

		case SORT_BY_GROUP_NAME:
			retval = first->gid - second->gid;
			break;
	}

	if(retval == 0)
		retval = strcmp(first->name, second->name);
	else if(view->sort_descending)
		retval = -retval;

	return retval;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
