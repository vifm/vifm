#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fileops.h"

SETUP()
{
	assert_success(chdir(SANDBOX_PATH));

	strcpy(lwin.curr_dir, ".");
}

TEST(make_dirs_does_nothing_for_custom_view)
{
	int i;
	char path[] = "dir";
	char *paths[] = {path};

	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	make_dirs(&lwin, paths, 1, 0);
	assert_false(path_exists(path, NODEREF));

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free_dir_entry(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);

	filter_dispose(&lwin.local_filter.filter);
}

TEST(make_dirs_does_nothing_for_duplicated_names)
{
	char path[] = "dir";
	char *paths[] = {path, path};

	make_dirs(&lwin, paths, 2, 0);
	assert_false(path_exists(path, NODEREF));
}

TEST(make_dirs_does_nothing_for_empty_names)
{
	char path[] = "dir";
	char empty[] = "";
	char *paths[] = {path, empty};

	make_dirs(&lwin, paths, 2, 0);
	assert_false(path_exists(path, NODEREF));
}

TEST(make_dirs_does_nothing_for_existing_names)
{
	char path[] = "not-exist";
	char empty[] = ".";
	char *paths[] = {path, empty};

	make_dirs(&lwin, paths, 2, 0);
	assert_false(path_exists(path, NODEREF));
}

TEST(make_dirs_creates_one_dir)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "dir";
		char *paths[] = {path};

		make_dirs(&lwin, paths, 1, 0);
		assert_true(is_dir(path));

		assert_success(rmdir("dir"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_rel_path)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "parent/child";
		char *paths[] = {path};

		make_dirs(&lwin, paths, 1, 1);
		assert_true(is_dir(path));

		assert_success(rmdir("parent/child"));
		assert_success(rmdir("parent"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_abs_path)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = SANDBOX_PATH "/parent/child";
		char *paths[] = {path};

		make_dirs(&lwin, paths, 1, 1);
		assert_true(is_dir(path));

		assert_success(rmdir("parent/child"));
		assert_success(rmdir("parent"));
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
