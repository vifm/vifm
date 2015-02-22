#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/utils/env.h"
#include "../../src/builtin_functions.h"
#include "../parsing/asserts.h"

static void
test_executable_true_for_executable(void)
{
	ASSERT_INT_OK("executable('bin/misc')", 1);
}

static void
test_executable_false_for_regular_file(void)
{
	ASSERT_INT_OK("executable('Makefile')", 0);
}

static void
test_executable_false_for_dir(void)
{
	ASSERT_INT_OK("executable('.')", 0);
}

static void
test_expand_expands_environment_variables(void)
{
	env_set("OPEN_ME", "Found something interesting?");
	ASSERT_OK("expand('$OPEN_ME')", "Found something interesting?");
}

static void
test_system_catches_stdout(void)
{
	ASSERT_OK("system('echo a')", "a");
}

static void
test_system_catches_stderr(void)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a 1>&2')", "a");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a 1>&2')", "a ");
#endif
}

static void
test_system_catches_stdout_and_err(void)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a && echo b 1>&2')", "a\nb");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a && echo b 1>&2')", "a \nb ");
#endif
}

void
builtin_functions_tests(void)
{
	test_fixture_start();

	cfg.shell = strdup("sh");
	init_builtin_functions();

	run_test(test_executable_true_for_executable);
	run_test(test_executable_false_for_regular_file);
	run_test(test_executable_false_for_dir);

	run_test(test_expand_expands_environment_variables);

	run_test(test_system_catches_stdout);
	run_test(test_system_catches_stderr);
	run_test(test_system_catches_stdout_and_err);

	function_reset_all();
	free(cfg.shell);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
