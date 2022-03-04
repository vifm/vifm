#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
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
	conf_setup();
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

	conf_teardown();
}

TEST(vifm_startjob)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "job = vifm.startjob({ cmd = 'echo' })\n"
	                                     "job:stdout():lines()()\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("0", ui_sb_last());

	conf_teardown();
}

TEST(vifmjob_exitcode)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = {\n"
	                                     "  cmd = 'exit 41',\n"
	                                     "  visible = true,\n"
	                                     "  description = 'exit 41'\n"
	                                     "}\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("41", ui_sb_last());

	conf_teardown();
}

TEST(vifmjob_stdin, IF(have_cat))
{
	conf_setup();

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

	conf_teardown();

	const char *lines[] = { "text" };
	file_is(SANDBOX_PATH "/file", lines, 1);
	remove_file(SANDBOX_PATH "/file");
}

TEST(vifmjob_stdin_broken_pipe, IF(not_windows))
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
	      "info = { cmd = 'no-such-command-exists', iomode = 'w' }"
	      "job = vifm.startjob(info)"
	      "vifm.startjob({ cmd = 'sleep 0.01' }):wait()"
	      "print(job:stdin():write('text') == job:stdin())"
	      "job:stdin():close()"
	      "job:wait()"));
	assert_string_equal("true", ui_sb_last());

	conf_teardown();
}

TEST(vifmjob_stdout)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo out' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "if job:stdout() ~= job:stdout() then\n"
	                                     "  print('Result should be the same')\n"
	                                     "else\n"
	                                     "  print(job:stdout():read('a'))\n"
	                                     "end"));
	assert_true(starts_with_lit(ui_sb_last(), "out"));

	conf_teardown();
}

TEST(vifmjob_stderr)
{
	conf_setup();

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

	conf_teardown();
}

TEST(vifmjob_no_out)
{
	conf_setup();

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "info = { cmd = 'echo ignored',"
	                                     "         iomode = '' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout() and 'FAIL')"));
	assert_true(ends_with(ui_sb_last(), "The job has no output stream"));

	conf_teardown();
}

TEST(vifmjob_no_in)
{
	conf_setup();

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "info = { cmd = 'echo ignored',"
	                                     "         iomode = '' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdin() and 'FAIL')"));
	assert_true(ends_with(ui_sb_last(), "The job has no input stream"));

	conf_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
