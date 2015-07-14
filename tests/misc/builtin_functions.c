#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/utils/env.h"
#include "../../src/builtin_functions.h"
#include "../../src/status.h"
#include "../parsing/asserts.h"

SETUP()
{
	cfg.shell = strdup("sh");
	init_builtin_functions();
	init_parser(NULL);
}

TEARDOWN()
{
	function_reset_all();
	free(cfg.shell);
	cfg.shell = NULL;
}

TEST(executable_true_for_executable)
{
	ASSERT_INT_OK("executable('../src/vifm')", 1);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
