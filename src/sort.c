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

#include <curses.h>

#include <fcntl.h> /* access */
#include <sys/stat.h>

#include <ctype.h>
#include <string.h> /* strrchr */

#include "config.h"
#include "filelist.h"
#include "macros.h"
#include "status.h"
#include "tree.h"
#include "ui.h"
#include "utils.h"

static FileView* view;

void
set_view_to_sort(FileView *view_to_sort)
{
	view = view_to_sort;
}

#ifndef TEST
static
#endif
int
compare_file_names(const char *s, const char *t)
{
	if(!cfg.sort_numbers)
		return strcmp(s, t);

	while(*s != '\0' && *t != '\0')
	{
		if(*s == *t)
		{
			s++;
			t++;
		}
		else if(isdigit(*s) && isdigit(*s))
		{
			int num_a, num_b;
			char *p;

			num_a = strtol(s, &p, 10);
			s = p;

			num_b = strtol(t, &p, 10);
			t = p;

			if(num_a != num_b)
				return num_a - num_b;
		}
		else
			break;
	}

	return *s - *t;
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
		first_is_dir = check_link_is_dir(one);

	if(second->type == DIRECTORY)
		second_is_dir = true;
	else if(second->type == LINK)
		second_is_dir = check_link_is_dir(two);

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
				retval = compare_file_names(++pfirst, ++psecond);
			else if(pfirst || psecond)
				retval = pfirst ? -1 : 1;
			else
				retval = compare_file_names(first->name, second->name);
			break;

		case SORT_BY_SIZE:
			{
				if(first_is_dir)
					tree_get_data(curr_stats.dirsize_cache, first->name, &first->size);

				if(second_is_dir)
					tree_get_data(curr_stats.dirsize_cache, second->name, &second->size);

				retval = (first->size < second->size) ?
						-1 : (first->size > second->size);
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
		retval = compare_file_names(first->name, second->name);
	else if(view->sort_descending)
		retval = -retval;

	return retval;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
