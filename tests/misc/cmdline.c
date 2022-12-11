#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/event_loop.h"

static void prompt_callback(const char response[], void *arg);

static line_stats_t *stats;

static char *prompt_response;
static int prompt_invocation_count;

SETUP_ONCE()
{
	stats = get_line_stats();
	try_enable_utf8_locale();
	init_builtin_functions();
}

SETUP()
{
	modes_init();

	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
}

TEST(expr_reg_completion)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ex" WK_C_i);
	assert_wstring_equal(L"executable(", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(expr_reg_completion_ignores_pipe)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ab|ex" WK_C_i);
	assert_wstring_equal(L"ab|ex", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(prompt_cb_is_called_on_success)
{
	update_string(&prompt_response, NULL);
	prompt_invocation_count = 0;

	modcline_prompt("(prompt)", "initial", &prompt_callback, /*cb_arg=*/NULL,
			/*complete=*/NULL, /*allow_ee=*/0);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_int_equal(CLS_PROMPT, stats->sub_mode);
	(void)vle_keys_exec_timed_out(WK_CR);

	assert_string_equal("initial", prompt_response);
	assert_int_equal(1, prompt_invocation_count);
}

TEST(prompt_cb_is_called_on_cancellation)
{
	update_string(&prompt_response, NULL);
	prompt_invocation_count = 0;

	modcline_prompt("(prompt)", "initial", &prompt_callback, /*cb_arg=*/NULL,
			/*complete=*/NULL, /*allow_ee=*/0);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_int_equal(CLS_PROMPT, stats->sub_mode);
	(void)vle_keys_exec_timed_out(WK_C_c);

	assert_string_equal(NULL, prompt_response);
	assert_int_equal(1, prompt_invocation_count);
}

TEST(user_prompt_accepts_input)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"suffix" WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('prompt', 'input')" WK_CR);

	assert_string_equal("inputsuffix", ui_sb_last());
}

TEST(user_prompt_handles_cancellation)
{
	cfg.timeout_len = 1;
	ui_sb_msg("old");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"suffix" WK_C_c);
	(void)vle_keys_exec_timed_out(L":echo input('prompt', 'input')" WK_CR);

	assert_string_equal("", ui_sb_last());
}

TEST(user_prompt_nests)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"-" WK_CR L"*" WK_CR);
	(void)vle_keys_exec_timed_out(
			L":echo input('p2', input('p1', '1').'2')" WK_CR);

	assert_string_equal("1-2*", ui_sb_last());
}

TEST(user_prompt_and_expr_reg)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_r WK_EQUALS
			"input('n')" WK_CR
			L"nested" WK_CR
			L"extra" WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p').'out'" WK_CR);

	assert_string_equal("nestedextraout", ui_sb_last());
}

TEST(user_prompt_completion)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", NULL);

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_i WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p', 'read/dos', 'dir')" WK_CR);
	assert_string_equal("read/dos", ui_sb_last());


	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_i WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p', 'read/dos', 'file')" WK_CR);
	assert_string_equal("read/dos-eof", ui_sb_last());
}

static void
prompt_callback(const char response[], void *arg)
{
	update_string(&prompt_response, response);
	++prompt_invocation_count;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
