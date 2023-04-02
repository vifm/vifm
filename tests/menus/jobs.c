#include <stic.h>

#include <unistd.h> /* usleep() */

#include <test-utils.h>

#include "../../src/compat/pthread.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/background.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

static void task(bg_op_t *bg_op, void *arg);
static void wait_until_locked(pthread_spinlock_t *lock);

static pthread_spinlock_t locks[2];

SETUP_ONCE()
{
	pthread_spin_init(&locks[0], PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&locks[1], PTHREAD_PROCESS_PRIVATE);
}

TEARDOWN_ONCE()
{
	pthread_spin_destroy(&locks[0]);
	pthread_spin_destroy(&locks[1]);
}

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	/* The redraw code updates 'columns' and 'lines'. */
	opt_handlers_setup();
	modes_init();
	cmds_init();

	curr_stats.load_stage = -1;

	assert_success(bg_execute("job", "", 0, 0, &task, (void *)locks));
	wait_until_locked(&locks[0]);

	assert_success(cmds_dispatch("jobs", &lwin, CIT_COMMAND));
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_keys_reset();
	opt_handlers_teardown();

	view_teardown(&lwin);

	curr_stats.load_stage = 0;

	pthread_spin_lock(&locks[1]);
	pthread_spin_lock(&locks[0]);
	pthread_spin_unlock(&locks[0]);
	pthread_spin_unlock(&locks[1]);
}

TEST(jobs_are_listed)
{
	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("1/0       job", menu_get_current()->items[0]);
}

TEST(dd_press)
{
	(void)vle_keys_exec(WK_d WK_d);
	assert_string_equal("1/0       (cancelling...) job",
			menu_get_current()->items[0]);
}

TEST(r_press)
{
	bg_job_t *job = bg_jobs;
	replace_string(&job->cmd, "job_updated");
	(void)vle_keys_exec(WK_r);
	assert_string_equal("1/0       job_updated", menu_get_current()->items[0]);
}

TEST(e_press_without_errors)
{
	(void)vle_keys_exec(WK_e);
	assert_string_equal("1/0       job", menu_get_current()->items[0]);
}

TEST(e_press_with_errors)
{
	bg_job_t *job = bg_jobs;

	(void)vle_keys_exec(WK_q);
	assert_success(cmds_dispatch("jobs", &lwin, CIT_COMMAND));

	pthread_spin_lock(&job->errors_lock);
	(void)strappend(&job->errors, &job->errors_len, "this\nis\nerror");
	pthread_spin_unlock(&job->errors_lock);

	(void)vle_keys_exec(WK_e);
	assert_int_equal(3, menu_get_current()->len);
	assert_string_equal("this", menu_get_current()->items[0]);
	assert_string_equal("is", menu_get_current()->items[1]);
	assert_string_equal("error", menu_get_current()->items[2]);

	(void)vle_keys_exec(WK_h);
	assert_int_equal(1, menu_get_current()->len);
}

static void
task(bg_op_t *bg_op, void *arg)
{
	pthread_spinlock_t *locks = arg;
	pthread_spin_lock(&locks[0]);
	wait_until_locked(&locks[1]);
	pthread_spin_unlock(&locks[0]);
}

static void
wait_until_locked(pthread_spinlock_t *lock)
{
	while(pthread_spin_trylock(lock) == 0)
	{
		usleep(5000);
		pthread_spin_unlock(lock);
		usleep(5000);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
