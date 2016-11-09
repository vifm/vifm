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

#include "flist_pos.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* abs() */
#include <string.h> /* strcmp() */
#include <wctype.h> /* towupper() */

#include "cfg/config.h"
#include "ui/fileview.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/regexp.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "filelist.h"
#include "filtering.h"
#include "types.h"

static void correct_list_pos_down(FileView *view, size_t pos_delta);
static void correct_list_pos_up(FileView *view, size_t pos_delta);
static void move_cursor_out_of_scope(FileView *view, entry_predicate pred);
static const char * get_last_ext(const char name[]);
static int is_mismatched_entry(const dir_entry_t *entry);
static int find_next(const FileView *view, entry_predicate pred);
static int find_prev(const FileView *view, entry_predicate pred);
static int file_can_be_displayed(const char directory[], const char filename[]);

int
find_file_pos_in_list(const FileView *const view, const char file[])
{
	return flist_find_entry(view, file, NULL);
}

int
flist_find_entry(const FileView *view, const char file[], const char dir[])
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		if(dir != NULL && stroscmp(view->dir_entry[i].origin, dir) != 0)
		{
			continue;
		}

		if(stroscmp(view->dir_entry[i].name, file) == 0)
		{
			return i;
		}
	}
	return -1;
}

void
correct_list_pos(FileView *view, ssize_t pos_delta)
{
	if(pos_delta > 0)
	{
		correct_list_pos_down(view, pos_delta);
	}
	else if(pos_delta < 0)
	{
		correct_list_pos_up(view, -pos_delta);
	}
}

int
correct_list_pos_on_scroll_down(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_down(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

int
correct_list_pos_on_scroll_up(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_up(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

/* Tries to move cursor forward by pos_delta positions. */
static void
correct_list_pos_down(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_down(view, pos_delta);
}

/* Tries to move cursor backwards by pos_delta positions. */
static void
correct_list_pos_up(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_up(view, pos_delta);
}

void
flist_set_pos(FileView *view, int pos)
{
	if(pos < 1)
	{
		pos = 0;
	}

	if(pos > view->list_rows - 1)
	{
		pos = view->list_rows - 1;
	}

	if(pos != -1)
	{
		FileView *const other = (view == curr_view) ? other_view : curr_view;

		view->list_pos = pos;
		fview_position_updated(view);

		/* Synchronize cursor with the other pane. */
		if(view->custom.type == CV_DIFF && other->list_pos != pos)
		{
			flist_set_pos(other, pos);
		}
	}
}

void
flist_ensure_pos_is_valid(FileView *view)
{
	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}
}

void
move_cursor_out_of(FileView *view, FileListScope scope)
{
	/* XXX: this functionality might be unnecessary now that we have directory
	 *      merging. */
	switch(scope)
	{
		case FLS_SELECTION:
			move_cursor_out_of_scope(view, &is_entry_selected);
			return;
		case FLS_MARKING:
			move_cursor_out_of_scope(view, &is_entry_marked);
			return;
	}
	assert(0 && "Unhandled file list scope type");
}

/* Ensures that cursor is moved outside of entries that satisfy the predicate if
 * that's possible. */
static void
move_cursor_out_of_scope(FileView *view, entry_predicate pred)
{
	/* TODO: if we reach bottom of the list and predicate holds try scanning to
	 * the top. */
	int i = view->list_pos;
	while(i < view->list_rows - 1 && pred(&view->dir_entry[i]))
	{
		++i;
	}
	view->list_pos = i;
}

int
at_first_line(const FileView *view)
{
	return view->list_pos/view->column_count == 0;
}

int
at_last_line(const FileView *view)
{
	const size_t col_count = view->column_count;
	return view->list_pos/col_count == (view->list_rows - 1)/col_count;
}

int
at_first_column(const FileView *view)
{
	return view->list_pos%view->column_count == 0;
}

int
at_last_column(const FileView *view)
{
	return view->list_pos%view->column_count == view->column_count - 1;
}

void
go_to_start_of_line(FileView *view)
{
	view->list_pos = get_start_of_line(view);
}

int
get_start_of_line(const FileView *view)
{
	const int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	return ROUND_DOWN(pos, view->column_count);
}

int
get_end_of_line(const FileView *view)
{
	int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	pos += (view->column_count - 1) - pos%view->column_count;
	return MIN(pos, view->list_rows - 1);
}

int
flist_find_group(const FileView *view, int next)
{
	/* TODO: refactor/simplify this function (flist_find_group()). */

	const int correction = next ? -1 : 0;
	const int lb = correction;
	const int ub = view->list_rows + correction;
	const int inc = next ? +1 : -1;

	int pos = view->list_pos;
	dir_entry_t *pentry = &view->dir_entry[pos];
	const char *ext = get_last_ext(pentry->name);
	size_t char_width = utf8_chrw(pentry->name);
	wchar_t ch = towupper(get_first_wchar(pentry->name));
	const SortingKey sorting_key =
		flist_custom_active(view) && cv_compare(view->custom.type)
		? SK_BY_ID
		: abs(view->sort[0]);
	const int is_dir = is_directory_entry(pentry);
	const char *const type_str = get_type_str(pentry->type);
	regmatch_t pmatch = { .rm_so = 0, .rm_eo = 0 };
#ifndef _WIN32
	char perms[16];
	get_perm_string(perms, sizeof(perms), pentry->mode);
#endif
	if(sorting_key == SK_BY_GROUPS)
	{
		pmatch = get_group_match(&view->primary_group, pentry->name);
	}
	while(pos > lb && pos < ub)
	{
		dir_entry_t *nentry;
		pos += inc;
		nentry = &view->dir_entry[pos];
		switch(sorting_key)
		{
			case SK_BY_FILEEXT:
				if(is_directory_entry(nentry))
				{
					if(strncmp(pentry->name, nentry->name, char_width) != 0)
					{
						return pos;
					}
				}
				if(strcmp(get_last_ext(nentry->name), ext) != 0)
				{
					return pos;
				}
				break;
			case SK_BY_EXTENSION:
				if(strcmp(get_last_ext(nentry->name), ext) != 0)
					return pos;
				break;
			case SK_BY_GROUPS:
				{
					regmatch_t nmatch = get_group_match(&view->primary_group,
							nentry->name);

					if(pmatch.rm_eo - pmatch.rm_so != nmatch.rm_eo - nmatch.rm_so ||
							(pmatch.rm_eo != pmatch.rm_so &&
							 strncmp(pentry->name + pmatch.rm_so, nentry->name + nmatch.rm_so,
								 pmatch.rm_eo - pmatch.rm_so + 1U) != 0))
						return pos;
				}
				break;
			case SK_BY_TARGET:
				if((nentry->type == FT_LINK) != (pentry->type == FT_LINK))
				{
					/* One of the entries is not a link. */
					return pos;
				}
				if(nentry->type == FT_LINK)
				{
					/* Both entries are symbolic links. */
					char full_path[PATH_MAX];
					char nlink[PATH_MAX], plink[PATH_MAX];

					get_full_path_of(nentry, sizeof(full_path), full_path);
					if(get_link_target(full_path, nlink, sizeof(nlink)) != 0)
					{
						return pos;
					}
					get_full_path_of(pentry, sizeof(full_path), full_path);
					if(get_link_target(full_path, plink, sizeof(plink)) != 0)
					{
						return pos;
					}

					if(stroscmp(nlink, plink) != 0)
					{
						return pos;
					}
				}
				break;
			case SK_BY_NAME:
				if(strncmp(pentry->name, nentry->name, char_width) != 0)
					return pos;
				break;
			case SK_BY_INAME:
				if((wchar_t)towupper(get_first_wchar(nentry->name)) != ch)
					return pos;
				break;
			case SK_BY_SIZE:
				if(nentry->size != pentry->size)
					return pos;
				break;
			case SK_BY_NITEMS:
				if(entry_get_nitems(view, nentry) != entry_get_nitems(view, pentry))
					return pos;
				break;
			case SK_BY_TIME_ACCESSED:
				if(nentry->atime != pentry->atime)
					return pos;
				break;
			case SK_BY_TIME_CHANGED:
				if(nentry->ctime != pentry->ctime)
					return pos;
				break;
			case SK_BY_TIME_MODIFIED:
				if(nentry->mtime != pentry->mtime)
					return pos;
				break;
			case SK_BY_DIR:
				if(is_dir != is_directory_entry(nentry))
				{
					return pos;
				}
				break;
			case SK_BY_TYPE:
				if(get_type_str(nentry->type) != type_str)
				{
					return pos;
				}
				break;
#ifndef _WIN32
			case SK_BY_GROUP_NAME:
			case SK_BY_GROUP_ID:
				if(nentry->gid != pentry->gid)
					return pos;
				break;
			case SK_BY_OWNER_NAME:
			case SK_BY_OWNER_ID:
				if(nentry->uid != pentry->uid)
					return pos;
				break;
			case SK_BY_MODE:
				if(nentry->mode != pentry->mode)
					return pos;
				break;
			case SK_BY_PERMISSIONS:
				{
					char nperms[16];
					get_perm_string(nperms, sizeof(nperms), nentry->mode);
					if(strcmp(nperms, perms) != 0)
					{
						return pos;
					}
					break;
				}
			case SK_BY_NLINKS:
				if(nentry->nlinks != pentry->nlinks)
				{
					return pos;
				}
				break;
#endif
		}
		/* Id sorting is builtin only and is defined outside SortingKey
		 * enumeration. */
		if((int)sorting_key == SK_BY_ID)
		{
			if(nentry->id != pentry->id)
			{
				return pos;
			}
		}
	}
	return pos;
}

/* Finds pointer to the beginning of the last extension of the file name.
 * Returns the pointer, which might point to the NUL byte if there are no
 * extensions. */
static const char *
get_last_ext(const char name[])
{
	const char *const ext = strrchr(name, '.');
	return (ext == NULL) ? (name + strlen(name)) : (ext + 1);
}

int
flist_find_dir_group(const FileView *view, int next)
{
	const int correction = next ? -1 : 0;
	const int lb = correction;
	const int ub = view->list_rows + correction;
	const int inc = next ? +1 : -1;

	int pos = curr_view->list_pos;
	dir_entry_t *pentry = &curr_view->dir_entry[pos];
	const int is_dir = is_directory_entry(pentry);
	while(pos > lb && pos < ub)
	{
		dir_entry_t *nentry;
		pos += inc;
		nentry = &curr_view->dir_entry[pos];
		if(is_dir != is_directory_entry(nentry))
		{
			break;
		}
	}
	return pos;
}

int
flist_first_sibling(const FileView *view)
{
	const int parent = view->list_pos - view->dir_entry[view->list_pos].child_pos;
	return (parent == view->list_pos ? 0 : parent + 1);
}

int
flist_last_sibling(const FileView *view)
{
	int pos = view->list_pos - view->dir_entry[view->list_pos].child_pos;
	if(pos == view->list_pos)
	{
		/* For top-level entry, find the last top-level entry. */
		pos = view->list_rows - 1;
		while(view->dir_entry[pos].child_pos != 0)
		{
			pos -= view->dir_entry[pos].child_pos;
		}
	}
	else
	{
		/* For non-top-level entry, go to last tree item and go up until our
		 * child. */
		const int parent = pos;
		pos = parent + view->dir_entry[parent].child_count;
		while(pos - view->dir_entry[pos].child_pos != parent)
		{
			pos -= view->dir_entry[pos].child_pos;
		}
	}
	return pos;
}

int
flist_next_dir_sibling(const FileView *view)
{
	int pos = view->list_pos;
	const int parent = view->dir_entry[pos].child_pos == 0
	                 ? -1
	                 : pos - view->dir_entry[pos].child_pos;
	const int past_end = parent == -1
	                   ? view->list_rows
	                   : parent + view->dir_entry[parent].child_count;
	pos += view->dir_entry[pos].child_count + 1;
	while(pos < past_end)
	{
		dir_entry_t *const e = &view->dir_entry[pos];
		if(is_directory_entry(e))
		{
			break;
		}
		/* Skip over whole sub-tree. */
		pos += e->child_count + 1;
	}
	return (pos < past_end ? pos : view->list_pos);
}

int
flist_prev_dir_sibling(const FileView *view)
{
	int pos = view->list_pos;
	/* Determine original parent (-1 for top-most entry). */
	const int parent = view->dir_entry[pos].child_pos == 0
	                 ? -1
	                 : pos - view->dir_entry[pos].child_pos;
	--pos;
	while(pos > parent)
	{
		dir_entry_t *const e = &view->dir_entry[pos];
		const int p = (e->child_pos == 0) ? -1 : (pos - e->child_pos);
		/* If we find ourselves deeper than originally, just go up one level. */
		if(p != parent)
		{
			pos = p;
			continue;
		}

		/* We're looking for directories. */
		if(is_directory_entry(e))
		{
			break;
		}
		/* We're on a file on the same level. */
		--pos;
	}
	return (pos > parent ? pos : view->list_pos);
}

int
flist_next_dir(const FileView *view)
{
	return find_next(view, &is_directory_entry);
}

int
flist_prev_dir(const FileView *view)
{
	return find_prev(view, &is_directory_entry);
}

int
flist_next_selected(const FileView *view)
{
	return find_next(view, &is_entry_selected);
}

int
flist_prev_selected(const FileView *view)
{
	return find_prev(view, &is_entry_selected);
}

int
flist_next_mismatch(const FileView *view)
{
	return (view->custom.type == CV_DIFF)
	     ? find_next(view, &is_mismatched_entry)
	     : view->list_pos;
}

int
flist_prev_mismatch(const FileView *view)
{
	return (view->custom.type == CV_DIFF)
	     ? find_prev(view, &is_mismatched_entry)
	     : view->list_pos;
}

/* Checks whether entry corresponds to comparison mismatch.  Returns non-zero if
 * so, otherwise zero is returned. */
static int
is_mismatched_entry(const dir_entry_t *entry)
{
	/* To avoid passing view pointer here, we exploit the fact that entry_to_pos()
	 * checks whether it's argument belongs to the given view. */
	int pos = entry_to_pos(&lwin, entry);
	FileView *other = &rwin;
	if(pos == -1)
	{
		pos = entry_to_pos(&rwin, entry);
		other = &lwin;
	}

	return (other->dir_entry[pos].id != entry->id);
}

/* Finds position of the next entry matching the predicate.  Returns new
 * position which isn't changed if no next directory is found. */
static int
find_next(const FileView *view, entry_predicate pred)
{
	int pos = view->list_pos;
	while(++pos < view->list_rows)
	{
		if(pred(&view->dir_entry[pos]))
		{
			break;
		}
	}
	return (pos == view->list_rows ? view->list_pos : pos);
}

/* Finds position of the previous entry matching the predicate.  Returns new
 * position which isn't changed if no previous directory is found. */
static int
find_prev(const FileView *view, entry_predicate pred)
{
	int pos = view->list_pos;
	while(--pos >= 0)
	{
		if(pred(&view->dir_entry[pos]))
		{
			break;
		}
	}
	return (pos < 0 ? view->list_pos : pos);
}

int
ensure_file_is_selected(FileView *view, const char name[])
{
	int file_pos;
	char nm[NAME_MAX];

	/* Don't reset filters to find "file with empty name". */
	if(name[0] == '\0')
	{
		return 0;
	}

	/* This is for compatibility with paths loaded from vifminfo that have
	 * trailing slash. */
	copy_str(nm, sizeof(nm), name);
	chosp(nm);

	file_pos = find_file_pos_in_list(view, nm);
	if(file_pos < 0 && file_can_be_displayed(view->curr_dir, nm))
	{
		if(nm[0] == '.')
		{
			set_dot_files_visible(view, 1);
			file_pos = find_file_pos_in_list(view, nm);
		}

		if(file_pos < 0)
		{
			remove_filename_filter(view);

			/* remove_filename_filter() postpones list of files reloading. */
			populate_dir_list(view, 1);

			file_pos = find_file_pos_in_list(view, nm);
		}
	}

	flist_set_pos(view, (file_pos < 0) ? 0 : file_pos);
	return file_pos >= 0;
}

/* Checks if file specified can be displayed. Used to filter some files, that
 * are hidden intentionally.  Returns non-zero if file can be made visible. */
static int
file_can_be_displayed(const char directory[], const char filename[])
{
	if(is_parent_dir(filename))
	{
		return cfg_parent_dir_is_visible(is_root_dir(directory));
	}
	return path_exists_at(directory, filename, DEREF);
}

int
flist_find_by_ch(const FileView *view, int ch, int backward, int wrap)
{
	int x;
	const int upcase = (cfg.case_override & CO_GOTO_FILE)
	                 ? (cfg.case_ignore & CO_GOTO_FILE)
	                 : (cfg.ignore_case && !(cfg.smart_case && iswupper(ch)));

	if(upcase)
	{
		ch = towupper(ch);
	}

	x = view->list_pos;
	do
	{
		if(backward)
		{
			x--;
			if(x < 0)
			{
				if(wrap)
					x = view->list_rows - 1;
				else
					return -1;
			}
		}
		else
		{
			x++;
			if(x > view->list_rows - 1)
			{
				if(wrap)
					x = 0;
				else
					return -1;
			}
		}

		if(ch > 255)
		{
			wchar_t wc = get_first_wchar(view->dir_entry[x].name);
			if(upcase)
				wc = towupper(wc);
			if(wc == ch)
				break;
		}
		else
		{
			int c = view->dir_entry[x].name[0];
			if(upcase)
				c = towupper(c);
			if(c == ch)
				break;
		}
	}
	while(x != view->list_pos);

	return x;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
