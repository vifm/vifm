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

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stdlib.h> /* abs() qsort() */
#include <string.h> /* strrchr() */

#include "cfg/config.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/tree.h"
#include "utils/utils.h"
#include "filelist.h"
#include "status.h"
#include "types.h"
#include "ui.h"

static FileView* view;
static int sort_descending;
static int sort_type;

static void sort_by_key(char key);
static int sort_dir_list(const void *one, const void *two);
TSTATIC int strnumcmp(const char s[], const char t[]);
#if !defined(HAVE_STRVERSCMP_FUNC) || !HAVE_STRVERSCMP_FUNC
static int vercmp(const char s[], const char t[]);
#else
static char * skip_leading_zeros(const char str[]);
#endif
static int compare_file_names(const char s[], const char t[],
		int ignore_case);

void
sort_view(FileView *v)
{
	int i;

	view = v;
	i = SK_COUNT;
	while(--i >= 0)
	{
		const char sorting_key = view->sort[i];

		if(abs(sorting_key) > SK_LAST)
		{
			continue;
		}

		sort_by_key(sorting_key);
	}

	if(!ui_view_sort_list_contains(v->sort, SK_BY_TYPE))
	{
		sort_by_key(SK_BY_TYPE);
	}
}

/* Sorts view by the key in a stable way. */
static void
sort_by_key(char key)
{
	int j;

	sort_descending = (key < 0);
	sort_type = abs(key);

	for(j = 0; j < view->list_rows; j++)
	{
		view->dir_entry[j].list_num = j;
	}

	qsort(view->dir_entry, view->list_rows, sizeof(dir_entry_t), sort_dir_list);
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
	dir_entry_t *const first = (dir_entry_t *)one;
	dir_entry_t *const second = (dir_entry_t *)two;
	int first_is_dir;
	int second_is_dir;

	if(is_parent_dir(first->name))
	{
		return -1;
	}
	else if(is_parent_dir(second->name))
	{
		return 1;
	}

	first_is_dir = is_directory_entry(first);
	second_is_dir = is_directory_entry(second);

	retval = 0;
	switch(sort_type)
	{
		case SK_BY_NAME:
		case SK_BY_INAME:
			if(first->name[0] == '.' && second->name[0] != '.')
				retval = -1;
			else if(first->name[0] != '.' && second->name[0] == '.')
				retval = 1;
			else
				retval = compare_file_names(first->name, second->name,
						sort_type == SK_BY_INAME);
			break;

		case SK_BY_TYPE:
			if(first_is_dir != second_is_dir)
			{
				retval = first_is_dir ? -1 : 1;
			}
			break;

		case SK_BY_EXTENSION:
			pfirst  = strrchr(first->name,  '.');
			psecond = strrchr(second->name, '.');

			if(pfirst && psecond)
				retval = compare_file_names(++pfirst, ++psecond, 0);
			else if(pfirst || psecond)
				retval = pfirst ? -1 : 1;
			else
				retval = compare_file_names(first->name, second->name, 0);
			break;

		case SK_BY_SIZE:
			{
				if(first_is_dir)
					tree_get_data(curr_stats.dirsize_cache, first->name, &first->size);

				if(second_is_dir)
					tree_get_data(curr_stats.dirsize_cache, second->name, &second->size);

				retval = (first->size < second->size) ?
						-1 : (first->size > second->size);
			}
			break;

		case SK_BY_TIME_MODIFIED:
			retval = first->mtime - second->mtime;
			break;

		case SK_BY_TIME_ACCESSED:
			retval = first->atime - second->atime;
			break;

		case SK_BY_TIME_CHANGED:
			retval = first->ctime - second->ctime;
			break;
#ifndef _WIN32
		case SK_BY_MODE:
			retval = first->mode - second->mode;
			break;

		case SK_BY_OWNER_NAME: /* FIXME */
		case SK_BY_OWNER_ID:
			retval = first->uid - second->uid;
			break;

		case SK_BY_GROUP_NAME: /* FIXME */
		case SK_BY_GROUP_ID:
			retval = first->gid - second->gid;
			break;

		case SK_BY_PERMISSIONS:
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
	{
		retval = first->list_num - second->list_num;
	}
	else if(sort_descending)
	{
		retval = -retval;
	}

	return retval;
}

/* Compares two filenames.  Returns positive value if s greater than t, zero if
 * they are equal, otherwise negative value is returned. */
static int
compare_file_names(const char s[], const char t[], int ignore_case)
{
	char s_buf[NAME_MAX];
	char t_buf[NAME_MAX];

	if(ignore_case)
	{
		copy_str(s_buf, sizeof(s_buf), s);
		s = s_buf;

		copy_str(t_buf, sizeof(t_buf), t);
		t = t_buf;

		str_to_lower(s_buf);
		str_to_lower(t_buf);
	}

	return cfg.sort_numbers ? strnumcmp(s, t) : strcmp(s, t);
}

int
get_secondary_key(int primary_key)
{
	switch(primary_key)
	{
#ifndef _WIN32
		case SK_BY_OWNER_NAME:
		case SK_BY_OWNER_ID:
		case SK_BY_GROUP_NAME:
		case SK_BY_GROUP_ID:
		case SK_BY_MODE:
		case SK_BY_PERMISSIONS:
#endif
		case SK_BY_TIME_MODIFIED:
		case SK_BY_TIME_ACCESSED:
		case SK_BY_TIME_CHANGED:
			return primary_key;
		case SK_BY_NAME:
		case SK_BY_INAME:
		case SK_BY_EXTENSION:
		case SK_BY_SIZE:
		default:
			return SK_BY_SIZE;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
