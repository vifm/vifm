#include <stic.h>

#include <unistd.h> /* F_OK access() chdir() */

#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/commands.h"
#include "../../src/filelist.h"
#include "../../src/ops.h"
#include "../../src/undo.h"

static int builtin_cmd(const cmd_info_t* cmd_info);
static int exec_func(OPS op, void *data, const char *src, const char *dst);
static int op_avail(OPS op);

static const cmd_add_t commands[] = {
	{ .name = "builtin", .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 0, .max_args = 0, .bg = 1, },
	{ .name = "onearg",  .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 1, .max_args = 1, .bg = 0, },
};

static void free_view(FileView *view);

static int called;
static int bg;
static char *arg;

SETUP()
{
	static int max_undo_levels = 0;

	filter_init(&lwin.local_filter.filter, 1);
	filter_init(&lwin.manual_filter, 1);
	filter_init(&lwin.auto_filter, 1);

	filter_init(&rwin.local_filter.filter, 1);
	filter_init(&rwin.manual_filter, 1);
	filter_init(&rwin.auto_filter, 1);

	cfg.cd_path = strdup("");
	cfg.fuse_home = strdup("");
	cfg.slow_fs_list = strdup("");

	lwin.selected_files = 0;
	lwin.dir_entry = NULL;
	lwin.list_rows = 0;

	rwin.dir_entry = NULL;
	rwin.list_rows = 0;

	curr_view = &lwin;

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

	free_view(&lwin);
	free_view(&rwin);

	reset_cmds();
	reset_undo_list();
}

static void
free_view(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free_dir_entry(view, &view->dir_entry[i]);
	}
	dynarray_free(view->dir_entry);

	for(i = 0; i < view->custom.entry_count; ++i)
	{
		free_dir_entry(view, &view->custom.entries[i]);
	}
	dynarray_free(view->custom.entries);

	filter_dispose(&view->local_filter.filter);
	filter_dispose(&view->manual_filter);
	filter_dispose(&view->auto_filter);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
