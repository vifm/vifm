#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/cmd_core.h"

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

SETUP()
{
	init_commands();
	lwin.selected_files = 0;
	remove_tmp_vars();
	curr_view = &lwin;
}

TEARDOWN()
{
	curr_view = NULL;
}

TEST(if_without_else_true_condition)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
}

TEST(if_without_else_false_condition)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

TEST(if_with_else_true_condition)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

TEST(if_with_else_false_condition)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
}

TEST(if_true_if_true_condition)
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

TEST(if_true_if_false_condition)
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

TEST(if_false_if_true_condition)
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

TEST(if_false_else_if_true_condition)
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

TEST(if_false_if_else_condition)
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

TEST(if_true_else_if_else_condition)
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

TEST(if_true_elseif_else_condition)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | elseif 2 == \"   \""
	                             " |     let $"VAR_B" = '"VAR_B"'"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

TEST(if_false_elseif_true_else_condition)
{
	const char *const COMMANDS = " | if 1 == \"87  \""
	                             " | elseif 2 == 2"
	                             " |     let $"VAR_B" = '"VAR_B"'"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(VAR_B, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

TEST(if_false_elseif_false_else_condition)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " | elseif 2 == 1"
	                             " |     let $"VAR_B" = '"VAR_B"'"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(VAR_C, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

TEST(nested_passive_elseif)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     if 2 == 1"
	                             " |     elseif 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     endif"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(NULL, env_get(VAR_B));
	assert_string_equal(VAR_C, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

TEST(nested_active_elseif)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     if 2 == 1"
	                             " |     elseif 2 == 2"
	                             " |         let $"VAR_B" = '"VAR_B"'"
	                             " |     endif"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | endif";

	assert_int_equal(0, exec_commands(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
	assert_string_equal(VAR_B, env_get(VAR_B));
	assert_string_equal(NULL, env_get(VAR_C));
	assert_string_equal(NULL, env_get(VAR_D));
}

TEST(else_before_elseif)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " |     let $"VAR_C" = '"VAR_C"'"
	                             " | elseif 2 == 2"
	                             " | endif";

	assert_failure(exec_commands(COMMANDS, &lwin, CIT_COMMAND));
}

TEST(multiple_else_branches)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " | else"
	                             " | endif";

	assert_failure(exec_commands(COMMANDS, &lwin, CIT_COMMAND));
}

TEST(sourcing_in_body)
{
	const char *const CMDS = " | if 1 == 1"
	                         " |     source "TEST_DATA_PATH"/scripts/set-env.vifm"
	                         " | endif";

	assert_int_equal(0, exec_commands(CMDS, &lwin, CIT_COMMAND));
}

TEST(wrong_expr_causes_hard_error)
{
	assert_true(exec_commands("if $USER == root", &lwin, CIT_COMMAND) < 0);
}

TEST(misplaced_else_causes_hard_error)
{
	assert_true(exec_commands("else", &lwin, CIT_COMMAND) < 0);
}

TEST(misplaced_endif_causes_hard_error)
{
	assert_true(exec_commands("endif", &lwin, CIT_COMMAND) < 0);
}

TEST(finish_inside_if_statement_causes_no_missing_endif_error)
{
	const char *const CMDS =
		" | if 1 == 1"
		" |     source "TEST_DATA_PATH"/scripts/finish-inside-if.vifm"
		" |     source "TEST_DATA_PATH"/scripts/finish-inside-ifs.vifm"
		" | endif";

	assert_int_equal(0, exec_commands(CMDS, &lwin, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
