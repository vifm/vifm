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
#include "flist_sel.h"
#include "fops_common.h"
#include "fops_cpmv.h"
#include "fops_misc.h"
#include "running.h"
#include "undo.h"

/*
 * Optimization for content-based matching.
 *
 * At first fingerprint uses only size of the file, then it's looked up to see
 * if there is any other file of the same size.  If there isn't, store compare
 * record by that fingerprint.
 *
 * If there is, two cases are possible:
 *   - there is only one conflicting file and its contents fingerprint wasn't
 *     yet computed:
 *       * compute contents fingerprint for that file and insert it
 *       * compute contents fingerprint for current file and insert it
 *   - there is more than one conflicting file:
 *       * compute contents fingerprint for current file and insert it
 */

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
	int is_partial;                /* Shows that fingerprinting was lazy. */
	struct compare_record_t *next; /* Next entry in the list of conflicts. */
}
compare_record_t;

static void make_unique_lists(entries_t curr, entries_t other);
static void leave_only_dups(entries_t *curr, entries_t *other);
static int is_not_duplicate(view_t *view, const dir_entry_t *entry, void *arg);
static void fill_side_by_side_by_paths(entries_t curr, entries_t other,
		int flags, compare_stats_t *stats);
static void fill_side_by_side_by_ids(entries_t curr, entries_t other, int flags,
		compare_stats_t *stats);
static int compare_entries(dir_entry_t *curr, dir_entry_t *other, int flags);
static void put_side_by_side_pair(dir_entry_t *curr, dir_entry_t *other,
		int flags, compare_stats_t *stats);
static int id_sorter(const void *first, const void *second);
static void put_or_free(view_t *view, dir_entry_t *entry, int id, int take);
static entries_t make_diff_list(trie_t *trie, view_t *view, int *next_id,
		CompareType ct, int dups_only, int flags);
static void list_view_entries(const view_t *view, strlist_t *list);
static int append_valid_nodes(const char name[], int valid,
		const void *parent_data, void *data, void *arg);
static void list_files_recursively(const view_t *view, const char path[],
		int skip_dot_files, int flags, strlist_t *list);
static char * get_file_fingerprint(const char path[], const dir_entry_t *entry,
		CompareType ct, int flags, int lazy);
static char * get_contents_fingerprint(const char path[],
		unsigned long long size);
static int add_file_to_diff(trie_t *trie, const char path[], dir_entry_t *entry,
		CompareType ct, int dups_only, int flags, int *next_id);
static int files_are_identical(const char a[], const char b[]);
static void put_file_id(trie_t *trie, const char path[],
		const char fingerprint[], int id, int is_partial, CompareType ct);
static void free_compare_records(void *ptr);
static void compare_move_entry(ops_t *ops, view_t *from, view_t *to, int idx);

int
compare_two_panes(CompareType ct, ListType lt, int flags)
{
	assert((flags & (CF_IGNORE_CASE | CF_RESPECT_CASE)) !=
			(CF_IGNORE_CASE | CF_RESPECT_CASE) && "Wrong combination of flags.");

	const int group_paths = flags & CF_GROUP_PATHS;

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

	curr = make_diff_list(trie, curr_view, &next_id, ct, /*dups_only=*/0, flags);
	other = make_diff_list(trie, other_view, &next_id, ct, lt == LT_DUPS, flags);

	ui_cancellation_pop();
	trie_free(trie);

	/* Clear progress message displayed by make_diff_list(). */
	ui_sb_quick_msg_clear();

	if(ui_cancellation_requested())
	{
		free_dir_entries(&curr.entries, &curr.nentries);
		free_dir_entries(&other.entries, &other.nentries);
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

	compare_stats_t stats = {};

	if(group_paths)
	{
		fill_side_by_side_by_paths(curr, other, flags, &stats);
	}
	else
	{
		fill_side_by_side_by_ids(curr, other, flags, &stats);
	}

	/* Entries' data has been moved out of them, so need to free only the
	 * lists. */
	dynarray_free(curr.entries);
	dynarray_free(other.entries);

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
	curr_view->custom.diff_list_type = lt;
	other_view->custom.diff_list_type = lt;
	curr_view->custom.diff_cmp_flags = flags;
	other_view->custom.diff_cmp_flags = flags;

	/* Both views share the same stats, alternatively can put to status_t, but
	 * then have to save/store per global tab. */
	curr_view->custom.diff_stats = stats;
	other_view->custom.diff_stats = stats;

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
			fentry_free(&curr.entries[j++]);
		}
		while(i < other.nentries && other.entries[i].id == id)
		{
			fentry_free(&other.entries[i++]);
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

/* Composes side-by-side comparison of files in two views centered around their
 * relative paths. */
static void
fill_side_by_side_by_paths(entries_t curr, entries_t other, int flags,
		compare_stats_t *stats)
{
	int i = 0;
	int j = 0;

	while(i < curr.nentries || j < other.nentries)
	{
		int cmp;

		if(i < curr.nentries && j < other.nentries)
		{
			cmp = compare_entries(&curr.entries[i], &other.entries[j], flags);
		}
		else
		{
			cmp = (i < curr.nentries ? -1 : 1);
		}

		if(cmp == 0)
		{
			put_side_by_side_pair(&curr.entries[i++], &other.entries[j++], flags,
					stats);
		}
		else if(cmp < 0)
		{
			put_side_by_side_pair(&curr.entries[i++], NULL, flags, stats);
		}
		else
		{
			put_side_by_side_pair(NULL, &other.entries[j++], flags, stats);
		}
	}
}

/* Composes side-by-side comparison of files in two views that is guided by
 * comparison ids and minimizes edit script. */
static void
fill_side_by_side_by_ids(entries_t curr, entries_t other, int flags,
		compare_stats_t *stats)
{
	enum { UP, LEFT, DIAG };

	int i, j;

	/* Describes results of solving sub-problems.  Only two rows of the full
	 * table. */
	int (*d)[other.nentries + 1] = reallocarray(NULL, 2, sizeof(*d));
	/* Describes paths (backtracking handles ambiguity badly). */
	char (*p)[other.nentries + 1] =
		reallocarray(NULL, curr.nentries + 1, sizeof(*p));

	for(i = 0; i <= curr.nentries; ++i)
	{
		int row = i%2;
		for(j = 0; j <= other.nentries; ++j)
		{
			if(i == 0)
			{
				d[row][j] = j;
				p[i][j] = LEFT;
			}
			else if(j == 0)
			{
				d[row][j] = i;
				p[i][j] = UP;
			}
			else
			{
				const dir_entry_t *centry = &curr.entries[curr.nentries - i];
				const dir_entry_t *oentry = &other.entries[other.nentries - j];

				d[row][j] = MIN(d[1 - row][j] + 1, d[row][j - 1] + 1);
				p[i][j] = d[row][j] == d[1 - row][j] + 1 ? UP : LEFT;

				if(centry->id == oentry->id && d[1 - row][j - 1] <= d[row][j])
				{
					d[row][j] = d[1 - row][j - 1];
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
			case UP:
				put_side_by_side_pair(&curr.entries[curr.nentries - i--], NULL, flags,
						stats);
				break;
			case LEFT:
				put_side_by_side_pair(NULL, &other.entries[other.nentries - j--], flags,
						stats);
				break;
			case DIAG:
				put_side_by_side_pair(&curr.entries[curr.nentries - i--],
						&other.entries[other.nentries - j--], flags, stats);
				break;
		}
	}

	free(d);
	free(p);
}

/* Adds an entry per side.  Either curr or other can be NULL. */
static void
put_side_by_side_pair(dir_entry_t *curr, dir_entry_t *other, int flags,
		compare_stats_t *stats)
{
	/* Integer type to avoid warning about unhandled cases in switch(). */
	int flag;

	if(other == NULL)
	{
		flag = (curr_view == &lwin ? CF_SHOW_UNIQUE_LEFT : CF_SHOW_UNIQUE_RIGHT);
	}
	else if(curr == NULL)
	{
		flag = (other_view == &lwin ? CF_SHOW_UNIQUE_LEFT : CF_SHOW_UNIQUE_RIGHT);
	}
	else
	{
		flag = (curr->id == other->id ? CF_SHOW_IDENTICAL : CF_SHOW_DIFFERENT);
	}

	switch(flag)
	{
		case CF_SHOW_UNIQUE_LEFT:  ++stats->unique_left;  break;
		case CF_SHOW_UNIQUE_RIGHT: ++stats->unique_right; break;
		case CF_SHOW_IDENTICAL:    ++stats->identical;    break;
		case CF_SHOW_DIFFERENT:    ++stats->different;    break;
	}

	if(flags & flag)
	{
		if(curr != NULL)
		{
			flist_custom_put(curr_view, curr);
		}
		else
		{
			flist_custom_add_separator(curr_view, other->id);
		}

		if(other != NULL)
		{
			flist_custom_put(other_view, other);
		}
		else
		{
			flist_custom_add_separator(other_view, curr->id);
		}
	}
	else
	{
		if(curr != NULL)
		{
			fentry_free(curr);
		}
		if(other != NULL)
		{
			fentry_free(other);
		}
	}
}

/* Compares entries by their short paths.  Returns strcmp()-like result. */
static int
compare_entries(dir_entry_t *curr, dir_entry_t *other, int flags)
{
	char path_a[PATH_MAX + 1], path_b[PATH_MAX + 1];
	get_full_path_of(curr, sizeof(path_a), path_a);
	get_full_path_of(other, sizeof(path_b), path_b);

	int case_sensitive;
	if(flags & CF_IGNORE_CASE)
	{
		case_sensitive = 0;
	}
	else if(flags & CF_RESPECT_CASE)
	{
		case_sensitive = 1;
	}
	else
	{
		/* If at least one path is case-sensitive, don't ignore case.  Otherwise, we
		 * would end up with multiple matching pairs of paths. */
		case_sensitive = case_sensitive_paths(path_a)
		              || case_sensitive_paths(path_b);
	}

	get_short_path_of(curr_view, curr, NF_NONE, 0, sizeof(path_a), path_a);
	get_short_path_of(other_view, other, NF_NONE, 0, sizeof(path_b), path_b);

	const char *a = path_a, *b = path_b;

	char lower_a[PATH_MAX + 1], lower_b[PATH_MAX + 1];
	if(!case_sensitive)
	{
		str_to_lower(path_a, lower_a, sizeof(lower_a));
		str_to_lower(path_b, lower_b, sizeof(lower_b));

		a = lower_a;
		b = lower_b;
	}

	while(*a == *b && *a != '\0')
	{
		++a;
		++b;
	}

	int cmp = *a - *b;

	/* Correct result to reflect that a directory is smaller than a
	 * non-directory. */
	int dir_a = (strchr(a, '/') != NULL);
	int dir_b = (strchr(b, '/') != NULL);
	if(dir_a != dir_b)
	{
		cmp = (dir_a ? -1 : 1);
	}

	return cmp;
}

int
compare_one_pane(view_t *view, CompareType ct, ListType lt, int flags)
{
	assert((flags & (CF_IGNORE_CASE | CF_RESPECT_CASE)) !=
			(CF_IGNORE_CASE | CF_RESPECT_CASE) && "Wrong combination of flags.");

	int i, dup_id;
	view_t *other = (view == curr_view) ? other_view : curr_view;
	const char *const title = (lt == LT_ALL)  ? "compare"
	                        : (lt == LT_DUPS) ? "dups" : "nondups";

	int next_id = 1;
	entries_t curr;

	trie_t *trie = trie_create(&free_compare_records);
	ui_cancellation_push_on();

	curr = make_diff_list(trie, view, &next_id, ct, /*dups_only=*/0, flags);

	ui_cancellation_pop();
	trie_free(trie);

	/* Clear progress message displayed by make_diff_list(). */
	ui_sb_quick_msg_clear();

	if(ui_cancellation_requested())
	{
		free_dir_entries(&curr.entries, &curr.nentries);
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

	curr_view->custom.diff_cmp_flags = flags;
	other_view->custom.diff_cmp_flags = flags;

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
		fentry_free(entry);
	}
}

/* Makes sorted by path list of entries that.  The trie is used to keep track of
 * identical files.  With non-zero dups_only, new files aren't added to the
 * trie. */
static entries_t
make_diff_list(trie_t *trie, view_t *view, int *next_id, CompareType ct,
		int dups_only, int flags)
{
	const int skip_empty = flags & CF_SKIP_EMPTY;

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
		list_files_recursively(view, flist_get_dir(view), view->hide_dot, flags,
				&files);
	}

	show_progress("Querying...", 0);
	for(i = 0; i < files.nitems && !ui_cancellation_requested(); ++i)
	{
		int progress;
		const char *const path = files.items[i];
		dir_entry_t *const entry = entry_list_add(view, &r.entries, &r.nentries,
				path);
		if(entry == NULL)
		{
			/* Maybe the file doesn't exist anymore, maybe we've lost access to it or
			 * we're just out of memory. */
			continue;
		}

		if(skip_empty && entry->size == 0)
		{
			fentry_free(entry);
			--r.nentries;
			continue;
		}

		entry->tag = i;
		entry->id = add_file_to_diff(trie, path, entry, ct, dups_only, flags,
				next_id);

		if(entry->id == -1)
		{
			fentry_free(entry);
			--r.nentries;
		}

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
		int skip_dot_files, int flags, strlist_t *list)
{
	int i;

	/* Obtain sorted list of files. */
	int len;
	char **lst = list_all_files(path, &len);
	if(len < 0)
	{
		return;
	}

	if(flags & CF_IGNORE_CASE)
	{
		safe_qsort(lst, len, sizeof(*lst), &strcasesorter);
	}
	else if(flags & CF_RESPECT_CASE)
	{
		safe_qsort(lst, len, sizeof(*lst), &strsorter);
	}
	else
	{
		safe_qsort(lst, len, sizeof(*lst), &strossorter);
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
				list_files_recursively(view, full_path, skip_dot_files, flags, list);
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
 * fingerprint is determined by ct parameter.  Lazy fingerprint is an
 * optimization which prevents computing contents fingerprint until there is
 * more than one file of the given size.  Returns newly allocated string with
 * the fingerprint, which is empty or NULL on error. */
static char *
get_file_fingerprint(const char path[], const dir_entry_t *entry,
		CompareType ct, int flags, int lazy)
{
	switch(ct)
	{
		int case_sensitive;
		char name[NAME_MAX + 1];

		case CT_NAME:
			if(flags & CF_IGNORE_CASE)
			{
				case_sensitive = 0;
			}
			else if(flags & CF_RESPECT_CASE)
			{
				case_sensitive = 1;
			}
			else
			{
				case_sensitive = case_sensitive_paths(path);
			}

			if(case_sensitive)
			{
				return strdup(entry->name);
			}

			str_to_lower(entry->name, name, sizeof(name));
			return strdup(name);
		case CT_SIZE:
			return format_str("%" PRINTF_ULL, (unsigned long long)entry->size);
		case CT_CONTENTS:
			if(lazy)
			{
				/* Comparing by contents can't be done if file can't be read. */
				if(os_access(path, R_OK) != 0)
				{
					return strdup("");
				}

				return format_str("%" PRINTF_ULL, (unsigned long long)entry->size);
			}
			return get_contents_fingerprint(path, entry->size);
	}
	assert(0 && "Unexpected diffing type.");
	return strdup("");
}

/* Makes fingerprint of file contents (all or part of it of fixed size).
 * Returns the fingerprint as a string, which is empty or NULL on error. */
static char *
get_contents_fingerprint(const char path[], unsigned long long size)
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

	return format_str("%" PRINTF_ULL "|%" PRINTF_ULL, size, digest);
}

/* Looks up file in the trie by its fingerprint.  Returns id for the file or -1
 * if it should be skipped. */
static int
add_file_to_diff(trie_t *trie, const char path[], dir_entry_t *entry,
		CompareType ct, int dups_only, int flags, int *next_id)
{
	char *fingerprint = get_file_fingerprint(path, entry, ct, flags, /*lazy=*/1);
	if(is_null_or_empty(fingerprint))
	{
		/* In case we couldn't obtain fingerprint (e.g., comparing by contents and
		 * the file isn't readable), ignore the file and keep going. */
		free(fingerprint);
		return -1;
	}

	void *data = NULL;
	(void)trie_get(trie, fingerprint, &data);

	compare_record_t *record = data;
	int is_partial = (ct == CT_CONTENTS);

	/* Comparison by contents is the only one when we need to account for lazy
	 * fingerprint computation or resolve fingerprint conflicts. */
	if(record != NULL && ct == CT_CONTENTS)
	{
		free(fingerprint);
		is_partial = 0;

		fingerprint = get_file_fingerprint(path, entry, ct, flags, /*lazy=*/0);
		if(is_null_or_empty(fingerprint))
		{
			/* In case we couldn't obtain fingerprint (e.g., comparing by contents and
			 * the file isn't readable), ignore the file and keep going. */
			free(fingerprint);
			return -1;
		}

		if(record->is_partial)
		{
			/* There is another file of the same size whose contents fingerprint
			 * hasn't been computed yet.  Do it here. */
			char *other_fingerprint = get_contents_fingerprint(record->path,
					entry->size);
			if(is_null_or_empty(fingerprint))
			{
				/* That other file has issues, don't update it and skip any other file
				 * that can conflict with it by size.  The file itself won't be skipped
				 * though, should it be? */
				free(other_fingerprint);
				free(fingerprint);
				return -1;
			}

			put_file_id(trie, record->path, other_fingerprint, record->id,
					/*is_partial=*/0, ct);
			free(other_fingerprint);

			record->is_partial = 0;
		}

		/* Repeat trie lookup with contents fingerprint. */
		(void)trie_get(trie, fingerprint, &data);
		record = data;

		/* Fingerprint does not guarantee a match, go through files and find file
		 * with identical contents. */
		do
		{
			if(files_are_identical(path, record->path))
			{
				break;
			}
			record = record->next;
		}
		while(record != NULL);
	}

	if(record != NULL)
	{
		free(fingerprint);
		return record->id;
	}

	if(dups_only)
	{
		free(fingerprint);
		return -1;
	}

	int id = *next_id;
	++*next_id;
	put_file_id(trie, path, fingerprint, id, is_partial, ct);

	free(fingerprint);
	return id;
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
		int is_partial, CompareType ct)
{
	compare_record_t *const record = malloc(sizeof(*record));

	record->id = id;
	record->is_partial = is_partial;
	record->next = NULL;

	/* Comparison by contents is the only one when we need to resolve fingerprint
	 * conflicts. */
	record->path = (ct == CT_CONTENTS ? strdup(path) : NULL);

	/* Just add new entry to the list if something is already there. */
	void *data = NULL;
	(void)trie_get(trie, fingerprint, &data);
	compare_record_t *prev = data;
	if(prev != NULL)
	{
		prev->next = record;
		return;
	}

	/* Otherwise we're the head of the list. */
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
	const CompareType flags = from->custom.diff_cmp_flags;
	if(from->custom.type != CV_DIFF || !(flags & CF_GROUP_PATHS))
	{
		ui_sb_err("Not in diff mode with path grouping");
		return 1;
	}

	char *from_dir = strdup(replace_home_part(flist_get_dir(from)));
	char *to_dir = strdup(replace_home_part(flist_get_dir(to)));
	if(from_dir == NULL || to_dir == NULL)
	{
		free(from_dir);
		free(to_dir);
		return 1;
	}

	char undo_msg[2*PATH_MAX + 32];
	snprintf(undo_msg, sizeof(undo_msg), "Diff apply %s -> %s", from_dir, to_dir);

	ops_t *ops = fops_get_ops(OP_COPY, "Applying", from_dir, to_dir);
	free(from_dir);
	free(to_dir);

	if(ops == NULL)
	{
		show_error_msg("Comparison", "Failed to initialize apply operation");
		return 1;
	}

	/* We're going at least to try to update one of the views (which might refer
	 * to the same directory), so schedule a reload. */
	ui_view_schedule_reload(from);
	ui_view_schedule_reload(to);

	un_group_open(undo_msg);

	dir_entry_t *entry = NULL;
	while(iter_selection_or_current_any(curr_view, &entry) && fops_active(ops))
	{
		compare_move_entry(ops, from, to, entry_to_pos(curr_view, entry));
	}

	un_group_close();

	fops_free_ops(ops);
	flist_sel_stash(curr_view);
	return 0;
}

/* Moves a file identified by an entry from one view to the other. */
static void
compare_move_entry(ops_t *ops, view_t *from, view_t *to, int idx)
{
	char from_path[PATH_MAX + 1], to_path[PATH_MAX + 1];
	char *from_fingerprint, *to_fingerprint;

	const CompareType ct = from->custom.diff_cmp_type;
	const CompareType flags = from->custom.diff_cmp_flags;

	dir_entry_t *const curr = &from->dir_entry[idx];
	dir_entry_t *const other = &to->dir_entry[idx];

	if(curr->id == other->id && !fentry_is_fake(curr) && !fentry_is_fake(other))
	{
		/* Nothing to do if files are already equal. */
		return;
	}

	if(fentry_is_fake(curr))
	{
		/* Just remove the other file (it can't be fake entry too). */
		fops_delete_entry(ops, to, other, /*use_trash=*/1, /*nested=*/0);
		return;
	}

	get_full_path_of(curr, sizeof(from_path), from_path);
	get_full_path_of(other, sizeof(to_path), to_path);

	/* Overwrite file in the other pane with corresponding file from current
	 * pane. */
	fops_replace_entry(ops, from, curr, to, other);

	/* Obtaining file fingerprint relies on size field of entries, so try to load
	 * it and ignore if it fails. */
	other->size = get_file_size(to_path);

	/* Try to update id of the other entry by computing fingerprint of both files
	 * and checking if they match. */

	from_fingerprint = get_file_fingerprint(from_path, curr, ct, flags,
			/*lazy=*/0);
	to_fingerprint = get_file_fingerprint(to_path, other, ct, flags, /*lazy=*/0);

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
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
