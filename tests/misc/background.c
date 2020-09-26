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
#include "../../src/signals.h"
#include "../../src/status.h"

static void task(bg_op_t *bg_op, void *arg);

SETUP_ONCE()
{
	setup_signals();
}

SETUP()
{
	/* curr_view shouldn't be NULL, because of iteration over tabs before doing
	 * exec(). */
	curr_view = &lwin;

#ifndef _WIN32
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
#else
	update_string(&cfg.shell, "cmd");
	update_string(&cfg.shell_cmd_flag, "/C");
	stats_update_shell_type(cfg.shell);
#endif
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
	stats_update_shell_type("/bin/sh");

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

TEST(job_can_survive_on_its_own)
{
	assert_success(bg_run_external("exit 71", 1, SHELL_BY_APP));

	bg_job_t *job = bg_jobs;
	assert_non_null(job);

	bg_job_incref(job);

	int counter = 0;
	while(bg_job_is_running(job))
	{
		usleep(5000);
		bg_check();
		if(++counter > 100)
		{
			assert_fail("Waiting for too long.");
			return;
		}
	}

	assert_int_equal(71, job->exit_code);
	bg_job_decref(job);
}

TEST(explicitly_wait_for_a_job)
{
	assert_success(bg_run_external("exit 99", 1, SHELL_BY_APP));

	bg_job_t *job = bg_jobs;
	assert_non_null(job);

	bg_job_incref(job);

	assert_success(bg_job_wait(job));
	assert_int_equal(99, job->exit_code);

	bg_job_decref(job);
}

TEST(background_redirects_streams_properly, IF(not_windows))
{
	assert_success(bg_and_wait_for_errors("echo a", &no_cancellation));
}

static void
task(bg_op_t *bg_op, void *arg)
{
	usleep(5000);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
