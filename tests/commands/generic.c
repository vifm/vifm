#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* F_OK access() chdir() rmdir() symlink() unlink() */

#include <locale.h> /* LC_ALL setlocale() */
#include <stdio.h> /* FILE fclose() fopen() fprintf() remove() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/ops.h"
#include "../../src/registers.h"
#include "../../src/status.h"
#include "../../src/undo.h"

static int builtin_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] = {
	{ .name = "builtin",       .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK | HAS_BG_FLAG,
	  .handler = &builtin_cmd, .min_args = 0, .max_args = 0, },
	{ .name = "onearg",        .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = 0,
	  .handler = &builtin_cmd, .min_args = 1, .max_args = 1, },
};

static int called;
static int bg;
static char *arg;

static char cwd[PATH_MAX + 1];
static char sandbox[PATH_MAX + 1];
static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	conf_setup();
	cfg.use_system_calls = 1;

	init_commands();

	vle_cmds_add(commands, ARRAY_LEN(commands));

	called = 0;

	undo_setup();
}

TEARDOWN()
{
	conf_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();

	undo_teardown();
}

static int
builtin_cmd(const cmd_info_t* cmd_info)
{
	called = 1;
	bg = cmd_info->bg;

	if(cmd_info->argc != 0)
	{
		replace_string(&arg, cmd_info->argv[0]);
	}

	return 0;
}

TEST(space_amp)
{
	assert_success(exec_commands("builtin &", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(space_amp_spaces)
{
	assert_success(exec_commands("builtin &    ", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(space_bg_bar)
{
	assert_success(exec_commands("builtin &|", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(bg_space_bar)
{
	assert_success(exec_commands("builtin& |", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(space_bg_space_bar)
{
	assert_success(exec_commands("builtin & |", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(non_printable_arg)
{
	/* \x0C is Ctrl-L. */
	assert_success(exec_commands("onearg \x0C", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_string_equal("\x0C", arg);
}

TEST(non_printable_arg_in_udf)
{
	/* \x0C is Ctrl-L. */
	assert_success(exec_commands("command udf :onearg \x0C", &lwin, CIT_COMMAND));

	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_string_equal("\x0C", arg);
}

TEST(space_last_arg_in_udf)
{
	assert_success(exec_commands("command udf :onearg \\ ", &lwin, CIT_COMMAND));

	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_string_equal(" ", arg);
}

TEST(bg_mark_with_space_in_udf)
{
	assert_success(exec_commands("command udf :builtin &", &lwin, CIT_COMMAND));

	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(bg_mark_without_space_in_udf)
{
	assert_success(exec_commands("command udf :builtin&", &lwin, CIT_COMMAND));

	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_true(called);
	assert_true(bg);
}

TEST(shell_invocation_works_in_udf)
{
	const char *const cmd = "command! udf echo a > out";
#ifndef _WIN32
	replace_string(&cfg.shell, "/bin/sh");
	replace_string(&cfg.shell_cmd_flag, "-c");
#else
	replace_string(&cfg.shell, "cmd");
	replace_string(&cfg.shell_cmd_flag, "/C");
#endif

	assert_success(chdir(SANDBOX_PATH));

	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	curr_view = &lwin;

	assert_failure(access("out", F_OK));
	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_success(access("out", F_OK));
	assert_success(unlink("out"));
}

TEST(envvars_of_commands_come_from_variables_unit)
{
	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);

	assert_false(is_root_dir(lwin.curr_dir));
	assert_success(exec_commands("let $ABCDE = '/'", &lwin, CIT_COMMAND));
	env_set("ABCDE", SANDBOX_PATH);
	assert_success(exec_commands("cd $ABCDE", &lwin, CIT_COMMAND));
	assert_true(is_root_dir(lwin.curr_dir));
}

TEST(or_operator_is_attributed_to_echo)
{
	(void)exec_commands("echo 1 || builtin", &lwin, CIT_COMMAND);
	assert_false(called);
}

TEST(bar_is_not_attributed_to_echo)
{
	(void)exec_commands("echo 1 | builtin", &lwin, CIT_COMMAND);
	assert_true(called);
}

TEST(mixed_or_operator_and_bar)
{
	(void)exec_commands("echo 1 || 0 | builtin", &lwin, CIT_COMMAND);
	assert_true(called);
}

TEST(or_operator_is_attributed_to_if)
{
	(void)exec_commands("if 0 || 0 | builtin | endif", &lwin, CIT_COMMAND);
	assert_false(called);
}

TEST(or_operator_is_attributed_to_let)
{
	(void)exec_commands("let $a = 'x'", &lwin, CIT_COMMAND);
	assert_string_equal("x", env_get("a"));
	(void)exec_commands("let $a = 0 || 1", &lwin, CIT_COMMAND);
	assert_string_equal("1", env_get("a"));
}

TEST(user_command_is_executed_in_separated_scope)
{
	assert_success(exec_commands("command cmd :if 1 > 2", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("cmd", &lwin, CIT_COMMAND));
}

TEST(usercmd_can_provide_input_to_fg_process, IF(have_cat))
{
	assert_success(chdir(SANDBOX_PATH));

	setup_grid(&lwin, 20, 2, /*init=*/1);
	replace_string(&lwin.dir_entry[0].name, "a");
	replace_string(&lwin.dir_entry[1].name, "b");

	assert_int_equal(0, exec_commands("command list cat > file %Pl", &lwin,
				CIT_COMMAND));

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	assert_int_equal(0, exec_commands("list", &lwin, CIT_COMMAND));

	const char *lines[] = { "/path/a", "/path/b" };
	file_is("file", lines, ARRAY_LEN(lines));

	remove_file("file");
}

TEST(usercmd_can_provide_input_to_bg_process, IF(have_cat))
{
	assert_success(chdir(SANDBOX_PATH));

	setup_grid(&lwin, 20, 2, /*init=*/1);
	replace_string(&lwin.dir_entry[0].name, "a");
	replace_string(&lwin.dir_entry[1].name, "b");

	assert_int_equal(0, exec_commands("command list cat > file %Pl &", &lwin,
				CIT_COMMAND));

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	assert_int_equal(0, exec_commands("list", &lwin, CIT_COMMAND));

	wait_for_all_bg();

	const char *lines[] = { "/path/a", "/path/b" };
	file_is("file", lines, ARRAY_LEN(lines));

	remove_file("file");
}

TEST(cv_is_built_by_emark)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), test_data, "", cwd);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("!echo %c %u", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));

	assert_string_equal("!echo %c %u", lwin.custom.title);
}

TEST(title_of_cv_is_limited, IF(not_windows))
{
	const char *long_cmd = "!echo                                   "
	                       "                                        "
	                       "      %c%u";
	const char *title = "!echo                                   "
	                    "                                     ...";

	assert_success(stats_init(&cfg));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), test_data, "", cwd);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands(long_cmd, &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));

	assert_string_equal(title, lwin.custom.title);
}

TEST(cv_is_built_by_usercmd)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), test_data, "", cwd);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("command cmd echo %c %u", &lwin, CIT_COMMAND));
	assert_success(exec_commands("cmd", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));

	assert_string_equal(":cmd", lwin.custom.title);
}

TEST(tree_cv_keeps_title)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), test_data, "", cwd);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("!echo %c %u", &lwin, CIT_COMMAND));
	assert_success(exec_commands("tree", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));

	assert_string_equal("!echo %c %u", lwin.custom.title);
}

TEST(put_bg_cmd_is_parsed_correctly)
{
	/* Simulate custom view to force failure of the command. */
	lwin.curr_dir[0] = '\0';

	assert_success(exec_commands("put \" &", &lwin, CIT_COMMAND));
}

TEST(conversion_failure_is_handled)
{
	assert_non_null(setlocale(LC_ALL, "C"));
	init_modes();

	/* Execution of the following commands just shouldn't crash. */
	(void)exec_commands("nnoremap \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("nnoremap \xee\x85\x8b tj", &lwin, CIT_COMMAND);
	(void)exec_commands("nnoremap tj \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("nunmap \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("unmap \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("cabbrev \xee\x85\x8b tj", &lwin, CIT_COMMAND);
	/* The next command is needed so that there will be something to list. */
	(void)exec_commands("cabbrev a b", &lwin, CIT_COMMAND);
	(void)exec_commands("cabbrev \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("cunabbrev \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("normal \xee\x85\x8b", &lwin, CIT_COMMAND);
	(void)exec_commands("wincmd \xee", &lwin, CIT_COMMAND);

	vle_keys_reset();
}

TEST(selection_is_not_reset_in_visual_mode)
{
	init_modes();

	init_view_list(&lwin);
	update_string(&lwin.dir_entry[0].name, "name");

	(void)exec_commands("if 1 == 1 | execute 'norm! ggvG' | endif", &lwin,
			CIT_COMMAND);

	assert_true(lwin.dir_entry[0].selected);
	assert_int_equal(1, lwin.selected_files);

	vle_keys_reset();
}

TEST(usercmd_range_is_as_good_as_selection)
{
	stats_init(&cfg);
	init_modes();
	regs_init();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), test_data, "", cwd);
	populate_dir_list(&lwin, 0);

	/* For gA. */

	assert_success(exec_commands("command! size :normal gA", &lwin, CIT_COMMAND));
	assert_success(exec_commands("%size", &lwin, CIT_COMMAND));
	wait_for_bg();

	assert_string_equal("color-schemes", lwin.dir_entry[0].name);
	assert_ulong_equal(107, fentry_get_size(&lwin, &lwin.dir_entry[0]));
	assert_string_equal("various-sizes", lwin.dir_entry[lwin.list_rows - 1].name);
	assert_ulong_equal(73728,
			fentry_get_size(&lwin, &lwin.dir_entry[lwin.list_rows - 1]));

	/* For zf. */

	assert_success(exec_commands("command! afilter :normal zf", &lwin,
				CIT_COMMAND));
	assert_success(exec_commands("%afilter", &lwin, CIT_COMMAND));
	populate_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);

	/* For zd. */

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_non_null(flist_custom_add(&lwin, "existing-files/b"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("command! exclude :normal zd", &lwin,
				CIT_COMMAND));
	assert_success(exec_commands("%exclude", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);

	/* For :command in :usercmd. */

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_non_null(flist_custom_add(&lwin, "existing-files/b"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	reg_t *reg = regs_find('"');

	assert_success(exec_commands("command! myyank :yank", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("%myyank", &lwin, CIT_COMMAND));
	assert_int_equal(2, reg->nfiles);

	assert_success(exec_commands("command! myyank :yank %a", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("%myyank", &lwin, CIT_COMMAND));
	assert_int_equal(2, reg->nfiles);

#ifndef _WIN32

	conf_setup();
	update_string(&cfg.shell, "/bin/sh");

	char script_path[PATH_MAX + 1];
	make_abs_path(script_path, sizeof(script_path), SANDBOX_PATH, "script", cwd);

	FILE *fp = fopen(SANDBOX_PATH "/script", "w");
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "for arg; do echo \"$arg\" >> %s/vi-list; done\n", SANDBOX_PATH);
	fclose(fp);
	assert_success(chmod(SANDBOX_PATH "/script", 0777));

	/* For l. */

	update_string(&cfg.vi_command, script_path);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_non_null(flist_custom_add(&lwin, "existing-files/b"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("command! run :normal l", &lwin, CIT_COMMAND));
	assert_success(exec_commands("%run", &lwin, CIT_COMMAND));

	const char *lines[] = { "existing-files/a", "existing-files/b" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/vi-list"));

	/* For cp. */

	update_string(&cfg.shell, "/bin/sh");

	assert_success(chdir(sandbox));
	create_file("file1");
	create_file("file2");

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), sandbox, "", cwd);
	populate_dir_list(&lwin, 0);

	assert_success(exec_commands("command! ex :normal 777cp", &lwin,
				CIT_COMMAND));
	assert_success(exec_commands("%ex", &lwin, CIT_COMMAND));

	populate_dir_list(&lwin, 1);
	assert_int_equal(FT_EXEC, lwin.dir_entry[0].type);
	assert_int_equal(FT_EXEC, lwin.dir_entry[1].type);

	assert_success(remove("file1"));
	assert_success(remove("file2"));

	/* For some :menus. */

	put_string(&cfg.find_prg, format_str("%s %%s %%A", script_path));

	assert_success(chdir(cwd));
	strcpy(lwin.curr_dir, test_data);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_non_null(flist_custom_add(&lwin, "existing-files/b"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_failure(exec_commands(".find a", &lwin, CIT_COMMAND));

	const char *find_lines[] = { "existing-files/a", "a" };
	file_is(SANDBOX_PATH "/vi-list", find_lines, ARRAY_LEN(find_lines));

	assert_success(remove(SANDBOX_PATH "/vi-list"));

	assert_success(remove(script_path));
	conf_teardown();

#endif

	regs_reset();
	vle_keys_reset();
	stats_reset(&cfg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
