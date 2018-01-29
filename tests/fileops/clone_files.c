#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() symlink() unlink() */

#include "../../src/cfg/config.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	cfg.use_trash = 0;
}

TEARDOWN()
{
	view_teardown(&lwin);
	restore_cwd(saved_cwd);
}

TEST(files_are_cloned)
{
	create_empty_file("a");
	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	(void)fops_clone(&lwin, NULL, 0, 0, 2);

	assert_success(unlink("a"));
	assert_success(unlink("a(1)"));
	assert_success(unlink("a(2)"));
	assert_success(unlink("a(3)"));
}

TEST(files_are_cloned_with_custom_name)
{
	char *names[] = { "b" };

	create_empty_file("a");
	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_clone(&lwin, names, 1, 0, 1);

	assert_success(unlink("a"));
	assert_success(unlink("b"));
}

TEST(files_are_cloned_according_to_tree_structure)
{
	create_empty_dir("dir");
	create_empty_file("dir/a");

	/* Clone at the top level. */

	flist_load_tree(&lwin, ".");
	lwin.list_pos = 0;

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 0;
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_success(unlink("dir(1)/a"));
	assert_success(rmdir("dir(1)"));

	/* Clone at nested level. */

	flist_load_tree(&lwin, ".");
	lwin.list_pos = 0;

	lwin.dir_entry[0].marked = 0;
	lwin.dir_entry[1].marked = 1;
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_success(unlink("dir/a(1)"));

	/* Clone at both levels. */

	flist_load_tree(&lwin, ".");
	lwin.list_pos = 0;

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_success(unlink("dir(1)/a"));
	assert_success(rmdir("dir(1)"));
	assert_success(unlink("dir/a(1)"));

	/* Cloning same file twice. */

	flist_load_tree(&lwin, ".");
	lwin.list_pos = 0;
	lwin.dir_entry[1].marked = 1;
	assert_string_equal("a", lwin.dir_entry[1].name);
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	populate_dir_list(&lwin, 1);
	lwin.list_pos = 0;
	lwin.dir_entry[1].marked = 1;
	assert_string_equal("a", lwin.dir_entry[1].name);
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_success(unlink("dir/a(1)"));
	assert_success(unlink("dir/a(2)"));

	assert_success(unlink("dir/a"));
	assert_success(rmdir("dir"));
}

TEST(cloning_does_not_work_in_custom_view)
{
	char *names[] = { "a-clone" };

	create_empty_file("do-not-clone-me");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "do-not-clone-me");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	/* Without specifying new name. */
	lwin.dir_entry[0].marked = 1;
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_failure(unlink("do-not-clone-me(1)"));

	/* With name specified. */
	lwin.dir_entry[0].marked = 1;
	(void)fops_clone(&lwin, names, 1, 0, 1);
	assert_failure(unlink("a-clone"));

	assert_success(unlink("do-not-clone-me"));
}

TEST(cloning_of_broken_symlink, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("no-such-file", "broken-link"));
#endif

	flist_load_tree(&lwin, ".");

	/* Without specifying new name. */
	lwin.dir_entry[0].marked = 1;
	(void)fops_clone(&lwin, NULL, 0, 0, 1);
	assert_success(unlink("broken-link(1)"));

	assert_success(unlink("broken-link"));
}

TEST(cloning_is_aborted_if_we_can_not_read_a_file, IF(not_windows))
{
	create_empty_file("can-read");
	create_empty_file("can-not-read");
	assert_success(chmod("can-not-read", 0000));
	populate_dir_list(&lwin, 0);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	assert_int_equal(0, fops_clone(&lwin, NULL, 0, 0, 1));

	assert_success(unlink("can-read"));
	assert_success(unlink("can-not-read"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
