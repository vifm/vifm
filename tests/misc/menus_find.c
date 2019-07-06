#include <stic.h>

#include <unistd.h> /* chdir() */

#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"

#include "utils.h"

static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();

	init_commands();
}

TEARDOWN()
{
	opt_handlers_teardown();

	vle_cmds_reset();
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
	assert_failure(exec_commands("find", &lwin, CIT_COMMAND));

	assert_success(exec_commands("find a", &lwin, CIT_COMMAND));
	assert_int_equal(3, lwin.list_rows);

	assert_success(exec_commands("find . -name aaa", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.list_rows);

	assert_success(exec_commands("find -name '*.vifm'", &lwin, CIT_COMMAND));
	assert_int_equal(9, lwin.list_rows);

	view_teardown(&lwin);
	view_setup(&lwin);

	/* Repeat last search. */
	strcpy(lwin.curr_dir, test_data);
	assert_success(exec_commands("find", &lwin, CIT_COMMAND));
	assert_int_equal(9, lwin.list_rows);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
