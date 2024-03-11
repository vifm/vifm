#include <stic.h>

#include <unistd.h> /* usleep() */

#include "../../src/engine/var.h"
#include "../../src/engine/variables.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/background.h"

#include <test-utils.h>

#include "asserts.h"

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
	BLUA_ENDS(vlua, "Unknown 'iomode' value: u",
						"job = vifm.startjob { cmd = 'echo ignored', iomode = 'u' }");
}

/* This test comes before other good startjob tests to make it pass faster.
 * When it's first, there are no other jobs which can slow down receiving errors
 * from the process. */
TEST(vifmjob_errors)
{
	GLUA_STARTS(vlua, "err",
			"job = vifm.startjob { cmd = 'echo err 1>&2' }"
			"job:wait()"
			"print(job:errors())");

	GLUA_EQ(vlua, "",
			"job = vifm.startjob { cmd = 'echo out' }"
			"print(job:errors())");
}

TEST(vifm_startjob)
{
	GLUA_EQ(vlua, "0",
			"job = vifm.startjob { cmd = 'echo' }"
			"job:stdout():lines()()"
			"print(job:exitcode())");
}

TEST(vifmjob_exitcode)
{
	GLUA_EQ(vlua, "41",
			"info = {"
			"  cmd = 'exit 41',"
			"  visible = true,"
			"  description = 'exit 41'"
			"}"
			"job = vifm.startjob(info)"
			"print(job:exitcode())");
}

TEST(vifmjob_stdin, IF(have_cat))
{
	GLUA_EQ(vlua, "true",
			"info = { cmd = 'cat > " SANDBOX_PATH "/file', iomode = 'w' }"
			"job = vifm.startjob(info)"
			"if job:stdin() ~= job:stdin() then"
			"  print('Result should be the same')"
			"else"
			"  print(job:stdin():write('text') == job:stdin())"
			"end\n"
			"job:stdin():close()"
			"job:wait()");

	const char *lines[] = { "text" };
	file_is(SANDBOX_PATH "/file", lines, 1);
	remove_file(SANDBOX_PATH "/file");
}

TEST(vifmjob_stdin_broken_pipe, IF(not_windows))
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	GLUA_EQ(vlua, "true",
			"job = vifm.startjob { cmd = 'no-such-command-exists', iomode = 'w' }"
			"vifm.startjob({ cmd = 'sleep 0.01' }):wait()"
			"print(job:stdin():write('text') == job:stdin())"
			"job:stdin():close()"
			"job:wait()");

	/* Broken pipe + likely dead parent VifmJob object. */
	GLUA_EQ(vlua, "true",
			"info = { cmd = 'no-such-command-exists', iomode = 'w' }"
			"stdin = vifm.startjob(info):stdin()"
			"vifm.startjob({ cmd = 'sleep 0.01' }):wait()"
			"print(stdin:write('text') == stdin)");
	bg_check();
	BLUA_ENDS(vlua, ": attempt to use a closed file",
			"print(stdin:write('text') == stdin)");
}

TEST(vifmjob_stdout)
{
	GLUA_STARTS(vlua, "out",
			"job = vifm.startjob { cmd = 'echo out' }"
			"if job:stdout() ~= job:stdout() then"
			"  print('Result should be the same')"
			"else"
			"  print(job:stdout():read('a'))"
			"end");
}

TEST(vifmjob_stderr)
{
	GLUA_EQ(vlua, "",
			"job = vifm.startjob { cmd = 'echo err 1>&2' }"
			"print(job:stdout():read('a'))");
	GLUA_STARTS(vlua, "err",
			"job = vifm.startjob { cmd = 'echo err 1>&2', mergestreams = true }"
			"print(job:stdout():read('a'))");
}

TEST(vifmjob_no_out)
{
	BLUA_ENDS(vlua, "The job has no output stream",
			"job = vifm.startjob { cmd = 'echo ignored', iomode = '' }"
			"print(job:stdout() and 'FAIL')");
}

TEST(vifmjob_no_in)
{
	BLUA_ENDS(vlua, "The job has no input stream",
			"job = vifm.startjob { cmd = 'echo ignored', iomode = '' }"
			"print(job:stdin() and 'FAIL')");
}

TEST(vifmjob_onexit_good)
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	GLUA_EQ(vlua, "",
			"info = { cmd = 'echo hi',"
			"         onexit = function(job) print(job:exitcode()) end }"
			"vifm.startjob(info)");

	wait_for_job();
	vlua_process_callbacks(vlua);

	assert_string_equal("0", ui_sb_last());
}

TEST(vifmjob_onexit_bad)
{
	var_t var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);

	GLUA_EQ(vlua, "",
			"info = { cmd = 'echo hi',"
			"         onexit = function(job) fail_here() end }"
			"vifm.startjob(info)");

	wait_for_job();
	vlua_process_callbacks(vlua);

	assert_string_ends_with(": attempt to call a nil value (global 'fail_here')",
			ui_sb_last());
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
