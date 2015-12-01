#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/cfg/config.h"
#include "../../src/engine/autocmds.h"
#include "../../src/utils/env.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

#include "utils.h"

SETUP()
{
	curr_view = &lwin;

	update_string(&cfg.slow_fs_list, "");

	init_commands();

	view_setup(&lwin);
}

TEARDOWN()
{
	view_teardown(&lwin);

	update_string(&cfg.slow_fs_list, NULL);

	vle_aucmd_remove(NULL, NULL);
}

TEST(no_args_fail)
{
	assert_failure(exec_commands("autocmd", &lwin, CIT_COMMAND));
}

TEST(addition_match)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(addition_no_match)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, TEST_DATA_PATH) >= 0);
	assert_string_equal("", env_get("a"));
}

TEST(remove_exact_match)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! DirEnter '" SANDBOX_PATH "'", &lwin,
				CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("", env_get("a"));
}

TEST(remove_event_match)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! DirEnter", &lwin, CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("", env_get("a"));
}

TEST(remove_path_match)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! * '" SANDBOX_PATH "'", &lwin,
				CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("", env_get("a"));
}

TEST(remove_all)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd!", &lwin, CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("", env_get("a"));
}

TEST(remove_too_many_args)
{
	assert_failure(exec_commands("autocmd! a b c", &lwin, CIT_COMMAND));
}

TEST(extra_slash_is_fine)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter '" SANDBOX_PATH "/' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(envvars_are_expanded)
{
	assert_success(exec_commands("let $a = ''", &lwin, CIT_COMMAND));

	env_set("path", SANDBOX_PATH);
	assert_success(exec_commands("autocmd DirEnter $path let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
