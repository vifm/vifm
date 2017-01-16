#include <stic.h>

#include <unistd.h> /* rmdir() unlink() */

#include <string.h> /* memset() strcpy() */

#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

#include "utils.h"

SETUP()
{
	view_setup(&lwin);

	init_commands();

	lwin.sort_g[0] = SK_BY_NAME;
	memset(&lwin.sort_g[1], SK_NONE, sizeof(lwin.sort_g) - 1);

	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);

	reset_cmds();

	opt_handlers_teardown();
}

TEST(sibl_do_nothing_in_cv)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
}

TEST(sibl_navigate_correctly)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/read");

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/rename"));

	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/read"));
}

TEST(sibl_does_not_wrap_by_default)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/various-sizes");

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/various-sizes"));

	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare");

	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/compare"));
}

TEST(sibl_wrap)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/various-sizes");

	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/compare"));

	exec_commands("siblprev!", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/various-sizes"));
}

TEST(sibl_handles_errors_without_failures)
{
	strcpy(lwin.curr_dir, "not/a/valid/path");
	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	assert_string_equal(lwin.curr_dir, "not/a/valid/path");

	strcpy(lwin.curr_dir, TEST_DATA_PATH "/not_a_valid_last_entry");
	exec_commands("siblprev", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir,
				TEST_DATA_PATH "/not_a_valid_last_entry"));
}

TEST(sibl_skips_files_and_can_work_without_sorting)
{
	assert_success(os_mkdir(SANDBOX_PATH "/a", 0700));
	create_file(SANDBOX_PATH "/b");

	lwin.sort_g[0] = SK_NONE;

	strcpy(lwin.curr_dir, SANDBOX_PATH "/a");
	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir, SANDBOX_PATH "/a"));

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(unlink(SANDBOX_PATH "/b"));
}

TEST(sibl_uses_global_sort_option)
{
	lwin.sort[0] = -SK_BY_NAME;

	strcpy(lwin.curr_dir, TEST_DATA_PATH "/read");

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/rename"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
