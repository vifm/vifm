#include <stic.h>

#include <sys/time.h> /* timeval utimes() */
#include <unistd.h> /* chdir() rmdir() symlink() */

#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() remove() */
#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/variables.h"
#include "../../src/lua/vlua.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/args.h"
#include "../../src/builtin_functions.h"
#include "../../src/compare.h"
#include "../../src/filelist.h"
#include "../../src/status.h"
#include "../parsing/asserts.h"

SETUP()
{
	update_string(&cfg.shell, "sh");
	update_string(&cfg.shell_cmd_flag, "-c");

	init_builtin_functions();
	init_parser(NULL);
	init_variables();

	view_setup(&lwin);

	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
}

TEARDOWN()
{
	clear_envvars();
	function_reset_all();
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);

	view_teardown(&lwin);
}

TEST(executable_true_for_executable)
{
	const char *const exec_file = SANDBOX_PATH "/exec-for-completion" EXE_SUFFIX;

	create_executable(exec_file);

	ASSERT_INT_OK(
			"executable('" SANDBOX_PATH "/exec-for-completion" EXE_SUFFIX "')", 1);

	assert_success(remove(exec_file));
}

TEST(executable_false_for_regular_file)
{
	ASSERT_INT_OK("executable('Makefile')", 0);
}

TEST(executable_false_for_dir)
{
	ASSERT_INT_OK("executable('.')", 0);
}

TEST(expand_expands_environment_variables)
{
	let_variables("$OPEN_ME = 'Found something interesting?'");
	ASSERT_OK("expand('$OPEN_ME')", "Found something interesting?");
}

TEST(expand_allows_escaping_dollar)
{
	let_variables("$NO_EXPAND = 'value'");
	ASSERT_OK("expand('\\$NO_EXPAND')", "$NO_EXPAND");
}

TEST(expand_escapes_paths)
{
	curr_view = &lwin;
	other_view = &rwin;

	init_view_list(&lwin);
	replace_string(&lwin.dir_entry[0].name, "f i l e");
	strcpy(lwin.curr_dir, "/dir");

	ASSERT_OK("expand('%c')", "f\\ i\\ l\\ e");
}

TEST(expand_does_not_need_double_escaping)
{
	curr_view = &lwin;
	other_view = &rwin;

	init_view_list(&lwin);
	replace_string(&lwin.dir_entry[0].name, "file");
	strcpy(lwin.curr_dir, "/dir");

	ASSERT_OK("expand('%c:p:gs!/!\\\\!')", "\\\\dir\\\\file");
}

TEST(system_catches_stdout)
{
	ASSERT_OK("system('echo a')", "a");
}

TEST(system_catches_stderr)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a 1>&2')", "a");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a 1>&2')", "a ");
#endif
}

TEST(system_catches_stdout_and_err)
{
#ifndef _WIN32
	ASSERT_OK("system('echo a && echo b 1>&2')", "a\nb");
#else
	/* "echo" implemented by cmd.exe is kinda disabled, and doesn't ignore
	 * spaces. */
	ASSERT_OK("system('echo a && echo b 1>&2')", "a \nb ");
#endif
}

TEST(term_catches_stdout)
{
	ASSERT_OK("term('echo a')", "a");
}

TEST(layoutis_is_correct_for_single_pane)
{
	curr_stats.number_of_windows = 1;
	curr_stats.split = VSPLIT;
	curr_stats.split = HSPLIT;
	ASSERT_OK("layoutis('only')", "1");
	ASSERT_OK("layoutis('split')", "0");
	ASSERT_OK("layoutis('hsplit')", "0");
	ASSERT_OK("layoutis('vsplit')", "0");
}

TEST(layoutis_is_correct_for_hsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = HSPLIT;
	ASSERT_OK("layoutis('only')", "0");
	ASSERT_OK("layoutis('split')", "1");
	ASSERT_OK("layoutis('hsplit')", "1");
	ASSERT_OK("layoutis('vsplit')", "0");
}

TEST(layoutis_is_correct_for_vsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;
	ASSERT_OK("layoutis('only')", "0");
	ASSERT_OK("layoutis('split')", "1");
	ASSERT_OK("layoutis('hsplit')", "0");
	ASSERT_OK("layoutis('vsplit')", "1");
}

TEST(paneisat_is_correct_for_single_pane)
{
	curr_stats.number_of_windows = 1;
	curr_stats.split = VSPLIT;
	curr_stats.split = HSPLIT;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(paneisat_is_correct_for_hsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = HSPLIT;

	curr_view = &lwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "0");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");

	curr_view = &rwin;
	ASSERT_OK("paneisat('top')", "0");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(paneisat_is_correct_for_vsplit)
{
	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;

	curr_view = &lwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "1");
	ASSERT_OK("paneisat('right')", "0");

	curr_view = &rwin;
	ASSERT_OK("paneisat('top')", "1");
	ASSERT_OK("paneisat('bottom')", "1");
	ASSERT_OK("paneisat('left')", "0");
	ASSERT_OK("paneisat('right')", "1");
}

TEST(getpanetype_for_regular_view)
{
	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "regular");
}

TEST(getpanetype_for_custom_view)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "custom");
}

TEST(getpanetype_for_very_custom_view)
{
	opt_handlers_setup();

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);

	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "very-custom");

	opt_handlers_teardown();
}

TEST(getpanetype_for_tree_view)
{
	flist_load_tree(&lwin, TEST_DATA_PATH, INT_MAX);

	curr_view = &lwin;
	ASSERT_OK("getpanetype()", "tree");
}

TEST(getpanetype_for_compare_view)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/rename");

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
	compare_one_pane(&lwin, CT_CONTENTS, LT_DUPS, 0);
	opt_handlers_teardown();

	ASSERT_OK("getpanetype()", "compare");
}

TEST(chooseopt_options_are_not_set)
{
	args_t args = { };
	char *argv[] = { "vifm", NULL };

	assert_success(stats_init(&cfg));

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	args_process(&args, AS_GENERAL, /*ipc=*/NULL);
	args_process(&args, AS_IPC, /*ipc=*/NULL);
	args_process(&args, AS_OTHER, /*ipc=*/NULL);

	ASSERT_OK("chooseopt('files')", "");
	ASSERT_OK("chooseopt('dir')", "");
	ASSERT_OK("chooseopt('cmd')", "");
	ASSERT_OK("chooseopt('delimiter')", "\n");

	assert_success(stats_reset(&cfg));

	args_free(&args);
}

TEST(chooseopt_options_are_set)
{
	args_t args = { };
	char *argv[] = { "vifm",
	                 "--choose-files", "files-file",
	                 "--choose-dir", "dir-file",
	                 "--on-choose", "cmd",
	                 "--delimiter", "delim",
	                 NULL };

	assert_success(stats_init(&cfg));

	args_parse(&args, ARRAY_LEN(argv) - 1U, argv, "/");
	args_process(&args, AS_GENERAL, /*ipc=*/NULL);
	args_process(&args, AS_IPC, /*ipc=*/NULL);
	args_process(&args, AS_OTHER, /*ipc=*/NULL);

	ASSERT_OK("chooseopt('files')", "/files-file");
	ASSERT_OK("chooseopt('dir')", "/dir-file");
	ASSERT_OK("chooseopt('cmd')", "cmd");
	ASSERT_OK("chooseopt('delimiter')", "delim");

	assert_success(stats_reset(&cfg));

	args_free(&args);
}

TEST(tabpagenr)
{
	ASSERT_OK("tabpagenr()", "1");
	ASSERT_OK("tabpagenr('$')", "1");

	ASSERT_FAIL("tabpagenr(1)", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("tabpagenr('a')", PE_INVALID_EXPRESSION);
}

TEST(fnameescape)
{
	ASSERT_FAIL("fnameescape()", PE_INVALID_EXPRESSION);

	ASSERT_OK("fnameescape(' ')", "\\ ");
	ASSERT_OK("fnameescape('%')", "%%");
	ASSERT_OK("fnameescape(\"'\")", "\\'");
	ASSERT_OK("fnameescape('\"')", "\\\"");
}

TEST(filetype)
{
	opt_handlers_setup();

	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	char test_data[PATH_MAX + 1];
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/existing-files", test_data);
	assert_success(symlink(path, SANDBOX_PATH "/dir-link"));
	snprintf(path, sizeof(path), "%s/existing-files/b", test_data);
	assert_success(symlink(path, SANDBOX_PATH "/file-link"));
	snprintf(path, sizeof(path), "%s/no-such-file", test_data);
	assert_success(symlink(path, SANDBOX_PATH "/broken-link"));
#endif

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/compare");
#ifndef _WIN32
	flist_custom_add(&lwin, SANDBOX_PATH "/dir-link");
	flist_custom_add(&lwin, SANDBOX_PATH "/file-link");
	flist_custom_add(&lwin, SANDBOX_PATH "/broken-link");
#endif
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);

	curr_view = &lwin;

	ASSERT_OK("filetype('')", "");
	lwin.list_pos = 0;
	ASSERT_OK("filetype('.')", "reg");
	ASSERT_OK("filetype(1)", "reg");
	ASSERT_OK("filetype(2)", "dir");
#ifndef _WIN32
	ASSERT_OK("filetype('3')", "link");
	ASSERT_OK("filetype('4')", "link");
	ASSERT_OK("filetype('3', 1)", "dir");
	ASSERT_OK("filetype('4', 1)", "reg");
	ASSERT_OK("filetype(5, 1)", "broken");
#endif

	opt_handlers_teardown();

#ifndef _WIN32
	assert_success(remove(SANDBOX_PATH "/dir-link"));
	assert_success(remove(SANDBOX_PATH "/file-link"));
	assert_success(remove(SANDBOX_PATH "/broken-link"));
#endif
}

TEST(has)
{
#ifndef _WIN32
	ASSERT_OK("has('unix')", "1");
	ASSERT_OK("has('win')", "0");
#else
	ASSERT_OK("has('unix')", "0");
	ASSERT_OK("has('win')", "1");
#endif

	ASSERT_OK("has('anythingelse')", "0");
	ASSERT_OK("has('nix')", "0");
	ASSERT_OK("has('windows')", "0");
}

TEST(has_handler)
{
	curr_stats.vlua = vlua_init();

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info) end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'handler', handler = handler }"));

	ASSERT_OK("has('#vifmtest#nohandler')", "0");
	ASSERT_OK("has('#vifmtest#handler')", "1");

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(extcached)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", NULL);
	assert_success(chdir(lwin.curr_dir));

	/* Testing with FMT_CHANGED is problematic, because change time is not
	 * programmatically accessible for writing. */
	set_extcached_monitor_type(FMT_MODIFIED);

	create_file("file");
	assert_success(os_mkdir("dir", 0700));

	/* For directories. */
	ASSERT_OK("extcached('cache1', 'dir', 'echo 1')", "1");
	/* Unchanged. */
	ASSERT_OK("extcached('cache1', 'dir', 'echo 2')", "1");
#ifndef _WIN32
	/* Changed. */
	struct timeval tvs[2] = {};
	assert_success(utimes("dir", tvs));
	ASSERT_OK("extcached('cache1', 'dir', 'echo 3')", "3");
	/* Unchanged again. */
	ASSERT_OK("extcached('cache1', 'dir', 'echo 4')", "3");
#endif

	/* For files. */
	ASSERT_OK("extcached('cache1', 'file', 'echo 5')", "5");
	/* Unchanged. */
	ASSERT_OK("extcached('cache1', 'file', 'echo 6')", "5");
#ifndef _WIN32
	/* Changed. */
	assert_success(utimes("file", tvs));
	ASSERT_OK("extcached('cache1', 'file', 'echo 7')", "7");
	/* Unchanged again. */
	ASSERT_OK("extcached('cache1', 'file', 'echo 8')", "7");
#endif

	/* Caches don't mix. */
	ASSERT_OK("extcached('cache2', 'dir', 'echo a')", "a");
	ASSERT_OK("extcached('cache3', 'dir', 'echo b')", "b");

#ifndef _WIN32
	/* Change resets all caches. */
	tvs[0].tv_sec = 1;
	tvs[1].tv_sec = 1;
	assert_success(utimes("dir", tvs));
	ASSERT_OK("extcached('cache1', 'dir', 'echo 00')", "00");
	ASSERT_OK("extcached('cache2', 'dir', 'echo aa')", "aa");
	ASSERT_OK("extcached('cache3', 'dir', 'echo bb')", "bb");
#endif

	assert_success(remove("file"));
	assert_success(rmdir("dir"));

	set_extcached_monitor_type(FMT_CHANGED);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
