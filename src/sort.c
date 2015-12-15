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

#include <regex.h> /* regex_t regcomp() regexec() regfree() */

#include <assert.h> /* assert() */
#include <ctype.h>
#include <stdlib.h> /* abs() qsort() */
#include <string.h> /* strcmp() strrchr() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "ui/ui.h"
#include "utils/fsdata.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "filelist.h"
#include "status.h"
#include "types.h"

/* View which is being sorted. */
static FileView* view;
/* Whether the view displays custom file list. */
static int custom_view;
static int sort_descending;
/* Key used to sort entries in current sorting round. */
static SortingKey sort_type;
/* Sorting key specific data. */
static void *sort_data;

static void sort_by_groups(void);
static void sort_by_key(char key, void *data);
static int sort_dir_list(const void *one, const void *two);
TSTATIC int strnumcmp(const char s[], const char t[]);
#if !defined(HAVE_STRVERSCMP_FUNC) || !HAVE_STRVERSCMP_FUNC
static int vercmp(const char s[], const char t[]);
#else
static char * skip_leading_zeros(const char str[]);
#endif
static int compare_entry_names(const dir_entry_t *a, const dir_entry_t *b,
		int ignore_case);
static int compare_full_file_names(const char s[], const char t[],
		int ignore_case);
static int compare_file_names(const char s[], const char t[], int ignore_case);
static int compare_file_sizes(const dir_entry_t *f, int fdir,
		const dir_entry_t *s, int sdir);
static int compare_item_count(const dir_entry_t *f, int fdir,
		const dir_entry_t *s, int sdir);
static int compare_group(const char f[], const char s[], regex_t *regex);

void
sort_view(FileView *v)
{
	int i;

	if(v->sort[0] > SK_LAST)
	{
		/* Completely skip sorting if primary key isn't set. */
		return;
	}

	view = v;
	custom_view = flist_custom_active(v);

	i = SK_COUNT;
	while(--i >= 0)
	{
		const char sorting_key = view->sort[i];

		if(abs(sorting_key) > SK_LAST)
		{
			continue;
		}

		if(sorting_key == SK_BY_GROUPS)
		{
			sort_by_groups();
			continue;
		}

		sort_by_key(sorting_key, NULL);
	}

	if(!ui_view_sort_list_contains(v->sort, SK_BY_DIR))
	{
		sort_by_key(SK_BY_DIR, NULL);
	}
}

/* Sorts view according to sorting groups option. */
static void
sort_by_groups(void)
{
	char **groups = NULL;
	int ngroups = 0;
	int i;

	char *const copy = strdup(view->sort_groups);
	char *group = copy, *state = NULL;
	while((group = split_and_get(group, ',', &state)) != NULL)
	{
		ngroups = add_to_string_array(&groups, ngroups, 1, group);
	}
	free(copy);

	for(i = ngroups - 1; i >= 1; --i)
	{
		regex_t regex;
		(void)regcomp(&regex, groups[i], REG_EXTENDED | REG_ICASE);
		sort_by_key(SK_BY_GROUPS, &regex);
		regfree(&regex);
	}
	if(ngroups != 0)
	{
		sort_by_key(SK_BY_GROUPS, &view->primary_group);
	}

	free_string_array(groups, ngroups);
}

/* Sorts view by the key in a stable way. */
static void
sort_by_key(char key, void *data)
{
	int j;

	sort_descending = (key < 0);
	sort_type = (SortingKey)abs(key);
	sort_data = data;

	for(j = 0; j < view->list_rows; ++j)
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
	/* TODO: refactor this function sort_dir_list(). */

	int retval;
	char *pfirst, *psecond;
	const dir_entry_t *const first = (dir_entry_t *)one;
	const dir_entry_t *const second = (dir_entry_t *)two;
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
			if(custom_view)
			{
				retval = compare_entry_names(first, second, sort_type == SK_BY_INAME);
			}
			else
			{
				retval = compare_full_file_names(first->name, second->name,
						sort_type == SK_BY_INAME);
			}
			break;

		case SK_BY_DIR:
			if(first_is_dir != second_is_dir)
			{
				retval = first_is_dir ? -1 : 1;
			}
			break;

		case SK_BY_TYPE:
			retval = strcmp(get_type_str(first->type), get_type_str(second->type));
			break;

		case SK_BY_FILEEXT:
		case SK_BY_EXTENSION:
			pfirst = strrchr(first->name,  '.');
			psecond = strrchr(second->name, '.');

			if(first_is_dir && second_is_dir && sort_type == SK_BY_FILEEXT)
			{
				retval = compare_file_names(first->name, second->name, 0);
			}
			else if(first_is_dir != second_is_dir && sort_type == SK_BY_FILEEXT)
			{
				retval = first_is_dir ? -1 : 1;
			}
			else if(pfirst && psecond)
			{
				if(pfirst == first->name && psecond != second->name)
				{
					retval = -1;
				}
				else if(pfirst != first->name && psecond == second->name)
				{
					retval = 1;
				}
				else
				{
					retval = compare_file_names(++pfirst, ++psecond, 0);
				}
			}
			else if(pfirst || psecond)
				retval = pfirst ? -1 : 1;
			else
				retval = compare_file_names(first->name, second->name, 0);
			break;

		case SK_BY_SIZE:
			retval = compare_file_sizes(first, first_is_dir, second, second_is_dir);
			break;

		case SK_BY_NITEMS:
			retval = compare_item_count(first, first_is_dir, second, second_is_dir);
			break;

		case SK_BY_GROUPS:
			retval = compare_group(first->name, second->name, sort_data);
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

/* Compares two file sizes.  Returns standard -1, 0, 1 for comparisons. */
static int
compare_file_sizes(const dir_entry_t *f, int fdir, const dir_entry_t *s,
		int sdir)
{
	uint64_t fsize = f->size;
	uint64_t ssize = s->size;

	if(fdir)
	{
		uint64_t size;
		dcache_get_of(f, &size, NULL);
		if(size != DCACHE_UNKNOWN)
		{
			fsize = size;
		}
	}

	if(sdir)
	{
		uint64_t size;
		dcache_get_of(s, &size, NULL);
		if(size != DCACHE_UNKNOWN)
		{
			ssize = size;
		}
	}

	return (fsize < ssize) ? -1 : (fsize > ssize);
}

/* Compares number of items in two directories (taken as zero for files).
 * Returns standard -1, 0, 1 for comparisons. */
static int
compare_item_count(const dir_entry_t *f, int fdir, const dir_entry_t *s,
		int sdir)
{
	/* We don't want to call entry_get_nitems() for files as sorting huge lists
	 * of files can call this function a lot of times, thus even small extra
	 * performance overhead is not desirable. */
	const uint64_t fsize = fdir ? entry_get_nitems(view, f) : 0U;
	const uint64_t ssize = sdir ? entry_get_nitems(view, s) : 0U;
	return (fsize > ssize) ? 1 : (fsize < ssize) ? -1 : 0;
}

/* Compares two file names according to grouping regular expression.  Returns
 * standard -1, 0, 1 for comparisons. */
static int
compare_group(const char f[], const char s[], regex_t *regex)
{
	char fname[NAME_MAX], sname[NAME_MAX];
	regmatch_t fmatch = get_group_match(regex, f);
	regmatch_t smatch = get_group_match(regex, s);

	copy_str(fname, MIN(sizeof(fname), fmatch.rm_eo - fmatch.rm_so + 1U),
			f + fmatch.rm_so);
	copy_str(sname, MIN(sizeof(sname), smatch.rm_eo - smatch.rm_so + 1U),
			s + smatch.rm_so);

	return strcmp(fname, sname);
}

/* Compares names of two file entries.  Returns positive value if a is greater
 * than b, zero if they are equal, otherwise negative value is returned. */
static int
compare_entry_names(const dir_entry_t *a, const dir_entry_t *b, int ignore_case)
{
	char a_short_path[PATH_MAX];
	char b_short_path[PATH_MAX];

	get_short_path_of(view, a, 0, sizeof(a_short_path), a_short_path);
	get_short_path_of(view, b, 0, sizeof(b_short_path), b_short_path);

	return compare_full_file_names(a_short_path, b_short_path, ignore_case);
}

/* Compares two full filenames and assumes that dot character is smaller than
 * any other character.  Returns positive value if s is greater than t, zero if
 * they are equal, otherwise negative value is returned. */
static int
compare_full_file_names(const char s[], const char t[], int ignore_case)
{
	if(s[0] == '.' && t[0] != '.')
	{
		return -1;
	}
	else if(s[0] != '.' && t[0] == '.')
	{
		return 1;
	}
	else
	{
		return compare_file_names(s, t, ignore_case);
	}
}

/* Compares two file names or their parts (e.g. extensions).  Returns positive
 * value if s is greater than t, zero if they are equal, otherwise negative
 * value is returned. */
static int
compare_file_names(const char s[], const char t[], int ignore_case)
{
	const char *s_val = s, *t_val = t;
	char s_buf[NAME_MAX];
	char t_buf[NAME_MAX];
	int result;

	if(ignore_case)
	{
		/* Ignore too small buffer errors by not caring about part that didn't
		 * fit. */
		(void)str_to_lower(s, s_buf, sizeof(s_buf));
		(void)str_to_lower(t, t_buf, sizeof(t_buf));

		s_val = s_buf;
		t_val = t_buf;
	}

	result = cfg.sort_numbers ? strnumcmp(s_val, t_val) : strcmp(s_val, t_val);
	if(result == 0 && ignore_case)
	{
		/* Resort to comparing original names when their normalized versions match
		 * to always solve ties in deterministic way. */
		result = strcmp(s, t);
	}
	return result;
}

SortingKey
get_secondary_key(SortingKey primary_key)
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
		case SK_BY_TYPE:
		case SK_BY_NITEMS:
		case SK_BY_TIME_MODIFIED:
		case SK_BY_TIME_ACCESSED:
		case SK_BY_TIME_CHANGED:
			return primary_key;
		case SK_BY_NAME:
		case SK_BY_INAME:
		case SK_BY_EXTENSION:
		case SK_BY_FILEEXT:
		case SK_BY_SIZE:
		case SK_BY_GROUPS:
		case SK_BY_DIR:
			return SK_BY_SIZE;
	}
	assert(0 && "Unhandled sorting key?");
	return SK_BY_SIZE;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
