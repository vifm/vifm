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

#include "sort.h"

#include <curses.h>

#include <fcntl.h> /* access */
#include <sys/stat.h>

#include <assert.h> /* assert() */
#include <ctype.h>
#include <string.h> /* strrchr */

#include "cfg/config.h"
#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/tree.h"
#include "utils/utils.h"
#include "filelist.h"
#include "status.h"
#include "ui.h"

static FileView* view;
static int sort_descending;
static int sort_type;

static int sort_dir_list(const void *one, const void *two);
TSTATIC int strnumcmp(const char s[], const char t[]);
#if !defined(HAVE_STRVERSCMP_FUNC) || !HAVE_STRVERSCMP_FUNC
static int vercmp(const char s[], const char t[]);
#else
static char * skip_leading_zeros(const char str[]);
#endif

void
sort_view(FileView *v)
{
	int i;

	view = v;
	i = SORT_OPTION_COUNT;
	while(--i >= 0)
	{
		int j;

		if(abs(view->sort[i]) > LAST_SORT_OPTION)
		{
			continue;
		}

		sort_descending = (view->sort[i] < 0);
		sort_type = abs(view->sort[i]);

		for(j = 0; j < view->list_rows; j++)
		{
			view->dir_entry[j].list_num = j;
		}

		qsort(view->dir_entry, view->list_rows, sizeof(dir_entry_t), sort_dir_list);
	}
}

static int
compare_file_names(const char *s, const char *t, int ignore_case)
{
	char s_buf[NAME_MAX];
	char t_buf[NAME_MAX];

	snprintf(s_buf, sizeof(s_buf), "%s", s);
	chosp(s_buf);
	s = s_buf;
	snprintf(t_buf, sizeof(t_buf), "%s", t);
	chosp(t_buf);
	t = t_buf;

	if(ignore_case)
	{
		strtolower(s_buf);
		strtolower(t_buf);
	}

	return cfg.sort_numbers ? strnumcmp(s, t) : strcmp(s, t);
}

/* Compares file names containing numbers correctly. */
TSTATIC int
strnumcmp(const char s[], const char t[])
{
#if !defined(HAVE_STRVERSCMP_FUNC) || !HAVE_STRVERSCMP_FUNC
	return vercmp(s, t);
#else
	const char *new_s = skip_leading_zeros(s);
	const char *new_t = skip_leading_zeros(t);
	return strverscmp(new_s, new_t);
#endif
}

#if !defined(HAVE_STRVERSCMP_FUNC) || !HAVE_STRVERSCMP_FUNC
static int
vercmp(const char s[], const char t[])
{
	while(*s != '\0' && *t != '\0')
	{
		if(isdigit(*s) && isdigit(*t))
		{
			int num_a, num_b;
			const char *os = s, *ot = t;
			char *p;

			num_a = strtol(s, &p, 10);
			s = p;

			num_b = strtol(t, &p, 10);
			t = p;

			if(num_a != num_b)
				return num_a - num_b;
			else if(*os != *ot)
				return *os - *ot;
		}
		else if(*s == *t)
		{
			s++;
			t++;
		}
		else
			break;
	}

	return *s - *t;
}
#else
/* Skips all zeros in front of numbers (correctly handles zero).  Returns str, a
 * pointer to '0' or a pointer to non-zero digit. */
static char *
skip_leading_zeros(const char str[])
{
	while(str[0] == '0' && isdigit(str[1]))
	{
		str++;
	}
	return (char *)str;
}
#endif

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
		first_is_dir = (first->name[strlen(first->name) - 1] == '/');

	if(second->type == DIRECTORY)
		second_is_dir = 1;
	else if(second->type == LINK)
		second_is_dir = (second->name[strlen(second->name) - 1] == '/');

	if(first_is_dir != second_is_dir)
		return first_is_dir ? -1 : 1;

	if(is_parent_dir(first->name))
		return -1;
	else if(is_parent_dir(second->name))
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
#ifndef _WIN32
		case SORT_BY_MODE:
			retval = first->mode - second->mode;
			break;

		case SORT_BY_OWNER_NAME: /* FIXME */
		case SORT_BY_OWNER_ID:
			retval = first->uid - second->uid;
			break;

		case SORT_BY_GROUP_NAME: /* FIXME */
		case SORT_BY_GROUP_ID:
			retval = first->gid - second->gid;
			break;

		case SORT_BY_PERMISSIONS:
			{
				char first_perm[11], second_perm[11];
				get_perm_string(first_perm, sizeof(first_perm), first->mode);
				get_perm_string(second_perm, sizeof(second_perm), second->mode);
				retval = strcmp(first_perm, second_perm);
			}
			break;
#endif

		default:
			assert(0 && "All possible sort options should be handled");
			break;
	}

	if(retval == 0)
		retval = first->list_num - second->list_num;
	else if(sort_descending)
		retval = -retval;

	return retval;
}

int
get_secondary_key(int primary_key)
{
	switch(primary_key)
	{
#ifndef _WIN32
		case SORT_BY_OWNER_NAME:
		case SORT_BY_OWNER_ID:
		case SORT_BY_GROUP_NAME:
		case SORT_BY_GROUP_ID:
		case SORT_BY_MODE:
		case SORT_BY_PERMISSIONS:
#endif
		case SORT_BY_TIME_MODIFIED:
		case SORT_BY_TIME_ACCESSED:
		case SORT_BY_TIME_CHANGED:
			return primary_key;
		case SORT_BY_NAME:
		case SORT_BY_INAME:
		case SORT_BY_EXTENSION:
		case SORT_BY_SIZE:
		default:
			return SORT_BY_SIZE;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
