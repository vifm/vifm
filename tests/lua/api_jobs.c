#include <stic.h>

#include <unistd.h> /* usleep() */

#include "../../src/engine/var.h"
#include "../../src/engine/variables.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"
#include "../../src/background.h"

#include <test-utils.h>

static void wait_for_job(void);

static vlua_t *vlua;

SETUP()
{
	conf_setup();
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
	conf_teardown();
}

TEST(vifmjob_bad_arg)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "info = { cmd = 'echo ignored',"
	                                     "         iomode = 'u' }\n"
	                                     "job = vifm.startjob(info)"));
	assert_true(ends_with(ui_sb_last(), "Unknown 'iomode' value: u"));
}

/* This test comes before other good startjob tests to make it pass faster.
 * When it's first, there are no other jobs which can slow down receiving errors
 * from the process. */
TEST(vifmjob_errors)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "job:wait()\n"
	                                     "while #job:errors() == 0 do end\n"
	                                     "print(job:errors())"));
	assert_true(starts_with_lit(ui_sb_last(), "err"));

	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo out' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:errors())"));
	assert_string_equal("", ui_sb_last());
}

TEST(vifm_startjob)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "job = vifm.startjob({ cmd = 'echo' })\n"
	                                     "job:stdout():lines()()\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("0", ui_sb_last());
}

TEST(vifmjob_exitcode)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = {\n"
	                                     "  cmd = 'exit 41',\n"
	                                     "  visible = true,\n"
	                                     "  description = 'exit 41'\n"
	                                     "}\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("41", ui_sb_last());
}

TEST(vifmjob_stdin, IF(have_cat))
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'cat > " SANDBOX_PATH "/file', iomode = 'w' }\n"
	      "job = vifm.startjob(info)\n"
	      "if job:stdin() ~= job:stdin() then\n"
	      "  print('Result should be the same')\n"
	      "else\n"
	      "  print(job:stdin():write('text') == job:stdin())\n"
	      "end\n"
	      "job:stdin():close()\n"
	      "job:wait()"));
	assert_string_equal("true", ui_sb_last());

	const char *lines[] = { "text" };
	file_is(SANDBOX_PATH "/file", lines, 1);
	remove_file(SANDBOX_PATH "/file");
}

TEST(vifmjob_stdin_broken_pipe, IF(not_windows))
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'no-such-command-exists', iomode = 'w' }"
	      "job = vifm.startjob(info)"
	      "vifm.startjob({ cmd = 'sleep 0.01' }):wait()"
	      "print(job:stdin():write('text') == job:stdin())"
	      "job:stdin():close()"
	      "job:wait()"));
	assert_string_equal("true", ui_sb_last());

	/* Broken pipe + likely dead parent VifmJob object. */
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'no-such-command-exists', iomode = 'w' }"
	      "stdin = vifm.startjob(info):stdin()"
	      "vifm.startjob({ cmd = 'sleep 0.01' }):wait()"
	      "print(stdin:write('text') == stdin)"));
	assert_string_equal("true", ui_sb_last());
	bg_check();
	assert_failure(vlua_run_string(vlua, "print(stdin:write('text') == stdin)"));
	assert_true(ends_with(ui_sb_last(), ": attempt to use a closed file"));
}

TEST(vifmjob_stdout)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo out' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "if job:stdout() ~= job:stdout() then\n"
	                                     "  print('Result should be the same')\n"
	                                     "else\n"
	                                     "  print(job:stdout():read('a'))\n"
	                                     "end"));
	assert_true(starts_with_lit(ui_sb_last(), "out"));
}

TEST(vifmjob_stderr)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout():read('a'))"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2',"
	                                     "         mergestreams = true }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout():read('a'))"));
	assert_true(starts_with_lit(ui_sb_last(), "err"));
}

TEST(vifmjob_no_out)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "info = { cmd = 'echo ignored',"
	                                     "         iomode = '' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout() and 'FAIL')"));
	assert_true(ends_with(ui_sb_last(), "The job has no output stream"));
}

TEST(vifmjob_no_in)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "info = { cmd = 'echo ignored',"
	                                     "         iomode = '' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdin() and 'FAIL')"));
	assert_true(ends_with(ui_sb_last(), "The job has no input stream"));
}

TEST(vifmjob_onexit_good)
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'echo hi',"
	              " onexit = function(job) print(job:exitcode()) end }"
	      "vifm.startjob(info)"));

	wait_for_job();
	vlua_process_callbacks(vlua);

	assert_string_equal("0", ui_sb_last());
}

TEST(vifmjob_onexit_bad)
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'echo hi',"
	              " onexit = function(job) fail_here() end }"
	      "vifm.startjob(info)"));

	wait_for_job();
	vlua_process_callbacks(vlua);

	assert_true(ends_with(ui_sb_last(),
				": attempt to call a nil value (global 'fail_here')"));
}

static void
wait_for_job(void)
{
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

	/* When the job is marked as not running, the callback might not yet been
	 * dispatched, so call bg_check() once again to be sure. */
	bg_check();

	assert_int_equal(0, job->exit_code);
	bg_job_decref(job);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
