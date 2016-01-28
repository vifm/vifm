#include <stic.h>

#include <unistd.h> /* F_OK access() chdir() */

#include <stdio.h> /* remove() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/modes/modes.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/env.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/ops.h"
#include "../../src/undo.h"

#include "utils.h"

static int builtin_cmd(const cmd_info_t* cmd_info);
static int exec_func(OPS op, void *data, const char *src, const char *dst);
static int op_avail(OPS op);

static const cmd_add_t commands[] = {
	{ .name = "builtin", .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 0, .max_args = 0, .bg = 1, },
	{ .name = "onearg",  .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 1, .max_args = 1, .bg = 0, },
};

static int called;
static int bg;
static char *arg;

SETUP()
{
	static int max_undo_levels = 0;

	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	cfg.cd_path = strdup("");
	cfg.fuse_home = strdup("");
	cfg.slow_fs_list = strdup("");
	cfg.use_system_calls = 1;

	init_commands();

	add_builtin_commands(commands, ARRAY_LEN(commands));

	called = 0;

	init_undo_list(&exec_func, &op_avail, NULL, &max_undo_levels);
}

TEARDOWN()
{
	update_string(&cfg.cd_path, NULL);
	update_string(&cfg.fuse_home, NULL);
	update_string(&cfg.slow_fs_list, NULL);

	view_teardown(&lwin);
	view_teardown(&rwin);

	reset_cmds();
	reset_undo_list();
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

static int
exec_func(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static int
op_avail(OPS op)
{
	return op == OP_MOVE;
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
#ifndef _WIN32
	const char *const cmd = "command! udf echo a > out";
	replace_string(&cfg.shell, "/bin/sh");
#else
	const char *const cmd = "command! udf echo a > out";
	replace_string(&cfg.shell, "cmd");
#endif

	assert_success(chdir(SANDBOX_PATH));

	stats_update_shell_type(cfg.shell);

	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	curr_view = &lwin;

	assert_failure(access("out", F_OK));
	assert_success(exec_commands("udf", &lwin, CIT_COMMAND));
	assert_success(access("out", F_OK));
	assert_success(unlink("out"));

	stats_update_shell_type("/bin/sh");

	free(cfg.shell);
	cfg.shell = NULL;
}

TEST(double_cd_uses_same_base_for_rel_paths)
{
	assert_success(chdir(TEST_DATA_PATH));

	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	strcpy(rwin.curr_dir, "..");

	assert_success(exec_commands("cd read rename", &lwin, CIT_COMMAND));

	assert_true(paths_are_equal(lwin.curr_dir, TEST_DATA_PATH "/read"));
	assert_true(paths_are_equal(rwin.curr_dir, TEST_DATA_PATH "/rename"));
}

TEST(cpmv_does_not_crash_on_wrong_list_access)
{
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	strcpy(lwin.curr_dir, TEST_DATA_PATH "/existing-files");
	strcpy(rwin.curr_dir, SANDBOX_PATH);

	lwin.list_rows = 3;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].name = strdup("c");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;

	/* cpmv used to use presence of the argument as indication of availability of
	 * file list and access memory beyond array boundaries. */
	(void)exec_commands("co .", &lwin, CIT_COMMAND);

	assert_success(remove(SANDBOX_PATH "/a"));
	assert_success(remove(SANDBOX_PATH "/b"));
	assert_success(remove(SANDBOX_PATH "/c"));
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

TEST(tr_extends_second_field)
{
	assert_success(chdir(SANDBOX_PATH));

	strcpy(lwin.curr_dir, SANDBOX_PATH);

	create_file(SANDBOX_PATH "/a b");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a b");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;

	(void)exec_commands("tr/ ?<>\\\\:*|\"/_", &lwin, CIT_COMMAND);

	assert_success(remove(SANDBOX_PATH "/a_b"));
}

TEST(put_bg_cmd_is_parsed_correctly)
{
	/* Simulate custom view to force failure of the command. */
	lwin.curr_dir[0] = '\0';

	assert_int_equal(0, exec_commands("put \" &", &lwin, CIT_COMMAND));
}

TEST(wincmd_can_switch_views)
{
	curr_view = &rwin;
	other_view = &lwin;

	init_modes();
	opt_handlers_setup();
	assert_int_equal(0, exec_commands("wincmd h", &lwin, CIT_COMMAND));
	opt_handlers_teardown();

	assert_true(curr_view == &lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
