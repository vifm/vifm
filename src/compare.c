/* vifm
 * Copyright (C) 2016 xaizek.
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

#include "compare.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdint.h> /* INTPTR_MAX INT64_MAX */
#include <stdio.h> /* FILE fclose() feof() fopen() fread() */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memcmp() */

#include "compat/fs_limits.h"
#include "compat/os.h"
#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/dynarray.h"
#include "utils/fs.h"
#include "utils/fsdata.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/trie.h"
#include "utils/utils.h"
#include "filelist.h"
#include "filtering.h"
#include "fops_cpmv.h"
#include "fops_misc.h"
#include "running.h"

/* This is the only unit that uses xxhash, so import it directly here. */
#define XXH_PRIVATE_API
#include "utils/xxhash.h"

/* Amount of data to read at once. */
#define BLOCK_SIZE (32*1024)

/* Amount of data to hash for coarse comparison. */
#define PREFIX_SIZE (4*1024)

/* Entry in singly-bounded list of files that have matched fingerprints. */
typedef struct compare_record_t
{
	char *path;                    /* Full path to file with sample content. */
	int id;                        /* Chosen id. */
	struct compare_record_t *next; /* Next entry in the list. */
}
compare_record_t;

static void make_unique_lists(entries_t curr, entries_t other);
static void leave_only_dups(entries_t *curr, entries_t *other);
static int is_not_duplicate(view_t *view, const dir_entry_t *entry, void *arg);
static void fill_side_by_side(entries_t curr, entries_t other, int group_paths);
static int id_sorter(const void *first, const void *second);
static void put_or_free(view_t *view, dir_entry_t *entry, int id, int take);
static entries_t make_diff_list(trie_t *trie, view_t *view, int *next_id,
		CompareType ct, int skip_empty, int dups_only);
static void list_view_entries(const view_t *view, strlist_t *list);
static int append_valid_nodes(const char name[], int valid,
		const void *parent_data, void *data, void *arg);
static void list_files_recursively(const view_t *view, const char path[],
		int skip_dot_files, strlist_t *list);
static char * get_file_fingerprint(const char path[], const dir_entry_t *entry,
		CompareType ct);
static char * get_contents_fingerprint(const char path[],
		const dir_entry_t *entry);
static int get_file_id(trie_t *trie, const char path[],
		const char fingerprint[], int *id, CompareType ct);
static int files_are_identical(const char a[], const char b[]);
static void put_file_id(trie_t *trie, const char path[],
		const char fingerprint[], int id, CompareType ct);
static void free_compare_records(void *ptr);

int
compare_two_panes(CompareType ct, ListType lt, int group_paths, int skip_empty)
{
	/* We don't compare lists of files, so skip the check if at least one of the
	 * views is a custom one. */
	if(!flist_custom_active(&lwin) && !flist_custom_active(&rwin) &&
			paths_are_same(flist_get_dir(&lwin), flist_get_dir(&rwin)))
	{
		ui_sb_err("Both views are at the same location");
		return 1;
	}

	int next_id = 1;
	entries_t curr, other;

	trie_t *const trie = trie_create(&free_compare_records);
	ui_cancellation_push_on();

	curr = make_diff_list(trie, curr_view, &next_id, ct, skip_empty, 0);
	other = make_diff_list(trie, other_view, &next_id, ct, skip_empty,
			lt == LT_DUPS);

	ui_cancellation_pop();
	trie_free(trie);

	/* Clear progress message displayed by make_diff_list(). */
	ui_sb_quick_msg_clear();

	if(ui_cancellation_requested())
	{
		free_dir_entries(curr_view, &curr.entries, &curr.nentries);
		free_dir_entries(other_view, &other.entries, &other.nentries);
		ui_sb_msg("Comparison has been cancelled");
		return 1;
	}

	if(!group_paths || lt != LT_ALL)
	{
		/* Sort both lists according to unique file numbers to group identical files
		 * (sorting is stable, tags are set in make_diff_list()). */
		safe_qsort(curr.entries, curr.nentries, sizeof(*curr.entries), &id_sorter);
		safe_qsort(other.entries, other.nentries, sizeof(*other.entries),
				&id_sorter);
	}

	if(lt == LT_UNIQUE)
	{
		make_unique_lists(curr, other);
		return 0;
	}

	if(lt == LT_DUPS)
	{
		leave_only_dups(&curr, &other);
	}

	flist_custom_start(curr_view, lt == LT_ALL ? "diff" : "dups diff");
	flist_custom_start(other_view, lt == LT_ALL ? "diff" : "dups diff");

	fill_side_by_side(curr, other, group_paths);

	if(flist_custom_finish(curr_view, CV_DIFF, 0) != 0)
	{
		show_error_msg("Comparison", "No results to display");
		return 0;
	}
	if(flist_custom_finish(other_view, CV_DIFF, 0) != 0)
	{
		assert(0 && "The error shouldn't be happening here.");
	}

	curr_view->list_pos = 0;
	other_view->list_pos = 0;
	curr_view->custom.diff_cmp_type = ct;
	other_view->custom.diff_cmp_type = ct;
	curr_view->custom.diff_path_group = group_paths;
	other_view->custom.diff_path_group = group_paths;

	assert(curr_view->list_rows == other_view->list_rows &&
			"Diff views must be in sync!");

	ui_view_schedule_redraw(curr_view);
	ui_view_schedule_redraw(other_view);
	return 0;
}

/* Composes two views containing only files that are unique to each of them.
 * Assumes that both lists are sorted by id. */
static void
make_unique_lists(entries_t curr, entries_t other)
{
	int i, j = 0;

	flist_custom_start(curr_view, "unique");
	flist_custom_start(other_view, "unique");

	/* Inspect entries of both sides in a manner similar to the merging procedure.
	 * Put unique ones into custom views and purge non-unique ones in place. */
	for(i = 0; i < other.nentries; ++i)
	{
		const int id = other.entries[i].id;

		while(j < curr.nentries && curr.entries[j].id < id)
		{
			flist_custom_put(curr_view, &curr.entries[j]);
			++j;
		}

		if(j >= curr.nentries || curr.entries[j].id != id)
		{
			flist_custom_put(other_view, &other.entries[i]);
			continue;
		}

		while(j < curr.nentries && curr.entries[j].id == id)
		{
			fentry_free(curr_view, &curr.entries[j++]);
		}
		while(i < other.nentries && other.entries[i].id == id)
		{
			fentry_free(other_view, &other.entries[i++]);
		}
		/* Want to revisit this entry on the next iteration of the loop. */
		--i;
	}

	/* After iterating through all elements in the other view, all entries that
	 * remain in the current view are unique to it. */
	while(j < curr.nentries)
	{
		flist_custom_put(curr_view, &curr.entries[j++]);
	}

	/* Entries' data has been moved out of them or freed, so need to free only the
	 * lists. */
	dynarray_free(curr.entries);
	dynarray_free(other.entries);

	(void)flist_custom_finish(curr_view, CV_REGULAR, 1);
	(void)flist_custom_finish(other_view, CV_REGULAR, 1);

	curr_view->list_pos = 0;
	other_view->list_pos = 0;

	ui_view_schedule_redraw(curr_view);
	ui_view_schedule_redraw(other_view);
}

/* Synchronizes two lists of entries so that they contain only items that
 * present in both of the lists.  Assumes that both lists are sorted by id. */
static void
leave_only_dups(entries_t *curr, entries_t *other)
{
	int new_id = 0;
	int i = 0, j = 0;

	/* Skip leading unmatched files. */
	while(i < other->nentries && other->entries[i].id == -1)
	{
		++i;
	}

	/* Process sequences of files in two lists. */
	while(i < other->nentries)
	{
		const int id = other->entries[i].id;

		/* Skip sequence of unmatched files. */
		while(j < curr->nentries && curr->entries[j].id < id)
		{
			curr->entries[j++].id = -1;
		}

		if(j < curr->nentries && curr->entries[j].id == id)
		{
			/* Allocate next id when we find a matching pair of files in both
			 * lists. */
			++new_id;
			/* Update sequence of identical ids for other list. */
			do
			{
				other->entries[i++].id = new_id;
			}
			while(i < other->nentries && other->entries[i].id == id);

			/* Update sequence of identical ids for current list. */
			do
			{
				curr->entries[j++].id = new_id;
			}
			while(j < curr->nentries && curr->entries[j].id == id);
		}
	}

	/* Exclude unprocessed files in the tail. */
	while(j < curr->nentries)
	{
		curr->entries[j++].id = -1;
	}

	(void)zap_entries(other_view, other->entries, &other->nentries,
			&is_not_duplicate, NULL, 1, 0);
	(void)zap_entries(curr_view, curr->entries, &curr->nentries,
			&is_not_duplicate, NULL, 1, 0);
}

/* zap_entries() filter to filter-out files marked for removal.  Returns
 * non-zero if entry is to be kept and zero otherwise. */
static int
is_not_duplicate(view_t *view, const dir_entry_t *entry, void *arg)
{
	return entry->id != -1;
}

/* Composes side-by-side comparison of files in two views. */
static void
fill_side_by_side(entries_t curr, entries_t other, int group_paths)
{
	enum { UP, LEFT, DIAG };

	int i, j;
	/* Describes results of solving sub-problems. */
	int (*d)[other.nentries + 1] =
		reallocarray(NULL, curr.nentries + 1, sizeof(*d));
	/* Describes paths (backtracking handles ambiguity badly). */
	char (*p)[other.nentries + 1] =
		reallocarray(NULL, curr.nentries + 1, sizeof(*p));

	for(i = 0; i <= curr.nentries; ++i)
	{
		for(j = 0; j <= other.nentries; ++j)
		{
			if(i == 0)
			{
				d[i][j] = j;
				p[i][j] = LEFT;
			}
			else if(j == 0)
			{
				d[i][j] = i;
				p[i][j] = UP;
			}
			else
			{
				const dir_entry_t *centry = &curr.entries[curr.nentries - i];
				const dir_entry_t *oentry = &other.entries[other.nentries - j];

				d[i][j] = MIN(d[i - 1][j] + 1, d[i][j - 1] + 1);
				p[i][j] = d[i][j] == d[i - 1][j] + 1 ? UP : LEFT;

				if((centry->id == oentry->id ||
							(group_paths && stroscmp(centry->name, oentry->name) == 0)) &&
						d[i - 1][j - 1] <= d[i][j])
				{
					d[i][j] = d[i - 1][j - 1];
					p[i][j] = DIAG;
				}
			}
		}
	}

	i = curr.nentries;
	j = other.nentries;
	while(i != 0 || j != 0)
	{
		switch(p[i][j])
		{
			dir_entry_t *e;

			case UP:
				e = &curr.entries[curr.nentries - 1 - --i];
				flist_custom_put(curr_view, e);
				flist_custom_add_separator(other_view, e->id);
				break;
			case LEFT:
				e = &other.entries[other.nentries - 1 - --j];
				flist_custom_put(other_view, e);
				flist_custom_add_separator(curr_view, e->id);
				break;
			case DIAG:
				flist_custom_put(curr_view, &curr.entries[curr.nentries - 1 - --i]);
				flist_custom_put(other_view, &other.entries[other.nentries - 1 - --j]);
				break;
		}
	}

	free(d);
	free(p);

	/* Entries' data has been moved out of them, so need to free only the
	 * lists. */
	dynarray_free(curr.entries);
	dynarray_free(other.entries);
}

int
compare_one_pane(view_t *view, CompareType ct, ListType lt, int skip_empty)
{
	int i, dup_id;
	view_t *other = (view == curr_view) ? other_view : curr_view;
	const char *const title = (lt == LT_ALL)  ? "compare"
	                        : (lt == LT_DUPS) ? "dups" : "nondups";

	int next_id = 1;
	entries_t curr;

	trie_t *trie = trie_create(&free_compare_records);
	ui_cancellation_push_on();

	curr = make_diff_list(trie, view, &next_id, ct, skip_empty, 0);

	ui_cancellation_pop();
	trie_free(trie);

	/* Clear progress message displayed by make_diff_list(). */
	ui_sb_quick_msg_clear();

	if(ui_cancellation_requested())
	{
		free_dir_entries(view, &curr.entries, &curr.nentries);
		ui_sb_msg("Comparison has been cancelled");
		return 1;
	}

	safe_qsort(curr.entries, curr.nentries, sizeof(*curr.entries), &id_sorter);

	flist_custom_start(view, title);

	dup_id = -1;
	next_id = 0;
	for(i = 0; i < curr.nentries; ++i)
	{
		dir_entry_t *entry = &curr.entries[i];

		if(lt == LT_ALL)
		{
			flist_custom_put(view, entry);
			continue;
		}

		if(entry->id == dup_id)
		{
			put_or_free(view, entry, next_id, lt == LT_DUPS);
			continue;
		}

		dup_id = (i < curr.nentries - 1 && entry[0].id == entry[1].id)
		       ? entry->id
		       : -1;

		if(entry->id == dup_id)
		{
			put_or_free(view, entry, ++next_id, lt == LT_DUPS);
			continue;
		}

		put_or_free(view, entry, next_id, lt == LT_UNIQUE);
	}

	/* Entries' data has been moved out of them or freed, so need to free only the
	 * list. */
	dynarray_free(curr.entries);

	if(flist_custom_finish(view, lt == LT_UNIQUE ? CV_REGULAR : CV_COMPARE,
				0) != 0)
	{
		show_error_msg("Comparison", "No results to display");
		return 0;
	}

	/* Leave the other pane, if it's in the CV_DIFF mode, two panes are needed for
	 * this. */
	if(other->custom.type == CV_DIFF)
	{
		rn_leave(other, 1);
	}

	view->list_pos = 0;
	ui_view_schedule_redraw(view);
	return 0;
}

/* qsort() comparer that stable sorts entries in ascending order.  Returns
 * standard -1, 0, 1 for comparisons. */
static int
id_sorter(const void *first, const void *second)
{
	const dir_entry_t *a = first;
	const dir_entry_t *b = second;
	return a->id == b->id ? a->tag - b->tag : a->id - b->id;
}

/* Either puts the entry into the view or frees it (depends on the take
 * argument). */
static void
put_or_free(view_t *view, dir_entry_t *entry, int id, int take)
{
	if(take)
	{
		entry->id = id;
		flist_custom_put(view, entry);
	}
	else
	{
		fentry_free(view, entry);
	}
}

/* Makes sorted by path list of entries that.  The trie is used to keep track of
 * identical files.  With non-zero dups_only, new files aren't added to the
 * trie. */
static entries_t
make_diff_list(trie_t *trie, view_t *view, int *next_id, CompareType ct,
		int skip_empty, int dups_only)
{
	int i;
	strlist_t files = {};
	entries_t r = {};
	int last_progress = 0;

	show_progress("Listing...", 0);
	if(flist_custom_active(view) &&
			ONE_OF(view->custom.type, CV_REGULAR, CV_VERY))
	{
		list_view_entries(view, &files);
	}
	else
	{
		list_files_recursively(view, flist_get_dir(view), view->hide_dot, &files);
	}

	show_progress("Querying...", 0);
	for(i = 0; i < files.nitems && !ui_cancellation_requested(); ++i)
	{
		int progress;
		int existing_id;
		char *fingerprint;
		const char *const path = files.items[i];
		dir_entry_t *const entry = entry_list_add(view, &r.entries, &r.nentries,
				path);

		if(skip_empty && entry->size == 0)
		{
			fentry_free(view, entry);
			--r.nentries;
			continue;
		}

		fingerprint = get_file_fingerprint(path, entry, ct);
		/* In case we couldn't obtain fingerprint (e.g., comparing by contents and
		 * files isn't readable), ignore the file and keep going. */
		if(is_null_or_empty(fingerprint))
		{
			free(fingerprint);
			fentry_free(view, entry);
			--r.nentries;
			continue;
		}

		entry->tag = i;
		if(get_file_id(trie, path, fingerprint, &existing_id, ct))
		{
			entry->id = existing_id;
		}
		else if(dups_only)
		{
			entry->id = -1;
		}
		else
		{
			entry->id = *next_id;
			++*next_id;
			put_file_id(trie, path, fingerprint, entry->id, ct);
		}

		free(fingerprint);

		progress = (i*100)/files.nitems;
		if(progress != last_progress)
		{
			char progress_msg[128];

			last_progress = progress;
			snprintf(progress_msg, sizeof(progress_msg), "Querying... %d (% 2d%%)", i,
					progress);
			show_progress(progress_msg, -1);
		}
	}

	free_string_array(files.items, files.nitems);
	return r;
}

/* Fills the list with entries of the view in hierarchical order (pre-order tree
 * traversal). */
static void
list_view_entries(const view_t *view, strlist_t *list)
{
	int i;

	fsdata_t *const tree = fsdata_create(0, 0);

	for(i = 0; i < view->list_rows; ++i)
	{
		if(!fentry_is_dir(&view->dir_entry[i]))
		{
			char full_path[PATH_MAX + 1];
			void *data = &view->dir_entry[i];
			get_full_path_of(&view->dir_entry[i], sizeof(full_path), full_path);
			fsdata_set(tree, full_path, &data, sizeof(data));
		}
	}

	fsdata_traverse(tree, &append_valid_nodes, list);

	fsdata_free(tree);
}

/* fsdata_traverse() callback that collects names of existing files into a
 * list.  Should return non-zero to stop traverser. */
static int
append_valid_nodes(const char name[], int valid, const void *parent_data,
		void *data, void *arg)
{
	strlist_t *const list = arg;
	dir_entry_t *const entry = *(dir_entry_t **)data;

	if(valid)
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);
		list->nitems = add_to_string_array(&list->items, list->nitems, full_path);
	}
	return 0;
}

/* Collects files under specified file system tree. */
static void
list_files_recursively(const view_t *view, const char path[],
		int skip_dot_files, strlist_t *list)
{
	int i;

	/* Obtain sorted list of files. */
	int len;
	char **lst = list_sorted_files(path, &len);
	if(len < 0)
	{
		return;
	}

	/* Visit all subdirectories ignoring symbolic links to directories. */
	for(i = 0; i < len && !ui_cancellation_requested(); ++i)
	{
		if(skip_dot_files && lst[i][0] == '.')
		{
			update_string(&lst[i], NULL);
			continue;
		}

		char *full_path = join_paths(path, lst[i]);
		const int dir = is_dir(full_path);
		if(!filters_file_is_visible(view, path, lst[i], dir, 1))
		{
			free(full_path);
			update_string(&lst[i], NULL);
			continue;
		}

		if(dir)
		{
			if(!is_symlink(full_path))
			{
				list_files_recursively(view, full_path, skip_dot_files, list);
			}
			free(full_path);
			update_string(&lst[i], NULL);
		}
		else
		{
			free(lst[i]);
			lst[i] = full_path;
		}

		show_progress("Listing...", 1000);
	}

	/* Append files. */
	for(i = 0; i < len; ++i)
	{
		if(lst[i] != NULL)
		{
			list->nitems = put_into_string_array(&list->items, list->nitems, lst[i]);
		}
	}

	free(lst);
}

/* Computes fingerprint of the file specified by path and entry.  Type of the
 * fingerprint is determined by ct parameter.  Returns newly allocated string
 * with the fingerprint, which is empty or NULL on error. */
static char *
get_file_fingerprint(const char path[], const dir_entry_t *entry,
		CompareType ct)
{
	switch(ct)
	{
		char name[NAME_MAX + 1];

		case CT_NAME:
			if(case_sensitive_paths(path))
			{
				return strdup(entry->name);
			}
			str_to_lower(entry->name, name, sizeof(name));
			return strdup(name);
		case CT_SIZE:
			return format_str("%" PRINTF_ULL, (unsigned long long)entry->size);
		case CT_CONTENTS:
			return get_contents_fingerprint(path, entry);
	}
	assert(0 && "Unexpected diffing type.");
	return strdup("");
}

/* Makes fingerprint of file contents (all or part of it of fixed size).
 * Returns the fingerprint as a string, which is empty or NULL on error. */
static char *
get_contents_fingerprint(const char path[], const dir_entry_t *entry)
{
	char block[BLOCK_SIZE];
	size_t to_read = PREFIX_SIZE;
	FILE *in = os_fopen(path, "rb");
	if(in == NULL)
	{
		return strdup("");
	}

	XXH3_state_t *st = XXH3_createState();
	if(st == NULL)
	{
		fclose(in);
		return strdup("");
	}

	if(XXH3_64bits_reset(st) == XXH_ERROR)
	{
		fclose(in);
		XXH3_freeState(st);
		return strdup("");
	}

	while(to_read != 0U)
	{
		const size_t portion = MIN(sizeof(block), to_read);
		const size_t nread = fread(&block, 1, portion, in);
		if(nread == 0U)
		{
			break;
		}

		XXH3_64bits_update(st, block, nread);
		to_read -= nread;
	}
	fclose(in);

	const unsigned long long digest = XXH3_64bits_digest(st);
	XXH3_freeState(st);

	return format_str("%" PRINTF_ULL "|%" PRINTF_ULL,
			(unsigned long long)entry->size, digest);
}

/* Retrieves file from the trie by its fingerprint.  Returns non-zero if it was
 * in the trie and sets *id, otherwise zero is returned. */
static int
get_file_id(trie_t *trie, const char path[], const char fingerprint[], int *id,
		CompareType ct)
{
	void *data;
	compare_record_t *record;
	if(trie_get(trie, fingerprint, &data) != 0)
	{
		return 0;
	}
	record = data;

	/* Comparison by contents is the only one when we need to resolve fingerprint
	 * conflicts. */
	if(ct != CT_CONTENTS)
	{
		*id = record->id;
		return 1;
	}

	/* Fingerprint does not guarantee a match, go through files and find file with
	 * identical content. */
	do
	{
		if(files_are_identical(path, record->path))
		{
			*id = record->id;
			return 1;
		}
		record = record->next;
	}
	while(record != NULL);

	return 0;
}

/* Checks whether two files specified by their names hold identical content.
 * Returns non-zero if so, otherwise zero is returned. */
static int
files_are_identical(const char a[], const char b[])
{
	char a_block[BLOCK_SIZE], b_block[BLOCK_SIZE];
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");

	if(a_file == NULL || b_file == NULL)
	{
		if(a_file != NULL)
		{
			fclose(a_file);
		}
		if(b_file != NULL)
		{
			fclose(b_file);
		}
		return 0;
	}

	while(1)
	{
		const size_t a_read = fread(&a_block, 1, sizeof(a_block), a_file);
		const size_t b_read = fread(&b_block, 1, sizeof(b_block), b_file);
		if(a_read == 0U && b_read == 0U && feof(a_file) && feof(b_file))
		{
			/* Ends of both files are reached. */
			break;
		}

		if(a_read == 0 || b_read == 0U || a_read != b_read ||
				memcmp(a_block, b_block, a_read) != 0)
		{
			fclose(a_file);
			fclose(b_file);
			return 0;
		}
	}

	fclose(a_file);
	fclose(b_file);
	return 1;
}

/* Stores id of a file with given fingerprint in the trie. */
static void
put_file_id(trie_t *trie, const char path[], const char fingerprint[], int id,
		CompareType ct)
{
	compare_record_t *const record = malloc(sizeof(*record));
	void *data = NULL;
	(void)trie_get(trie, fingerprint, &data);

	record->id = id;
	record->next = data;

	/* Comparison by contents is the only one when we need to resolve fingerprint
	 * conflicts. */
	record->path = (ct == CT_CONTENTS ? strdup(path) : NULL);

	if(trie_set(trie, fingerprint, record) < 0)
	{
		free(record->path);
		free(record);
	}
}

/* Frees list of compare entries.  Implements data free function for
 * trie_free_with_data(). */
static void
free_compare_records(void *ptr)
{
	compare_record_t *record = ptr;
	while(record != NULL)
	{
		compare_record_t *const current = record;
		record = record->next;
		free(current->path);
		free(current);
	}
}

int
compare_move(view_t *from, view_t *to)
{
	char from_path[PATH_MAX + 1], to_path[PATH_MAX + 1];
	char *from_fingerprint, *to_fingerprint;

	const CompareType ct = from->custom.diff_cmp_type;

	dir_entry_t *const curr = &from->dir_entry[from->list_pos];
	dir_entry_t *const other = &to->dir_entry[from->list_pos];

	if(from->custom.type != CV_DIFF || !from->custom.diff_path_group)
	{
		ui_sb_err("Not in diff mode with path grouping");
		return 1;
	}

	if(curr->id == other->id && !fentry_is_fake(curr) && !fentry_is_fake(other))
	{
		/* Nothing to do if files are already equal. */
		return 0;
	}

	/* We're going at least to try to update one of views (which might refer to
	 * the same directory), so schedule a reload. */
	ui_view_schedule_reload(from);
	ui_view_schedule_reload(to);

	if(fentry_is_fake(curr))
	{
		/* Just remove the other file (it can't be fake entry too). */
		return fops_delete_current(to, 1, 0);
	}

	get_full_path_of(curr, sizeof(from_path), from_path);
	get_full_path_of(other, sizeof(to_path), to_path);

	if(fentry_is_fake(other))
	{
		char to_path[PATH_MAX + 1];
		char canonical[PATH_MAX + 1];
		snprintf(to_path, sizeof(to_path), "%s/%s/%s", flist_get_dir(to),
				curr->origin + strlen(flist_get_dir(from)), curr->name);
		canonicalize_path(to_path, canonical, sizeof(canonical));

		/* Copy current file to position of the other one using relative path with
		 * different base. */
		fops_replace(from, canonical, 0);

		/* Update the other entry to not be fake. */
		remove_last_path_component(canonical);
		replace_string(&other->name, curr->name);
		replace_string(&other->origin, canonical);
	}
	else
	{
		/* Overwrite file in the other pane with corresponding file from current
		 * pane. */
		fops_replace(from, to_path, 1);
	}

	/* Obtaining file fingerprint relies on size field of entries, so try to load
	 * it and ignore if it fails. */
	other->size = get_file_size(to_path);

	/* Try to update id of the other entry by computing fingerprint of both files
	 * and checking if they match. */

	from_fingerprint = get_file_fingerprint(from_path, curr, ct);
	to_fingerprint = get_file_fingerprint(to_path, other, ct);

	if(!is_null_or_empty(from_fingerprint) && !is_null_or_empty(to_fingerprint))
	{
		int match = (strcmp(from_fingerprint, to_fingerprint) == 0);
		if(match && ct == CT_CONTENTS)
		{
			match = files_are_identical(from_path, to_path);
		}
		if(match)
		{
			other->id = curr->id;
		}
	}

	free(from_fingerprint);
	free(to_fingerprint);

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
