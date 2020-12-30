#include <stic.h>

#include <test-utils.h>

#include "../../src/ui/ui.h"

static view_t *const view = &lwin;

SETUP()
{
	view_setup(&lwin);

	(void)ui_view_query_scheduled_event(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

TEARDOWN()
{
	view_teardown(&lwin);
}

TEST(schedule_redraw_sets_redrawn)
{
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_REDRAW);
}

TEST(schedule_reload_sets_reload)
{
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

TEST(query_resets_redraw_event)
{
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_REDRAW);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

TEST(query_resets_reload_event)
{
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
	assert_true(ui_view_query_scheduled_event(view) == UUE_NONE);
}

TEST(redraw_request_does_not_reset_reload)
{
	ui_view_schedule_reload(view);
	ui_view_schedule_redraw(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

TEST(reload_resets_redraw)
{
	ui_view_schedule_redraw(view);
	ui_view_schedule_reload(view);
	assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
}

TEST(multiple_requests_do_not_cancel_each_other)
{
	int i;
	for(i = 0; i < 10; ++i)
	{
		ui_view_schedule_redraw(view);
		ui_view_schedule_redraw(view);
		assert_true(ui_view_query_scheduled_event(view) == UUE_REDRAW);

		ui_view_schedule_reload(view);
		ui_view_schedule_reload(view);
		assert_true(ui_view_query_scheduled_event(view) == UUE_RELOAD);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
