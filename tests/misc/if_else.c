#include "seatest.h"

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/commands.h"

#define VAR_A "VAR_A"
#define VAR_B "VAR_B"
#define VAR_C "VAR_C"
#define VAR_D "VAR_D"

static void
remove_tmp_vars(void)
{
	env_remove(VAR_A);
	env_remove(VAR_B);
	env_remove(VAR_C);
	env_remove(VAR_D);
}

static void
setup(void)
{
	init_commands();
	lwin.selected_files = 0;
	remove_tmp_vars();
}

static void
test_if_without_else_true_condition(void)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
}

static void
test_if_without_else_false_condition(void)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

static void
test_if_with_else_true_condition(void)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

static void
test_if_with_else_false_condition(void)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
}

static void
test_if_true_if_true_condition(void)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " |     if 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     endif"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
	assert_string_equal(VAR_B, env_get(VAR_B));
	assert_string_equal(VAR_C, env_get(VAR_C));
}

static void
test_if_true_if_false_condition(void)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " |     if 2 == 0"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     endif"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(VAR_C, env_get(VAR_C));
}

static void
test_if_false_if_true_condition(void)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " |     if 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     endif"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
}

static void
test_if_false_else_if_true_condition(void)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | else"
	                             " |     let $"VAR_B" = '"VAR_B"'"
	                             " |     if 2 == 2"
	                             " |         let $"VAR_C" = '"VAR_C"'"
	                             " |     endif"
	                             " |     let $"VAR_D" = '"VAR_D"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(VAR_B, env_get(VAR_B));
	assert_string_equal(VAR_C, env_get(VAR_C));
	assert_string_equal(VAR_D, env_get(VAR_D));
}

static void
test_if_false_if_else_condition(void)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " |     if 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     else"
	                             " |         let $"VAR_C" = '"VAR_C"'"
	                             " |     endif"
	                             " |     let $"VAR_D" = '"VAR_D"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

static void
test_if_true_else_if_else_condition(void)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " |     if 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     else"
	                             " |         let $"VAR_C" = '"VAR_C"'"
	                             " |     endif"
	                             " |     let $"VAR_D" = '"VAR_D"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

void
if_else_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(remove_tmp_vars);

	run_test(test_if_without_else_true_condition);
	run_test(test_if_without_else_false_condition);
	run_test(test_if_with_else_true_condition);
	run_test(test_if_with_else_false_condition);
	run_test(test_if_true_if_true_condition);
	run_test(test_if_true_if_false_condition);
	run_test(test_if_false_if_true_condition);
	run_test(test_if_false_else_if_true_condition);
	run_test(test_if_false_if_else_condition);
	run_test(test_if_true_else_if_else_condition);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
