#include <stic.h>

#include <limits.h> /* INT_MIN */

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
	assert_int_equal(UT_NONE, stats_update_fetch());
	stats_redraw_later();
	assert_int_equal(UT_REDRAW, stats_update_fetch());
	assert_int_equal(UT_NONE, stats_update_fetch());
}

TEST(stats_update_term_state_operates_correctly)
{
	assert_int_equal(TS_NORMAL, curr_stats.term_state);
	stats_update_term_state(5, 5);
	assert_int_equal(TS_TOO_SMALL, curr_stats.term_state);
	stats_update_term_state(50, 50);
	assert_int_equal(TS_BACK_TO_NORMAL, curr_stats.term_state);
}

TEST(changing_splitter_position_schedules_redraw)
{
	assert_int_equal(UT_NONE, stats_update_fetch());
	stats_set_splitter_pos(10);
	assert_int_equal(UT_REDRAW, stats_update_fetch());
	stats_set_splitter_pos(10);
	assert_int_equal(UT_NONE, stats_update_fetch());
	stats_set_splitter_pos(-1);
	assert_int_equal(UT_REDRAW, stats_update_fetch());
}

TEST(negative_one_splitter_ratio)
{
	curr_stats.splitter_pos = -1;
	curr_stats.splitter_ratio = 0;

	stats_set_splitter_ratio(-1);

	assert_int_equal(-1, curr_stats.splitter_pos);
	assert_true(curr_stats.splitter_ratio == 0.5);
}

TEST(setting_splitter_ratio_to_half_updates_position_accordingly)
{
	cfg.lines = 20;
	cfg.columns = 20;

	assert_int_equal(UT_NONE, stats_update_fetch());

	stats_set_splitter_pos(5);
	assert_true(curr_stats.splitter_ratio == 0.25);
	assert_int_equal(UT_REDRAW, stats_update_fetch());

	stats_set_splitter_ratio(0.5);
	assert_int_equal(-1, curr_stats.splitter_pos);
	assert_int_equal(UT_REDRAW, stats_update_fetch());

	cfg.lines = INT_MIN;
	cfg.columns = INT_MIN;
}

TEST(setting_splitter_ratio_updates_position_and_schedules_redraw)
{
	cfg.lines = 20;
	cfg.columns = 20;

	assert_int_equal(UT_NONE, stats_update_fetch());
	stats_set_splitter_ratio(0.75);

	assert_int_equal(15, curr_stats.splitter_pos);
	assert_true(curr_stats.splitter_ratio == 0.75);
	assert_int_equal(UT_REDRAW, stats_update_fetch());

	stats_set_splitter_ratio(0.75);
	assert_int_equal(UT_NONE, stats_update_fetch());

	cfg.lines = INT_MIN;
	cfg.columns = INT_MIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
