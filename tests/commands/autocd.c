#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/engine/cmds.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/path.h"
#include "../../src/cmd_core.h"

static char test_data[PATH_MAX + 1];
static char read_path[PATH_MAX + 1];

SETUP_ONCE()
{
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", NULL);
	make_abs_path(read_path, sizeof(read_path), TEST_DATA_PATH, "read", NULL);
}

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	conf_setup();
	view_setup(&lwin);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);

	init_commands();

	cfg.auto_cd = 1;
}

TEARDOWN()
{
	curr_view = NULL;
	other_view = NULL;

	conf_teardown();
	view_teardown(&lwin);

	vle_cmds_reset();

	cfg.auto_cd = 0;
}

TEST(existing_dir)
{
	assert_success(exec_command("read", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, read_path));
}

TEST(parent_dir)
{
	assert_success(exec_command("read", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, read_path));

	assert_success(exec_command("..", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, test_data));

	assert_success(exec_command("read", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, read_path));
}

TEST(ambiguity_and_error)
{
	assert_failure(exec_command("compare/a", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, test_data));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
