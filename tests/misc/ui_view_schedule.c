#include "seatest.h"

#include "../../src/ui.h"

static FileView *const view = &lwin;

static void
setup(void)
{
	ui_view_reloaded(view);
}

static void
test_schedule_redraw_sets_redrawn(void)
{
	assert_int_equal(0, ui_view_is_redraw_scheduled(view));
	ui_view_schedule_redraw(view);
	assert_int_equal(1, ui_view_is_redraw_scheduled(view));
}

static void
test_redrawn_resets_redraw(void)
{
	ui_view_schedule_redraw(view);
	assert_int_equal(1, ui_view_is_redraw_scheduled(view));

	ui_view_redrawn(view);
	assert_int_equal(0, ui_view_is_redraw_scheduled(view));
}

static void
test_redrawn_does_not_reset_reload(void)
{
	ui_view_schedule_redraw(view);
	assert_int_equal(1, ui_view_is_redraw_scheduled(view));

	ui_view_schedule_reload(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));

	ui_view_redrawn(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));
}

static void
test_schedule_reload_sets_reload(void)
{
	assert_int_equal(0, ui_view_is_reload_scheduled(view));
	ui_view_schedule_reload(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));
}

static void
test_schedule_reload_does_not_set_redraw(void)
{
	ui_view_schedule_reload(view);
	assert_int_equal(0, ui_view_is_redraw_scheduled(view));
}

static void
test_reloaded_resets_reload(void)
{
	ui_view_schedule_reload(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));

	ui_view_reloaded(view);
	assert_int_equal(0, ui_view_is_reload_scheduled(view));
}

static void
test_reloaded_resets_redraw(void)
{
	ui_view_schedule_redraw(view);
	assert_int_equal(1, ui_view_is_redraw_scheduled(view));

	ui_view_schedule_reload(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));

	ui_view_reloaded(view);
	assert_int_equal(0, ui_view_is_reload_scheduled(view));
}

static void
test_scheduling_full_reload_sets_full_reload(void)
{
	ui_view_schedule_full_reload(view);
	assert_int_equal(1, ui_view_is_full_reload_scheduled(view));
}

static void
test_scheduling_full_reload_sets_reload(void)
{
	ui_view_schedule_full_reload(view);
	assert_int_equal(1, ui_view_is_reload_scheduled(view));
}

static void
test_scheduling_reload_after_full_reload_does_not_reset_full_reload(void)
{
	assert_int_equal(0, ui_view_is_full_reload_scheduled(view));
	ui_view_schedule_full_reload(view);
	assert_int_equal(1, ui_view_is_full_reload_scheduled(view));

	ui_view_schedule_reload(view);
	assert_int_equal(1, ui_view_is_full_reload_scheduled(view));
}

void
ui_view_schedule_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_schedule_redraw_sets_redrawn);
	run_test(test_redrawn_resets_redraw);
	run_test(test_redrawn_does_not_reset_reload);

	run_test(test_schedule_reload_sets_reload);
	run_test(test_schedule_reload_does_not_set_redraw);
	run_test(test_reloaded_resets_reload);
	run_test(test_reloaded_resets_redraw);

	run_test(test_scheduling_full_reload_sets_full_reload);
	run_test(test_scheduling_full_reload_sets_reload);
	run_test(test_scheduling_reload_after_full_reload_does_not_reset_full_reload);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
