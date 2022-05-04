#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <limits.h> /* INT_MAX */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_common.h"
#include "../../src/fops_misc.h"

static char choice_func(const char title[], const char message[],
		const struct response_variant *variants);

static char *saved_cwd;
static int choice_called = 0;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);

	update_string(&cfg.fuse_home, "");
}

TEARDOWN()
{
	view_teardown(&lwin);
	update_string(&cfg.fuse_home, NULL);

	restore_cwd(saved_cwd);
}

TEST(make_dirs_does_nothing_for_custom_view)
{
	char path[] = "dir";
	char *paths[] = {path};

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "",
			saved_cwd);

	filter_dispose(&lwin.local_filter.filter);
	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	fops_mkdirs(&lwin, -1, paths, 1, 0);
	assert_false(path_exists("dir", NODEREF));
}

TEST(make_dirs_does_nothing_for_duplicated_names)
{
	char path[] = "dir";
	char *paths[] = {path, path};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("dir", NODEREF));
}

TEST(make_dirs_does_nothing_for_empty_names)
{
	char path[] = "dir";
	char empty[] = "";
	char *paths[] = {path, empty};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("dir", NODEREF));
}

TEST(make_dirs_does_nothing_for_existing_names)
{
	char path[] = "not-exist";
	char empty[] = ".";
	char *paths[] = {path, empty};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("not-exist", NODEREF));
}

TEST(make_dirs_creates_one_dir)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "dir";
		char *paths[] = {path};

		fops_mkdirs(&lwin, -1, paths, 1, 0);

		assert_true(is_dir("dir"));

		assert_success(rmdir("dir"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_rel_path)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "parent/child";
		char *paths[] = { path };

		fops_mkdirs(&lwin, -1, paths, 1, 1);
		assert_true(is_dir("parent/child"));

		assert_success(rmdir("parent/child"));
		assert_success(rmdir("parent"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_abs_path)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[PATH_MAX + 1];
		char *paths[] = { path };

		make_abs_path(path, sizeof(path), SANDBOX_PATH, "parent/child", saved_cwd);

		fops_mkdirs(&lwin, -1, paths, 1, 1);
		assert_true(is_dir("parent/child"));

		assert_success(rmdir("parent/child"));
		assert_success(rmdir("parent"));
	}
}

TEST(make_dirs_considers_tree_structure)
{
	char path[] = "new-dir";
	char *paths[] = { path };

	create_dir("dir");

	flist_load_tree(&lwin, lwin.curr_dir, INT_MAX);

	/* Set at to -1. */
	lwin.list_pos = 0;
	(void)fops_mkdirs(&lwin, -1, paths, 1, 0);

	/* Set at to desired position. */
	(void)fops_mkdirs(&lwin, 1, paths, 1, 0);

	/* Remove both files afterward to make sure they can both be created at the
	 * same time. */
	assert_success(rmdir("new-dir"));
	assert_success(rmdir("dir/new-dir"));

	assert_success(rmdir("dir"));
}

TEST(check_by_absolute_path_is_performed_beforehand)
{
	char name_a[] = "a";
	char name_b[PATH_MAX + 8];
	char *names[] = { name_a, name_b };

	snprintf(name_b, sizeof(name_b), "%s/b", lwin.curr_dir);
	create_dir(name_b);

	(void)fops_mkdirs(&lwin, -1, names, 2, 0);

	assert_failure(rmdir("a"));
	assert_success(rmdir("b"));
}

TEST(make_dirs_errors_are_handled)
{
	cfg.use_system_calls = 1;
	fops_init(NULL, &choice_func);

	char path[] = "dir/sub";
	char *paths[] = {path};

	choice_called = 0;

	fops_mkdirs(&lwin, -1, paths, 1, /*create_parent=*/0);
	assert_int_equal(1, choice_called);

	cfg.use_system_calls = 0;
	fops_init(NULL, NULL);
}

static char
choice_func(const char title[], const char message[],
		const struct response_variant *variants)
{
	++choice_called;
	return 'a';
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
