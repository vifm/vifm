#include <stic.h>

#include <unistd.h> /* rmdir() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

#include "utils.h"

static void validate_tree(const FileView *view);
static void validate_parents(const dir_entry_t *entries, int nchildren);

SETUP()
{
	update_string(&cfg.fuse_home, "no");
	update_string(&cfg.slow_fs_list, "");

	view_setup(&lwin);

	curr_view = &lwin;
	other_view = &lwin;
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);
}

TEST(empty_directory_tree_is_created)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH "/empty-dir"));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(complex_tree_is_built_correctly)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_dot_filter)
{
	lwin.hide_dot = 1;

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_auto_filter)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	lwin.dir_entry[11].selected = 1;
	lwin.selected_files = 1;
	filter_selected_files(&lwin);

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_manual_filter)
{
	(void)filter_set(&lwin.manual_filter, "^\\.hidden$");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_local_filter)
{
	(void)filter_set(&lwin.local_filter.filter, "file|dir");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

static void
validate_tree(const FileView *view)
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

static void
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
/* vim: set cinoptions+=t0 filetype=c : */
