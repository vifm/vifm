#include <stic.h>

#include <stdio.h> /* snprintf() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", NULL);
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	conf_setup();
	undo_setup();
	cmds_init();
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	conf_teardown();
	vle_cmds_reset();
	undo_teardown();
}

TEST(goto_navigates)
{
	assert_failure(cmds_dispatch("goto /", &lwin, CIT_COMMAND));
	assert_failure(cmds_dispatch("goto /no-such-path", &lwin, CIT_COMMAND));

	char cmd[PATH_MAX*2];
	snprintf(cmd, sizeof(cmd), "goto %s/compare", test_data);
	assert_success(cmds_dispatch(cmd, &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("compare", get_current_file_name(&lwin));

	assert_success(cmds_dispatch("goto tree", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("tree", get_current_file_name(&lwin));
}

TEST(goto_preserves_cv)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", NULL);

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "existing-files/a");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "existing-files/b");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(cmds_dispatch("goto b", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.list_pos);
	assert_true(flist_custom_active(&lwin));

	assert_success(cmds_dispatch("goto a", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.list_pos);
	assert_true(flist_custom_active(&lwin));
}

TEST(goto_preserves_tree)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "tree",
			NULL);
	assert_success(cmds_dispatch1("tree", &lwin, CIT_COMMAND));
	assert_true(cv_tree(lwin.custom.type));

	assert_success(cmds_dispatch("goto dir1/file4", &lwin, CIT_COMMAND));
	assert_int_equal(7, lwin.list_pos);
	assert_true(cv_tree(lwin.custom.type));

	assert_success(cmds_dispatch("goto dir5/file5", &lwin, CIT_COMMAND));
	assert_int_equal(10, lwin.list_pos);
	assert_true(cv_tree(lwin.custom.type));
}

TEST(goto_normalizes_slashes, IF(windows))
{
	char cmd[PATH_MAX*2];
	snprintf(cmd, sizeof(cmd), "goto %s\\\\compare", test_data);
	assert_success(cmds_dispatch(cmd, &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("compare", get_current_file_name(&lwin));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
