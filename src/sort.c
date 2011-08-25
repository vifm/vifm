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

#define _GNU_SOURCE

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

#include "sort.h"

static FileView* view;
static int sort_descending;
static int sort_type;

static int sort_dir_list(const void *one, const void *two);

void
sort_view(FileView *v)
{
	int i;

	view = v;
	i = NUM_SORT_OPTIONS;
	while(--i >= 0)
	{
		int j;

		if(view->sort[i] > NUM_SORT_OPTIONS)
			continue;

		sort_descending = (view->sort[i] < 0);
		sort_type = abs(view->sort[i]);

		for(j = 0; j < view->list_rows; j++)
			view->dir_entry[j].list_num = j;

		qsort(view->dir_entry, view->list_rows, sizeof(dir_entry_t), sort_dir_list);
	}
}

static void
strtoupper(char *s)
{
	while(*s != '\0')
	{
		*s = toupper(*s);
		s++;
	}
}

static int
compare_file_names(const char *s, const char *t, int ignore_case)
{
	char s_buf[NAME_MAX];
	char t_buf[NAME_MAX];
	if(ignore_case)
	{
		snprintf(s_buf, sizeof(s_buf), "%s", s);
		strtoupper(s_buf);
		s = s_buf;

		snprintf(t_buf, sizeof(t_buf), "%s", t);
		t = t_buf;
		strtoupper(t_buf);
	}
	if(!cfg.sort_numbers)
		return strcmp(s, t);
	else
		return strverscmp(s, t);
}

static int
sort_dir_list(const void *one, const void *two)
{
	int retval;
	char *pfirst, *psecond;
	dir_entry_t *first = (dir_entry_t *) one;
	dir_entry_t *second = (dir_entry_t *) two;
	int first_is_dir = 0;
	int second_is_dir = 0;

	if(first->type == DIRECTORY)
		first_is_dir = 1;
	else if(first->type == LINK)
		first_is_dir = check_link_is_dir(first->name);

	if(second->type == DIRECTORY)
		second_is_dir = 1;
	else if(second->type == LINK)
		second_is_dir = check_link_is_dir(second->name);

	if(first_is_dir != second_is_dir)
		return first_is_dir ? -1 : 1;

	if(strcmp(first->name, "../") == 0)
		return -1;
	else if(strcmp(second->name, "../") == 0)
		return 1;

	retval = 0;
	switch(sort_type)
	{
		case SORT_BY_NAME:
		case SORT_BY_INAME:
			if(first->name[0] == '.' && second->name[0] != '.')
				retval = -1;
			else if(first->name[0] != '.' && second->name[0] == '.')
				retval = 1;
			else
				retval = compare_file_names(first->name, second->name,
						sort_type == SORT_BY_INAME);
			break;

		case SORT_BY_EXTENSION:
			pfirst  = strrchr(first->name,  '.');
			psecond = strrchr(second->name, '.');

			if(pfirst && psecond)
				retval = compare_file_names(++pfirst, ++psecond, 0);
			else if(pfirst || psecond)
				retval = pfirst ? -1 : 1;
			else
				retval = compare_file_names(first->name, second->name, 0);
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
		retval = first->list_num - second->list_num;
	else if(sort_descending)
		retval = -retval;

	return retval;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
