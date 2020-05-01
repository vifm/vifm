#include <stic.h>

#include <stdlib.h> /* free() */
#include <wchar.h> /* wcsdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/hist.h"
#include "../../src/utils/str.h"
#include "../../src/fops_common.h"
#include "../../src/status.h"

static line_stats_t stats;

SETUP_ONCE()
{
	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(10);
	cfg_resize_histories(0);

	cfg_resize_histories(10);
}

TEARDOWN_ONCE()
{
	cfg_resize_histories(0);
}

SETUP()
{
	curr_view = &lwin;
	view_setup(&lwin);
	init_view_list(&lwin);
	update_string(&lwin.dir_entry[0].name, "fake");
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", NULL);
	conf_setup();
	init_modes();
	fops_init(&modcline_prompt, NULL);

	stats.line = NULL;
	stats.index = 0;
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = HIST_NONE;
	stats.line_buf = NULL;
}

TEARDOWN()
{
	free(stats.line);

	fops_init(NULL, NULL);
	conf_teardown();
	vle_keys_reset();
	view_teardown(&lwin);
}

TEST(entry_matching_input_is_skipped)
{
	hist_t hist;
	assert_success(hist_init(&hist, 10U));

	assert_success(hist_add(&hist, "older", -1));
	assert_success(hist_add(&hist, "newer", -1));

	stats.line = wcsdup(L"newer");
	stats.history_search = HIST_GO;
	hist_prev(&stats, &hist, 10U);
	assert_wstring_equal(L"older", stats.line);

	hist_reset(&hist);
}

TEST(entering_and_leaving_via_the_same_mapping_skips_cmdline_history)
{
	vle_keys_user_add(L"x", L":cmd" WK_CR, NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"x")));
	assert_true(hist_is_empty(&curr_stats.cmd_hist));

	vle_keys_user_add(L"x", L"/spattern" WK_CR, NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"x")));
	assert_false(hist_is_empty(&curr_stats.search_hist));

	vle_keys_user_add(L"x", L"=fpattern" WK_CR, NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"x")));
	assert_false(hist_is_empty(&curr_stats.filter_hist));

	vle_keys_user_add(L"x", L"cwname" WK_CR, NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"x")));
	assert_false(hist_is_empty(&curr_stats.prompt_hist));
}

TEST(just_entering_via_a_mapping_does_not_skip_cmdline_history)
{
	vle_keys_user_add(L";", L":", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L";")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"cmd" WK_CR)));
	assert_false(hist_is_empty(&curr_stats.cmd_hist));
}

TEST(just_leaving_via_a_mapping_does_not_skip_cmdline_history)
{
	vle_keys_user_add(L"x", WK_CR, CMDLINE_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L":cmdx")));
	assert_false(hist_is_empty(&curr_stats.cmd_hist));
}

TEST(entering_and_leaving_via_different_mappings_does_not_skip_cmdline_history)
{
	vle_keys_user_add(L";", L":", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"x", WK_CR, CMDLINE_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L";cmdx")));
	assert_false(hist_is_empty(&curr_stats.cmd_hist));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
