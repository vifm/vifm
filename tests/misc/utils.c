#include "utils.h"

#include <stic.h>

#include <limits.h> /* INT_MAX */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

int
load_tree(view_t *view, const char path[], const char cwd[])
{
	return load_limited_tree(view, path, cwd, INT_MAX);
}

int
load_limited_tree(view_t *view, const char path[], const char cwd[], int depth)
{
	char abs_path[PATH_MAX + 1];
	make_abs_path(abs_path, sizeof(abs_path), path, "", cwd);
	return flist_load_tree(view, abs_path, depth);
}

void
load_view(view_t *view)
{
	curr_stats.load_stage = 2;
	load_saving_pos(view);
	curr_stats.load_stage = 0;
}

void
validate_tree(const view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		const dir_entry_t *const e = &view->dir_entry[i];
		assert_true(i + e->child_count + 1 <= view->list_rows);
		assert_true(i - e->child_pos >= 0);

		if(e->child_pos != 0)
		{
			const int j = i - e->child_pos;
			const dir_entry_t *const p = &view->dir_entry[j];
			assert_true(p->child_count >= e->child_pos);
			assert_true(j + p->child_count >= e->child_pos + e->child_count);
		}
	}
	validate_parents(view->dir_entry, view->list_rows);
}

void
validate_parents(const dir_entry_t *entries, int nchildren)
{
	int i = 0;
	while(i < nchildren)
	{
		const int parent = entries[0].child_pos;
		if(parent == 0)
		{
			assert_int_equal(0, entries[i].child_pos);
		}
		else
		{
			assert_int_equal(parent + i, entries[i].child_pos);
		}
		validate_parents(&entries[i] + 1, entries[i].child_count);
		i += entries[i].child_count + 1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
