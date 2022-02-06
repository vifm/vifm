#include <stic.h>

#include <unistd.h> /* chdir() */

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"

static char sandbox[PATH_MAX + 1];
static char test_data[PATH_MAX + 1];

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	conf_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();

	conf_teardown();
}

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

TEST(cds_does_the_replacement)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds syntax-highlight rename", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
}

TEST(cds_aborts_on_broken_)
{
	char dst[PATH_MAX + 1];
	snprintf(dst, sizeof(dst), "%s/rename", test_data);

	assert_success(chdir(dst));

	strcpy(lwin.curr_dir, dst);

	assert_failure(exec_commands("cds/rename/read/t", &lwin, CIT_COMMAND));

	assert_string_equal(dst, lwin.curr_dir);
}

TEST(cds_acts_like_substitute)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds/SYNtax-?hi[a-z]*/rename/i", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
}

TEST(cds_can_change_path_of_both_panes)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds! syntax-highlight rename", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
	assert_string_equal(path, rwin.curr_dir);
}

TEST(cds_is_noop_when_pattern_not_found)
{
	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);
	strcpy(rwin.curr_dir, sandbox);

	assert_failure(exec_commands("cds asdlfkjasdlkfj rename", &lwin,
				CIT_COMMAND));

	assert_string_equal(test_data, lwin.curr_dir);
	assert_string_equal(sandbox, rwin.curr_dir);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
