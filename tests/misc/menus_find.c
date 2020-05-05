#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdio.h> /* snprintf() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/cmd_handlers.h"

static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	init_modes();

	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();

	init_commands();

	curr_stats.load_stage = -1;
}

TEARDOWN()
{
	opt_handlers_teardown();

	vle_cmds_reset();
	vle_keys_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);

	curr_stats.load_stage = 0;
}

TEST(find_command, IF(not_windows))
{
	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");

	assert_success(chdir(TEST_DATA_PATH));
	strcpy(lwin.curr_dir, test_data);

	assert_success(exec_commands("set findprg='find %s %a %u'", &lwin,
				CIT_COMMAND));

	/* Nothing to repeat. */
	cmds_drop_state();
	assert_failure(exec_commands("find", &lwin, CIT_COMMAND));

	assert_success(exec_commands("find a", &lwin, CIT_COMMAND));
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("Find a", lwin.custom.title);

	assert_success(exec_commands("find . -name aaa", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("Find . -name aaa", lwin.custom.title);

	assert_success(exec_commands("find -name '*.vifm'", &lwin, CIT_COMMAND));
	assert_int_equal(11, lwin.list_rows);
	assert_string_equal("Find -name '*.vifm'", lwin.custom.title);

	view_teardown(&lwin);
	view_setup(&lwin);

	/* Repeat last search. */
	strcpy(lwin.curr_dir, test_data);
	assert_success(exec_commands("find", &lwin, CIT_COMMAND));
	assert_int_equal(11, lwin.list_rows);
}

TEST(enter_navigates_to_found_file, IF(not_windows))
{
	assert_success(exec_commands("set findprg='find %s %a'", &lwin, CIT_COMMAND));

	assert_success(chdir(TEST_DATA_PATH));
	strcpy(lwin.curr_dir, test_data);

	assert_success(exec_commands("find dir1", &lwin, CIT_COMMAND));

	char dst[PATH_MAX + 1];
	snprintf(dst, sizeof(dst), "%s/tree", test_data);

	(void)vle_keys_exec(WK_CR);
	assert_true(paths_are_equal(lwin.curr_dir, dst));
	(void)vle_keys_exec(WK_ESC);
}

TEST(p_macro_works, IF(not_windows))
{
	assert_success(exec_commands("set findprg='find %s -name %p'", &lwin,
				CIT_COMMAND));

	assert_success(chdir(TEST_DATA_PATH));
	strcpy(lwin.curr_dir, test_data);

	assert_failure(exec_commands("find a$NO_SUCH_VAR", &lwin, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
