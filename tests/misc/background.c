#include <stic.h>

#include <unistd.h> /* chdir() usleep() */

#include <stdio.h> /* FILE fclose() fputs() */

#include <test-utils.h>

#include "../../src/compat/pthread.h"
#include "../../src/engine/var.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/cancellation.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/ui/ui.h"
#include "../../src/background.h"
#include "../../src/signals.h"
#include "../../src/status.h"

static void task(bg_op_t *bg_op, void *arg);
static void wait_until_locked(pthread_spinlock_t *lock);

SETUP_ONCE()
{
	setup_signals();
}

SETUP()
{
	/* curr_view shouldn't be NULL, because of iteration over tabs before doing
	 * exec(). */
	curr_view = &lwin;

	conf_setup();
}

TEARDOWN()
{
	conf_teardown();

	curr_view = NULL;
}

/* This test is the first one to make it pass faster.  When it's first, there
 * are no other jobs which can slow down receiving errors from the process. */
TEST(capture_error_of_external_command)
{
	bg_job_t *job = bg_run_external_job("echo there 1>&2", BJF_CAPTURE_OUT);
	assert_non_null(job);
	assert_non_null(job->output);

	int nlines;
	char **lines = read_stream_lines(job->output, &nlines, 0, NULL, NULL);
	assert_int_equal(0, nlines);
	free_string_array(lines, nlines);

	while(1)
	{
		pthread_spin_lock(&job->errors_lock);
		if(job->errors == NULL)
		{
			pthread_spin_unlock(&job->errors_lock);
			usleep(5000);
			continue;
		}
		assert_true(starts_with_lit(job->errors, "there"));
		pthread_spin_unlock(&job->errors_lock);
		break;
	}

	assert_success(bg_job_wait(job));
	assert_int_equal(0, job->exit_code);

	bg_job_decref(job);
}

TEST(provide_input_to_external_command_no_job, IF(have_cat))
{
	assert_success(chdir(SANDBOX_PATH));

	FILE *input;
	assert_success(bg_run_external("cat > file", 1, SHELL_BY_USER, &input));
	assert_non_null(input);

	fputs("input", input);
	fclose(input);

	wait_for_all_bg();

	const char *lines[] = { "input" };
	file_is("file", lines, ARRAY_LEN(lines));

	remove_file("file");
}

TEST(jobcount_variable_gets_updated)
{
	(void)stats_update_fetch();

	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	pthread_spinlock_t locks[2];
	pthread_spin_init(&locks[0], PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&locks[1], PTHREAD_PROCESS_PRIVATE);

	assert_int_equal(0, var_to_int(getvar("v:jobcount")));
	assert_false(stats_redraw_planned());

	assert_success(bg_execute("", "", 0, 0, &task, (void *)locks));

	wait_until_locked(&locks[0]);
	bg_check();

	assert_int_equal(1, var_to_int(getvar("v:jobcount")));
	assert_true(stats_redraw_planned());

	(void)stats_update_fetch();
	bg_check();

	assert_int_equal(1, var_to_int(getvar("v:jobcount")));
	assert_false(stats_redraw_planned());

	pthread_spin_lock(&locks[1]);
	pthread_spin_lock(&locks[0]);
	pthread_spin_unlock(&locks[0]);
	pthread_spin_unlock(&locks[1]);
	pthread_spin_destroy(&locks[0]);
	pthread_spin_destroy(&locks[1]);
}

TEST(job_can_survive_on_its_own)
{
	assert_success(bg_run_external("exit 71", 1, SHELL_BY_APP, NULL));

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
	assert_success(bg_run_external("exit 99", 1, SHELL_BY_APP, NULL));

	bg_job_t *job = bg_jobs;
	assert_non_null(job);

	bg_job_incref(job);

	assert_success(bg_job_wait(job));
	assert_int_equal(99, job->exit_code);

	bg_job_decref(job);
}

TEST(create_a_job_explicitly)
{
	bg_job_t *job = bg_run_external_job("exit 5", BJF_CAPTURE_OUT);
	assert_non_null(job);

	assert_success(bg_job_wait(job));
	assert_int_equal(5, job->exit_code);

	bg_job_decref(job);
}

TEST(capture_output_of_external_command)
{
	bg_job_t *job = bg_run_external_job("echo there", BJF_CAPTURE_OUT);
	assert_non_null(job);
	assert_non_null(job->output);

	int nlines;
	char **lines = read_stream_lines(job->output, &nlines, 0, NULL, NULL);
	assert_int_equal(1, nlines);
	assert_string_equal("there", lines[0]);
	free_string_array(lines, nlines);

	assert_success(bg_job_wait(job));
	assert_int_equal(0, job->exit_code);

	bg_job_decref(job);
}

TEST(supply_input_to_external_command, IF(have_cat))
{
	bg_job_t *job = bg_run_external_job("cat",
			BJF_CAPTURE_OUT | BJF_SUPPLY_INPUT);
	assert_non_null(job);
	assert_non_null(job->input);
	assert_non_null(job->output);

	fputs("1\n", job->input);
	fputs("2 2\n", job->input);
	fputs(" 3  3   3  ", job->input);
	fclose(job->input);
	job->input = NULL;

	int nlines;
	char **lines = read_stream_lines(job->output, &nlines, 0, NULL, NULL);
	assert_int_equal(3, nlines);
	assert_string_equal("1", lines[0]);
	assert_string_equal("2 2", lines[1]);
	assert_string_equal(" 3  3   3  ", lines[2]);
	free_string_array(lines, nlines);

	assert_success(bg_job_wait(job));
	assert_int_equal(0, job->exit_code);

	bg_job_decref(job);
}

TEST(background_redirects_streams_properly, IF(not_windows))
{
	assert_success(bg_and_wait_for_errors("echo a", &no_cancellation));
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
/* vim: set cinoptions+=t0 filetype=c : */
