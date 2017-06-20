#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/matcher.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
}

SETUP()
{
	char *error;

	init_modes();
	enter_cmdline_mode(CLS_COMMAND, "", NULL);

	curr_view = &lwin;
	assert_non_null(curr_view->manual_filter =
			matcher_alloc("{filt}", 0, 0, "", &error));
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_C_c);

	matcher_free(curr_view->manual_filter);
	curr_view->manual_filter = NULL;

	vle_keys_reset();
}

TEST(value_of_manual_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_m);

	assert_wstring_equal(L"filt", stats->line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
