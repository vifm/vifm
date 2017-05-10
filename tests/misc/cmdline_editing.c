#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
}

SETUP()
{
	init_modes();
	enter_cmdline_mode(CLS_COMMAND, "", NULL);

	curr_view = &lwin;
	filter_init(&curr_view->manual_filter, 0);
	filter_set(&curr_view->manual_filter, "filt");
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_C_c);

	filter_dispose(&curr_view->manual_filter);

	vle_keys_reset();
}

TEST(value_of_manual_filter_is_pasted)
{
	(void)vle_keys_exec_timed_out(WK_C_x WK_m);

	assert_wstring_equal(L"filt", stats->line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
