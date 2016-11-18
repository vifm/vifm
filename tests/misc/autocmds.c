#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* snprintf() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/autocmds.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

#include "utils.h"

static char sandbox[PATH_MAX];
static char test_data[PATH_MAX];
static char cmd[PATH_MAX];

SETUP_ONCE()
{
	char cwd[PATH_MAX];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	if(is_path_absolute(SANDBOX_PATH))
	{
		snprintf(sandbox, sizeof(sandbox), "%s", SANDBOX_PATH);
	}
	else
	{
		snprintf(sandbox, sizeof(sandbox), "%s/%s", cwd, SANDBOX_PATH);
	}

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(test_data, sizeof(test_data), "%s", TEST_DATA_PATH);
	}
	else
	{
		snprintf(test_data, sizeof(test_data), "%s/%s", cwd, TEST_DATA_PATH);
	}
}

SETUP()
{
	curr_view = &lwin;

	update_string(&cfg.slow_fs_list, "");

	init_commands();
	opt_handlers_setup();

	view_setup(&lwin);
}

TEARDOWN()
{
	view_teardown(&lwin);

	opt_handlers_teardown();

	update_string(&cfg.slow_fs_list, NULL);

	vle_aucmd_remove(NULL, NULL);
}

TEST(no_args_lists_elements)
{
	assert_failure(exec_commands("autocmd", &lwin, CIT_COMMAND));
}

TEST(addition_start)
{
	snprintf(cmd, sizeof(cmd), "autocmd * '%s' let $a = 1", sandbox);
	assert_failure(exec_commands(cmd, &lwin, CIT_COMMAND));
}

TEST(addition_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(autocmd_is_whole_line_command)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1 | let $a = 2",
			sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("2", env_get("a"));
}

TEST(addition_no_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, test_data) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_exact_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	snprintf(cmd, sizeof(cmd), "autocmd! DirEnter '%s'", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_event_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd! DirEnter", &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_path_match)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	snprintf(cmd, sizeof(cmd), "autocmd! * '%s'", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_all)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "autocmd DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd!", &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(remove_too_many_args)
{
	assert_failure(exec_commands("autocmd! a b c", &lwin, CIT_COMMAND));
}

TEST(extra_slash_is_fine)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s/' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(error_on_wrong_event_name)
{
	assert_failure(exec_commands("autocmd some /path let $a = 1", &lwin,
				CIT_COMMAND));
}

TEST(envvars_are_expanded)
{
	char cmd[PATH_MAX];

	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "let $dir = '%s'", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("autocmd DirEnter $dir let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(pattern_negation)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "auto DirEnter '!%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, test_data) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(tail_pattern)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(exec_commands("auto DirEnter existing-files let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, test_data) >= 0);
	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, "existing-files") >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(multiple_patterns_addition)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s,ab' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("1", env_get("a"));
}

TEST(multiple_patterns_removal)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s,%s' let $a = 1", sandbox,
			test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	snprintf(cmd, sizeof(cmd), "auto! DirEnter '%s,%s'", sandbox, test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_true(change_directory(curr_view, test_data) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(multiple_patterns_correct_expansion)
{
	/* Each pattern should be expanded on its own, not all pattern string should
	 * be expanded and then broken into patterns. */

	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));
	assert_success(exec_commands("let $c = ','", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s$c%s' let $a = 1", sandbox,
			test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(direnter_is_not_triggered_on_leaving_custom_view_to_original_path)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_true(change_directory(curr_view, sandbox) >= 0);
	replace_string(&curr_view->custom.orig_dir, curr_view->curr_dir);
	curr_view->curr_dir[0] = '\0';

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s' let $a = 1", sandbox);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, sandbox) >= 0);
	assert_string_equal("x", env_get("a"));
}

TEST(direnter_ist_triggered_on_leaving_custom_view_to_different_path)
{
	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_true(change_directory(curr_view, sandbox) >= 0);
	replace_string(&curr_view->custom.orig_dir, curr_view->curr_dir);
	curr_view->curr_dir[0] = '\0';

	snprintf(cmd, sizeof(cmd), "auto DirEnter '%s' let $a = 1", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, test_data) >= 0);
	assert_string_equal("1", env_get("a"));
}

/* Windows has various limitations on characters used in file names. */
TEST(tilde_is_expanded_after_negation, IF(not_windows))
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/~", sandbox);

	assert_success(exec_commands("let $a = 'x'", &lwin, CIT_COMMAND));

	assert_success(os_mkdir(path, 0700));

	assert_success(exec_commands("auto DirEnter !~ let $a = 1", &lwin,
				CIT_COMMAND));

	assert_string_equal("x", env_get("a"));
	assert_true(change_directory(curr_view, path) >= 0);
	assert_string_equal("1", env_get("a"));

	assert_success(rmdir(path));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
