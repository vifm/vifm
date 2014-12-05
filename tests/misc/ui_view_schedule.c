#include "seatest.h"

#include "../../src/ui/ui.h"

static FileView *const view = &lwin;

static void
setup(void)
{
	(void)ui_view_query_scheduled_event(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

static void
test_schedule_redraw_sets_redrawn(void)
{
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_REDRAW);
}

static void
test_schedule_reload_sets_reload(void)
{
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

static void
test_scheduling_full_reload_sets_full_reload(void)
{
	ui_view_schedule_full_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
}

static void
test_query_resets_redraw_event(void)
{
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_REDRAW);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

static void
test_query_resets_reload_event(void)
{
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

static void
test_query_resets_full_reload_event(void)
{
	ui_view_schedule_full_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

static void
test_redraw_request_does_not_reset_reload(void)
{
	ui_view_schedule_reload(view);
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

static void
test_reload_does_not_reset_full_reload(void)
{
	ui_view_schedule_full_reload(view);
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
}

static void
test_redraw_does_not_reset_full_reload(void)
{
	ui_view_schedule_full_reload(view);
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
}

static void
test_reload_resets_redraw(void)
{
	ui_view_schedule_redraw(view);
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

static void
test_full_reload_resets_redraw(void)
{
	ui_view_schedule_redraw(view);
	ui_view_schedule_full_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
}

static void
test_full_reload_resets_reload(void)
{
	ui_view_schedule_reload(view);
	ui_view_schedule_full_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_FULL_RELOAD);
}

void
ui_view_schedule_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_schedule_redraw_sets_redrawn);
	run_test(test_schedule_reload_sets_reload);
	run_test(test_scheduling_full_reload_sets_full_reload);

	run_test(test_query_resets_redraw_event);
	run_test(test_query_resets_reload_event);
	run_test(test_query_resets_full_reload_event);

	run_test(test_redraw_request_does_not_reset_reload);
	run_test(test_reload_does_not_reset_full_reload);
	run_test(test_redraw_does_not_reset_full_reload);

	run_test(test_reload_resets_redraw);
	run_test(test_full_reload_resets_redraw);
	run_test(test_full_reload_resets_reload);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
