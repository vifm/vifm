#include <stic.h>

#include <unistd.h> /* usleep() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/var.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/cancellation.h"
#include "../../src/utils/str.h"
#include "../../src/ui/ui.h"
#include "../../src/background.h"

static void task(bg_op_t *bg_op, void *arg);

SETUP()
{
	/* curr_view shouldn't be NULL, because of iteration over tabs before doing
	 * exec(). */
	curr_view = &lwin;
}

TEARDOWN()
{
	curr_view = NULL;
}

TEST(jobcount_variable_gets_updated)
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	assert_success(bg_execute("", "", 0, 0, &task, NULL));

	assert_int_equal(0, var_to_int(getvar("v:jobcount")));
	assert_false(stats_redraw_planned());

	bg_check();

	assert_int_equal(1, var_to_int(getvar("v:jobcount")));
	assert_true(stats_redraw_planned());

	(void)stats_update_fetch();
	bg_check();

	assert_int_equal(1, var_to_int(getvar("v:jobcount")));
	assert_false(stats_redraw_planned());
}

TEST(background_redirects_streams_properly, IF(not_windows))
{
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
	assert_success(bg_and_wait_for_errors("echo a", &no_cancellation));
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
}

static void
task(bg_op_t *bg_op, void *arg)
{
	usleep(5000);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
