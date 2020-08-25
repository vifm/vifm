#include <stic.h>

#include "../../src/ui/cancellation.h"

TEST(generic)
{
	assert_false(ui_cancellation_requested());

	ui_cancellation_push_on();
	ui_cancellation_request();
	ui_cancellation_disable();

	assert_true(ui_cancellation_requested());
	ui_cancellation_pop();
	assert_false(ui_cancellation_requested());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
