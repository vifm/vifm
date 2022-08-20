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

static int get_curr_col(const view_t *view);
static int get_curr_line(const view_t *view);
static int get_max_col(const view_t *view);
static int get_max_line(const view_t *view);
static int get_column_top_pos(const view_t *view);
static int get_column_bottom_pos(const view_t *view);
static const char * get_last_ext(const char name[]);
static int is_mismatched_entry(const dir_entry_t *entry);
static int find_next(const view_t *view, entry_predicate pred);
static int find_prev(const view_t *view, entry_predicate pred);
static int file_can_be_displayed(const char directory[], const char filename[]);

int
fpos_find_by_name(const view_t *view, const char name[])
{
	return fpos_find_entry(view, name, NULL);
}

int
fpos_find_entry(const view_t *view, const char name[], const char dir[])
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		if(dir != NULL && stroscmp(view->dir_entry[i].origin, dir) != 0)
		{
			continue;
		}

		if(stroscmp(view->dir_entry[i].name, name) == 0)
		{
			return i;
		}
	}
	return -1;
}

int
fpos_scroll_down(view_t *view, int lines_count)
{
	if(!fpos_are_all_files_visible(view))
	{
		view->list_pos =
			get_corrected_list_pos_down(view, lines_count*view->run_size);
		return 1;
	}
	return 0;
}

int
fpos_scroll_up(view_t *view, int lines_count)
{
	if(!fpos_are_all_files_visible(view))
	{
		view->list_pos =
			get_corrected_list_pos_up(view, lines_count*view->run_size);
		return 1;
	}
	return 0;
}

void
fpos_set_pos(view_t *view, int pos)
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
		view_t *const other = (view == curr_view) ? other_view : curr_view;

		view->list_pos = pos;
		fview_position_updated(view);

		/* Synchronize cursor with the other pane. */
		if(view->custom.type == CV_DIFF && other->list_pos != pos)
		{
			fpos_set_pos(other, pos);
		}
	}
}

void
fpos_ensure_valid_pos(view_t *view)
{
	if(view->list_pos < 0)
	{
		view->list_pos = 0;
	}

	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}
}

int
fpos_get_col(const view_t *view, int pos)
{
	return (fview_is_transposed(view) ? pos/view->run_size : pos%view->run_size);
}

int
fpos_get_line(const view_t *view, int pos)
{
	return (fview_is_transposed(view) ? pos%view->run_size : pos/view->run_size);
}

int
fpos_can_move_left(const view_t *view)
{
	return (view->list_pos > 0);
}

int
fpos_can_move_right(const view_t *view)
{
	return (view->list_pos < view->list_rows - 1);
}

int
fpos_can_move_up(const view_t *view)
{
	return fview_is_transposed(view) ? view->list_pos > 0
	                                 : view->list_pos >= view->run_size;
}

int
fpos_can_move_down(const view_t *view)
{
	return fview_is_transposed(view) ? view->list_pos < view->list_rows - 1
	                                 : get_curr_line(view) < get_max_line(view);
}

int
fpos_at_first_col(const view_t *view)
{
	return (get_curr_col(view) == 0);
}

int
fpos_at_last_col(const view_t *view)
{
	return (get_curr_col(view) == get_max_col(view));
}

/* Retrieves column number of cursor.  Returns the number. */
static int
get_curr_col(const view_t *view)
{
	return fpos_get_col(view, view->list_pos);
}

/* Retrieves line number of cursor.  Returns the number. */
static int
get_curr_line(const view_t *view)
{
	return fpos_get_line(view, view->list_pos);
}

/* Retrieves maximum column number.  Returns the number. */
static int
get_max_col(const view_t *view)
{
	return fview_is_transposed(view) ? (view->list_rows - 1)/view->run_size
	                                 : (view->run_size - 1);
}

/* Retrieves maximum line number.  Returns the number. */
static int
get_max_line(const view_t *view)
{
	return fview_is_transposed(view) ? (view->run_size - 1)
	                                 : (view->list_rows - 1)/view->run_size;
}

int
fpos_line_start(const view_t *view)
{
	return fview_is_transposed(view) ? view->list_pos%view->run_size
	                                 : ROUND_DOWN(view->list_pos, view->run_size);
}

int
fpos_line_end(const view_t *view)
{
	if(fview_is_transposed(view))
	{
		const int last_top_pos = ROUND_DOWN(view->list_rows - 1, view->run_size);
		const int pos = last_top_pos + view->list_pos%view->run_size;
		return (pos < view->list_rows ? pos : pos - view->run_size);
	}

	return MIN(view->list_rows - 1, fpos_line_start(view) + view->run_size - 1);
}

int
fpos_get_hor_step(const struct view_t *view)
{
	return (fview_is_transposed(view) ? view->run_size : 1);
}

int
fpos_get_ver_step(const struct view_t *view)
{
	return (fview_is_transposed(view) ? 1 : view->run_size);
}

int
fpos_has_hidden_top(const view_t *view)
{
	return (fview_is_transposed(view) ? 0 : can_scroll_up(view));
}

int
fpos_has_hidden_bottom(const view_t *view)
{
	return (fview_is_transposed(view) ? 0 : can_scroll_down(view));
}

int
fpos_get_top_pos(const view_t *view)
{
	return get_column_top_pos(view)
	     + (can_scroll_up(view) ? fpos_get_offset(view) : 0);
}

int
fpos_get_middle_pos(const view_t *view)
{
	const int top_pos = get_column_top_pos(view);
	const int bottom_pos = get_column_bottom_pos(view);
	const int v = (fview_is_transposed(view) ? 1 : view->run_size);
	return top_pos + (DIV_ROUND_UP(bottom_pos - top_pos, v)/2)*v;
}

int
fpos_get_bottom_pos(const view_t *view)
{
	return get_column_bottom_pos(view)
	     - (can_scroll_down(view) ? fpos_get_offset(view) : 0);
}

int
fpos_get_offset(const view_t *view)
{
	int val;

	if(fview_is_transposed(view))
	{
		/* Scroll offset doesn't make much sense for transposed table. */
		return 0;
	}

	val = MIN(DIV_ROUND_UP(view->window_rows - 1, 2), MAX(cfg.scroll_off, 0));
	return val*view->column_count;
}

int
fpos_are_all_files_visible(const view_t *view)
{
	return view->list_rows <= view->window_cells;
}

int
fpos_get_last_visible_cell(const view_t *view)
{
	return view->top_line + view->window_cells - 1;
}

int
fpos_half_scroll(view_t *view, int down)
{
	int new_pos;

	int offset = MAX(view->window_cells/2, view->run_size);
	offset = ROUND_DOWN(offset, curr_view->run_size);

	if(down)
	{
		new_pos = get_corrected_list_pos_down(view, offset);
		new_pos = MAX(new_pos, view->list_pos + offset);

		if(new_pos >= view->list_rows)
		{
			new_pos -= view->column_count*
				DIV_ROUND_UP(new_pos - (view->list_rows - 1), view->column_count);
		}
	}
	else
	{
		new_pos = get_corrected_list_pos_up(view, offset);
		new_pos = MIN(new_pos, view->list_pos - offset);

		if(new_pos < 0)
		{
			new_pos += view->column_count*DIV_ROUND_UP(-new_pos, view->column_count);
		}
	}

	scroll_by_files(view, new_pos - view->list_pos);
	return new_pos;
}

/* Retrieves position of a file at the top of visible part of current column.
 * Returns the position. */
static int
get_column_top_pos(const view_t *view)
{
	const int column_correction = fview_is_transposed(view)
	  ? ROUND_DOWN(view->list_pos - view->top_line, view->run_size)
	  : view->list_pos%view->run_size;
	return view->top_line + column_correction;
}

/* Retrieves position of a file at the bottom of visible part of current column.
 * Returns the position. */
static int
get_column_bottom_pos(const view_t *view)
{
	if(fview_is_transposed(view))
	{
		const int top_pos = get_column_top_pos(view);
		const int last = view->list_rows - 1;
		return MIN(top_pos + view->window_rows - 1, last);
	}
	else
	{
		const int last_top_pos =
			ROUND_DOWN(MIN(fpos_get_last_visible_cell(view), view->list_rows - 1),
					view->run_size);
		const int pos = last_top_pos + view->list_pos%view->run_size;
		return (pos < view->list_rows ? pos : pos - view->run_size);
	}
}

int
fpos_find_group(const view_t *view, int next)
{
	/* TODO: refactor/simplify this function (fpos_find_group()). */

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
	const int is_dir = fentry_is_dir(pentry);
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
				if(fentry_is_dir(nentry))
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
					char full_path[PATH_MAX + 1];
					char nlink[PATH_MAX + 1], plink[PATH_MAX + 1];

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
				if(fentry_get_nitems(view, nentry) != fentry_get_nitems(view, pentry))
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
				if(is_dir != fentry_is_dir(nentry))
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
			case SK_BY_INODE:
				if(nentry->inode != pentry->inode)
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
fpos_find_dir_group(const view_t *view, int next)
{
	const int correction = next ? -1 : 0;
	const int lb = correction;
	const int ub = view->list_rows + correction;
	const int inc = next ? +1 : -1;

	int pos = curr_view->list_pos;
	dir_entry_t *pentry = &curr_view->dir_entry[pos];
	const int is_dir = fentry_is_dir(pentry);
	while(pos > lb && pos < ub)
	{
		dir_entry_t *nentry;
		pos += inc;
		nentry = &curr_view->dir_entry[pos];
		if(is_dir != fentry_is_dir(nentry))
		{
			break;
		}
	}
	return pos;
}

int
fpos_first_sibling(const view_t *view)
{
	const int parent = view->list_pos - view->dir_entry[view->list_pos].child_pos;
	return (parent == view->list_pos ? 0 : parent + 1);
}

int
fpos_last_sibling(const view_t *view)
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
fpos_next_dir_sibling(const view_t *view)
{
	int pos = view->list_pos;
	const int parent = view->dir_entry[pos].child_pos == 0
	                 ? -1
	                 : pos - view->dir_entry[pos].child_pos;
	const int past_end = parent == -1
	                   ? view->list_rows
	                   : parent + 1 + view->dir_entry[parent].child_count;
	pos += view->dir_entry[pos].child_count + 1;
	while(pos < past_end)
	{
		dir_entry_t *const e = &view->dir_entry[pos];
		if(fentry_is_dir(e))
		{
			break;
		}
		/* Skip over whole sub-tree. */
		pos += e->child_count + 1;
	}
	return (pos < past_end ? pos : view->list_pos);
}

int
fpos_prev_dir_sibling(const view_t *view)
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
		if(fentry_is_dir(e))
		{
			break;
		}
		/* We're on a file on the same level. */
		--pos;
	}
	return (pos > parent ? pos : view->list_pos);
}

int
fpos_next_dir(const view_t *view)
{
	return find_next(view, &fentry_is_dir);
}

int
fpos_prev_dir(const view_t *view)
{
	return find_prev(view, &fentry_is_dir);
}

int
fpos_next_selected(const view_t *view)
{
	return find_next(view, &is_entry_selected);
}

int
fpos_prev_selected(const view_t *view)
{
	return find_prev(view, &is_entry_selected);
}

int
fpos_next_mismatch(const view_t *view)
{
	return (view->custom.type == CV_DIFF)
	     ? find_next(view, &is_mismatched_entry)
	     : view->list_pos;
}

int
fpos_prev_mismatch(const view_t *view)
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
	 * checks whether its argument belongs to the given view. */
	int pos = entry_to_pos(&lwin, entry);
	view_t *other = &rwin;
	if(pos == -1)
	{
		pos = entry_to_pos(&rwin, entry);
		other = &lwin;
	}

	return other->dir_entry[pos].id != entry->id
	    || fentry_is_fake(entry)
	    || fentry_is_fake(&other->dir_entry[pos]);
}

/* Finds position of the next entry matching the predicate.  Returns new
 * position which isn't changed if no next directory is found. */
static int
find_next(const view_t *view, entry_predicate pred)
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
find_prev(const view_t *view, entry_predicate pred)
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
fpos_ensure_selected(view_t *view, const char name[])
{
	int file_pos;
	char nm[NAME_MAX + 1];

	/* Don't reset filters to find "file with empty name". */
	if(name[0] == '\0')
	{
		return 0;
	}

	/* This is for compatibility with paths loaded from vifminfo that have
	 * trailing slash. */
	copy_str(nm, sizeof(nm), name);
	chosp(nm);

	file_pos = fpos_find_by_name(view, nm);
	if(file_pos < 0 && file_can_be_displayed(view->curr_dir, nm))
	{
		if(nm[0] == '.')
		{
			dot_filter_set(view, 1);
			file_pos = fpos_find_by_name(view, nm);
		}

		if(file_pos < 0)
		{
			name_filters_remove(view);

			/* name_filters_remove() postpones reloading of list files. */
			(void)populate_dir_list(view, 1);

			file_pos = fpos_find_by_name(view, nm);
		}
	}

	fpos_set_pos(view, (file_pos < 0) ? 0 : file_pos);
	return file_pos >= 0;
}

/* Checks if file specified can be displayed.  Used to filter some files, that
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
fpos_find_by_ch(const view_t *view, int ch, int backward, int wrap)
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
			if(wc == (wchar_t)ch)
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
