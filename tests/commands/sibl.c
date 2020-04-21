#include <stic.h>

#include <unistd.h> /* chdir() rmdir() unlink() */

#include <string.h> /* memset() strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	curr_view = &lwin;
	other_view = &rwin;
}

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

	vle_cmds_reset();

	opt_handlers_teardown();
}

TEST(sibl_do_nothing_in_cv)
{
	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files/a", cwd);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
}

TEST(sibl_navigate_correctly)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "rename", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(exec_commands("2siblnext", &lwin, CIT_COMMAND));
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "scripts", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(exec_commands("3siblprev", &lwin, CIT_COMMAND));
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "quotes-in-names", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));
}

TEST(sibl_does_not_wrap_by_default)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"various-sizes", cwd);

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/various-sizes"));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"color-schemes", cwd);

	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH "/color-schemes"));
}

TEST(sibl_wrap)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"various-sizes", cwd);

	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "color-schemes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	exec_commands("siblprev!", &lwin, CIT_COMMAND);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "various-sizes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));
}

TEST(sibl_handles_errors_without_failures)
{
	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "not_a_valid_last_entry",
			cwd);

	strcpy(lwin.curr_dir, "not/a/valid/path");
	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	assert_string_equal(lwin.curr_dir, "not/a/valid/path");

	strcpy(lwin.curr_dir, path);
	exec_commands("siblprev", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir, path));
}

TEST(sibl_skips_files_and_can_work_without_sorting)
{
	char path[PATH_MAX + 1];
	char *const saved_cwd = save_cwd();

	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "a", cwd);

	assert_success(os_mkdir(SANDBOX_PATH "/a", 0700));
	create_file(SANDBOX_PATH "/b");

	lwin.sort_g[0] = SK_NONE;

	strcpy(lwin.curr_dir, path);
	exec_commands("siblnext!", &lwin, CIT_COMMAND);
	assert_true(paths_are_same(lwin.curr_dir, path));

	restore_cwd(saved_cwd);

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(unlink(SANDBOX_PATH "/b"));
}

TEST(sibl_uses_global_sort_option)
{
	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "rename", cwd);

	lwin.sort[0] = -SK_BY_NAME;

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);

	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, path));
}

TEST(sibl_respects_dot_filter)
{
	char path[PATH_MAX + 1];

	lwin.hide_dot = 1;

	assert_success(os_mkdir(SANDBOX_PATH "/.a", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/b", 0700));

	make_abs_path(path, sizeof(path), SANDBOX_PATH, "b", cwd);

	strcpy(lwin.curr_dir, path);
	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, path));

	strcpy(lwin.curr_dir, path);
	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(rmdir(SANDBOX_PATH "/.a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));

	lwin.hide_dot = 0;
}

TEST(sibl_respects_name_filters)
{
	char path[PATH_MAX + 1];

	(void)replace_matcher(&lwin.manual_filter, "a");
	(void)filter_set(&lwin.auto_filter, "c");

	assert_success(os_mkdir(SANDBOX_PATH "/a", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/b", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/c", 0700));

	make_abs_path(path, sizeof(path), SANDBOX_PATH, "b", cwd);

	strcpy(lwin.curr_dir, path);
	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, path));

	strcpy(lwin.curr_dir, path);
	assert_success(exec_commands("siblprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));
	assert_success(rmdir(SANDBOX_PATH "/c"));
}

TEST(sibl_works_inside_filtered_out_directory)
{
	char path[PATH_MAX + 1];

	lwin.hide_dot = 1;

	assert_success(os_mkdir(SANDBOX_PATH "/a", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/.b", 0700));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, ".b", cwd);
	assert_success(exec_commands("siblnext", &lwin, CIT_COMMAND));
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "a", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_success(chdir(cwd));

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/.b"));

	lwin.hide_dot = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
