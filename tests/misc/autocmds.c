#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/engine/autocmds.h"
#include "../../src/utils/env.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

#include "utils.h"

static int not_windows(void);

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

TEST(no_args_lists_elements)
{
	assert_failure(exec_commands("autocmd", &lwin, CIT_COMMAND));
}

TEST(addition_start)
{
	assert_failure(exec_commands("autocmd * '" SANDBOX_PATH "' let $a = 1", &lwin,
				CIT_COMMAND));
}

TEST(addition_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(addition_no_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, TEST_DATA_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_exact_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! DirEnter '" SANDBOX_PATH "'", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_event_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! DirEnter", &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_path_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! * '" SANDBOX_PATH "'", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_all)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("autocmd DirEnter '" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd!", &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_too_many_args)
{
	assert_failure(exec_commands("autocmd! a b c", &lwin, CIT_COMMAND));
}

TEST(extra_slash_is_fine)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter '" SANDBOX_PATH "/' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(error_on_wrong_event_name)
{
	assert_failure(exec_commands("autocmd some /path let $a = 1", &lwin,
				CIT_COMMAND));
}

TEST(envvars_are_expanded)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	env_set("dir", SANDBOX_PATH);
	assert_success(exec_commands("autocmd DirEnter $dir let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(pattern_negation)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter '!" SANDBOX_PATH "' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, TEST_DATA_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(tail_pattern)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter existing-files let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, TEST_DATA_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, TEST_DATA_PATH "/existing-files/")
			>= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(multiple_patterns_addition)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter '" SANDBOX_PATH ",ab' let $a = 1",
				&lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(multiple_patterns_removal)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands(
				"auto DirEnter '" SANDBOX_PATH "," TEST_DATA_PATH "' let $a = 1", &lwin,
				CIT_COMMAND));
	assert_success(exec_commands(
				"auto! DirEnter '" SANDBOX_PATH "," TEST_DATA_PATH "'", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_true(change_directory(curr_view, TEST_DATA_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(multiple_patterns_correct_expansion)
{
	/* Each pattern should be expanded on its own, not all pattern string should
	 * be expanded and then broken into patterns. */

	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));
	assert_success(exec_commands("let $c = ','", &lwin, CIT_COMMAND));

	assert_success(exec_commands(
				"auto DirEnter '" SANDBOX_PATH "$c" TEST_DATA_PATH "' let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	assert_string_equal("x", env_get("a"));
}

/* Windows has various limitations on characters used in file names. */
TEST(tilde_is_expanded_after_negation, IF(not_windows))
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(os_mkdir(SANDBOX_PATH"/~", 0700));

	assert_success(exec_commands("auto DirEnter !~ let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, SANDBOX_PATH"/~") >= 0);
	assert_string_equal("1", env_get("a"));

	assert_success(rmdir(SANDBOX_PATH"/~"));
}

static int
not_windows(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
