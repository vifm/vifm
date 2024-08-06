#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/engine/functions.h"
#include "../../src/engine/variables.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"

#include <test-utils.h>

#define VAR_A "VAR_A"
#define VAR_B "VAR_B"
#define VAR_C "VAR_C"
#define VAR_D "VAR_D"

static int run_cmds(const char cmds[], int scoped);

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
	cmds_init();
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(VAR_A, env_get(VAR_A));
}

TEST(if_without_else_false_condition)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

TEST(if_with_else_true_condition)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get(VAR_A));
}

TEST(if_with_else_false_condition)
{
	const char *const COMMANDS = " | if 1 == 0"
	                             " | else"
	                             " |     let $"VAR_A" = '"VAR_A"'"
	                             " | endif";

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_int_equal(0, cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
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

	assert_failure(cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
}

TEST(multiple_else_branches)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " | else"
	                             " | else"
	                             " | endif";

	assert_failure(cmds_dispatch(COMMANDS, &lwin, CIT_COMMAND));
}

TEST(sourcing_in_body)
{
	const char *const CMDS = " | if 1 == 1"
	                         " |     source "TEST_DATA_PATH"/scripts/set-env.vifm"
	                         " | endif";

	assert_int_equal(0, cmds_dispatch(CMDS, &lwin, CIT_COMMAND));
}

TEST(wrong_expr_causes_hard_error)
{
	assert_true(cmds_dispatch("if $USER == root", &lwin, CIT_COMMAND) < 0);
}

TEST(misplaced_else_causes_hard_error)
{
	assert_true(cmds_dispatch("else", &lwin, CIT_COMMAND) < 0);
}

TEST(misplaced_endif_causes_hard_error)
{
	assert_true(cmds_dispatch("endif", &lwin, CIT_COMMAND) < 0);
}

TEST(finish_inside_if_statement_causes_no_missing_endif_error)
{
	const char *const CMDS =
		" | if 1 == 1"
		" |     source "TEST_DATA_PATH"/scripts/finish-inside-if.vifm"
		" |     source "TEST_DATA_PATH"/scripts/finish-inside-ifs.vifm"
		" | endif";

	assert_int_equal(0, cmds_dispatch(CMDS, &lwin, CIT_COMMAND));
}

TEST(condition_of_inactive_elseif_is_not_evaluated, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	const char *const CMDS = " | let $REACHED = 0"
	                         ""
	                         " | if 1"
	                         " |   let $REACHED .= 1"
	                         " | elseif system('echo > temp-file') == ''"
	                         " |   let $REACHED .= 2"
	                         " | endif"
	                         " | let $REACHED .= 3"
	                         ""
	                         " | if 0"
	                         " |   let $REACHED .= 4"
	                         " | elseif 1"
	                         " |   let $REACHED .= 5"
	                         " | elseif system('echo > temp-file') == ''"
	                         " |   let $REACHED .= 6"
	                         " | endif"
	                         " | let $REACHED .= 7"
	                         ""
	                         " | if 1"
	                         " |   let $REACHED .= 8"
	                         " | else"
	                         " |   if system('echo > temp-file') == ''"
	                         " |     let $REACHED .= 9"
	                         " |   endif"
	                         " | endif"
	                         " | let $REACHED .= 'A'";

	assert_success(chdir(SANDBOX_PATH));
	conf_setup();
	init_builtin_functions();

	ui_sb_msg("");
	assert_success(run_cmds(CMDS, scoped));
	assert_string_equal("", ui_sb_last());
	assert_string_equal("013578A", local_getenv_null("REACHED"));

	no_remove_file("temp-file");
	conf_teardown();
	function_reset_all();
}

TEST(stray_elseif, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	ui_sb_msg("");
	assert_failure(run_cmds("elseif 1 == 1", scoped));
	assert_string_equal("Misplaced :elseif", ui_sb_last());
}

TEST(error_in_if_condition)
{
	const char *const CMDS = " | let $REACHED = 0"
	                         " | if level1"
	                         " |   let $REACHED .= 1"
	                         " | endif"
	                         " | let $REACHED .= 2";

	ui_sb_msg("");
	/* Unscoped use can't handle top-level errors gracefully because there is no
	 * code that handles end of scope.  This isn't a big deal because user
	 * shouldn't be able to run commands out of scope anyway. */
	assert_failure(run_cmds(CMDS, /*scoped=*/1));
	assert_string_equal("Invalid expression: level1", ui_sb_last());
	assert_string_equal("02", local_getenv_null("REACHED"));
}

TEST(error_in_inactive_elseif_condition_is_ignored, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	const char *const CMDS = " | let $REACHED = 0"
	                         " | if 1"
	                         " |   let $REACHED .= 1"
	                         " | elseif bad_elseif"
	                         " |   let $REACHED .= 2"
	                         " | endif"
	                         " | let $REACHED .= 3";

	ui_sb_msg("");
	assert_success(run_cmds(CMDS, scoped));
	assert_string_equal("", ui_sb_last());
	assert_string_equal("013", local_getenv_null("REACHED"));
}

TEST(error_in_active_elseif_condition, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	const char *const CMDS = " | let $REACHED = 0"
	                         " | if 0"
	                         " |   let $REACHED .= 1"
	                         " | elseif bad_elseif"
	                         " |   let $REACHED .= 2"
	                         " | endif"
	                         " | let $REACHED .= 3";

	ui_sb_msg("");
	assert_failure(run_cmds(CMDS, scoped));
	assert_string_equal("Invalid expression: bad_elseif", ui_sb_last());
	assert_string_equal("03", local_getenv_null("REACHED"));
}

TEST(error_in_a_nested_condition, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	const char *const CMDS = " | let $REACHED = 0"
	                         " | if 1"
	                         " |   let $REACHED .= 1"
	                         " |   if level2"
	                         " |     let $REACHED .= 2"
	                         " |   endif"
	                         " |   let $REACHED .= 3"
	                         " | endif"
	                         " | let $REACHED .= 4";

	ui_sb_msg("");
	assert_failure(run_cmds(CMDS, scoped));
	assert_string_equal("Invalid expression: level2", ui_sb_last());
	assert_string_equal("0134", local_getenv_null("REACHED"));
}

TEST(error_in_doubly_nested_condition, REPEAT(2))
{
	const int scoped = STIC_TEST_PARAM;

	const char *const CMDS = " | let $REACHED = 0"
	                         " | if 1"
	                         " |   let $REACHED .= 1"
	                         " |   if 2"
	                         " |     let $REACHED .= 2"
	                         " |     if level3"
	                         " |       let $REACHED .= 3"
	                         " |     endif"
	                         " |     let $REACHED .= 4"
	                         " |   endif"
	                         " |   let $REACHED .= 5"
	                         " | endif"
	                         " | let $REACHED .= 6";

	ui_sb_msg("");
	assert_failure(run_cmds(CMDS, scoped));
	assert_string_equal("Invalid expression: level3", ui_sb_last());
	assert_string_equal("012456", local_getenv_null("REACHED"));
}

static int
run_cmds(const char cmds[], int scoped)
{
	if(scoped)
	{
		cmds_scope_start();
	}
	int result = cmds_dispatch(cmds, &lwin, CIT_COMMAND);
	if(scoped && cmds_scope_finish() != 0 && result == 0)
	{
		result = -1;
	}
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
