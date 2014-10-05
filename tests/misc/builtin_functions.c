#include <stdlib.h> /* free() */
#include <string.h>

#include "seatest.h"

#include "../parsing/asserts.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/builtin_functions.h"

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

void
builtin_functions_tests(void)
{
	test_fixture_start();

	init_builtin_functions();

	run_test(test_executable_true_for_executable);
	run_test(test_executable_false_for_regular_file);
	run_test(test_executable_false_for_dir);

	function_reset_all();

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
