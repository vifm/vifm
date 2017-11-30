#include <stic.h>

#include <stdlib.h> /* free() */
#include <wchar.h> /* wcsdup() */

#include "../../src/modes/cmdline.h"
#include "../../src/utils/hist.h"

static line_stats_t stats;

SETUP()
{
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
}

TEST(entry_matching_input_is_skipped)
{
	hist_t hist;
	assert_success(hist_init(&hist, 10U));

	assert_success(hist_add(&hist, "older", 10U));
	assert_success(hist_add(&hist, "newer", 10U));

	stats.line = wcsdup(L"newer");
	stats.history_search = HIST_GO;
	hist_prev(&stats, &hist, 10U);
	assert_wstring_equal(L"older", stats.line);

	hist_reset(&hist, 10U);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
