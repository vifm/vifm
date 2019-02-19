#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

SETUP()
{
	update_string(&cfg.shell, "");
	stats_init(&cfg);
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
}

TEST(stats_silence_ui_operates_correctly)
{
	assert_false(stats_silenced_ui());
	stats_silence_ui(1);
	assert_true(stats_silenced_ui());
	stats_silence_ui(1);
	assert_true(stats_silenced_ui());
	stats_silence_ui(0);
	assert_true(stats_silenced_ui());
	stats_silence_ui(0);
	assert_false(stats_silenced_ui());
}

TEST(redraw_flag_resets_on_query)
{
	assert_false(stats_redraw_fetch());
	stats_redraw_schedule();
	assert_true(stats_redraw_fetch());
	assert_false(stats_redraw_fetch());
}

TEST(stats_update_term_state_operates_correctly)
{
	assert_int_equal(TS_NORMAL, curr_stats.term_state);
	stats_update_term_state(5, 5);
	assert_int_equal(TS_TOO_SMALL, curr_stats.term_state);
	stats_update_term_state(50, 50);
	assert_int_equal(TS_BACK_TO_NORMAL, curr_stats.term_state);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
