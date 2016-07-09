#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() remove() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/args.h"
#include "../../src/builtin_functions.h"
#include "../../src/filelist.h"
#include "../../src/status.h"
#include "../parsing/asserts.h"

#include "utils.h"

SETUP()
{
	update_string(&cfg.shell, "sh");

	init_builtin_functions();
	init_parser(NULL);

	view_setup(&lwin);

	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
}

TEARDOWN()
{
	function_reset_all();
	update_string(&cfg.shell, NULL);

	view_teardown(&lwin);
}

TEST(executable_true_for_executable)
{
	const char *const exec_file = SANDBOX_PATH "/exec-for-completion" EXE_SUFFIX;

	create_executable(exec_file);

	ASSERT_INT_OK(
			"executable('" SANDBOX_PATH "/exec-for-completion" EXE_SUFFIX "')", 1);

	assert_success(remove(exec_file));
}

TEST(executable_false_for_regular_file)
{
	ASSERT_INT_OK("executable('Makefile')", 0);
}

TEST(executable_false_for_dir)
{
	ASSERT_INT_OK("executable('.')", 0);
}

TEST(expand_expands_environment_variables)
{
	env_set("OPEN_ME", "Found something interesting?");
	ASSERT_OK("expand('$OPEN_ME')", "Found something interesting?");
}

TEST(system_catches_stdout)
{
	ASSERT_OK("system('echo a')", "a");
}

TEST(system_catches_stderr)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a 1>&2')", "a");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a 1>&2')", "a ");
#endif
}

TEST(system_catches_stdout_and_err)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a && echo b 1>&2')", "a\nb");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a && echo b 1>&2')", "a \nb ");
#endif
}

TEST(layoutis_is_correct_for_single_pane)
{
	curr_stats.number_of_windows = 1;
	curr_stats.split = VSPLIT;
	curr_stats.split = HSPLIT;
	ASSERT_OK("layoutis('only')", "1");
	ASSERT_OK("layoutis('split')", "0");
	ASSERT_OK("layoutis('hsplit')", "0");
	ASSERT_OK("layoutis('vsplit')", "0");
}

TEST(layoutis_is_correct_for_hsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = HSPLIT;
	ASSERT_OK("layoutis('only')", "0");
	ASSERT_OK("layoutis('split')", "1");
	ASSERT_OK("layoutis('hsplit')", "1");
	ASSERT_OK("layoutis('vsplit')", "0");
}

TEST(layoutis_is_correct_for_vsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;
	ASSERT_OK("layoutis('only')", "0");
	ASSERT_OK("layoutis('split')", "1");
	ASSERT_OK("layoutis('hsplit')", "0");
	ASSERT_OK("layoutis('vsplit')", "1");
}

TEST(paneisat_is_correct_for_single_pane)
{
	curr_stats.number_of_windows = 1;
	curr_stats.split = VSPLIT;
	curr_stats.split = HSPLIT;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(paneisat_is_correct_for_hsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = HSPLIT;

	curr_view = &lwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "0");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");

	curr_view = &rwin;
	ASSERT_OK("paneisat('top')", "0");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(paneisat_is_correct_for_vsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;

	curr_view = &lwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "0");

	curr_view = &rwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "0");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(getpanetype_for_regular_view)
{
	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "regular");
}

TEST(getpanetype_for_custom_view)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "custom");
}

TEST(getpanetype_for_very_custom_view)
{
	opt_handlers_setup();

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 1) == 0);

	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "very-custom");

	opt_handlers_teardown();
}

TEST(chooseopt_options_are_not_set)
{
	args_t args = { };
	char *argv[] = { "vifm", NULL };

	assert_success(init_status(&cfg));

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	args_process(&args, 0);
	args_process(&args, 1);

	ASSERT_OK("chooseopt('files')", "");
	ASSERT_OK("chooseopt('dir')", "");
	ASSERT_OK("chooseopt('cmd')", "");
	ASSERT_OK("chooseopt('delimiter')", "\n");

	assert_success(reset_status(&cfg));

	args_free(&args);
}

TEST(chooseopt_options_are_set)
{
	args_t args = { };
	char *argv[] = { "vifm",
	                 "--choose-files", "files-file",
	                 "--choose-dir", "dir-file",
	                 "--on-choose", "cmd",
	                 "--delimiter", "delim",
	                 NULL };

	assert_success(init_status(&cfg));

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	args_process(&args, 0);
	args_process(&args, 1);

	ASSERT_OK("chooseopt('files')", "/files-file");
	ASSERT_OK("chooseopt('dir')", "/dir-file");
	ASSERT_OK("chooseopt('cmd')", "cmd");
	ASSERT_OK("chooseopt('delimiter')", "delim");

	assert_success(reset_status(&cfg));

	args_free(&args);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
