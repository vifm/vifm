#include <stic.h>

#include <unistd.h> /* F_OK access() chdir() */

#include <stdio.h> /* remove() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/functions.h"
#include "../../src/modes/modes.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/ops.h"
#include "../../src/undo.h"

#include "utils.h"

static int builtin_cmd(const cmd_info_t* cmd_info);
static int exec_func(OPS op, void *data, const char *src, const char *dst);
static int op_avail(OPS op);
static void check_filetype(void);
static int prog_exists(const char name[]);
static void add_some_files_to_view(FileView *view);

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

static char cwd[PATH_MAX];
static char sandbox[PATH_MAX];
static char test_data[PATH_MAX];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	if(is_path_absolute(SANDBOX_PATH))
	{
		snprintf(sandbox, sizeof(sandbox), "%s", SANDBOX_PATH);
	}
	else
	{
		snprintf(sandbox, sizeof(sandbox), "%s/%s", cwd, SANDBOX_PATH);
	}

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(test_data, sizeof(test_data), "%s", TEST_DATA_PATH);
	}
	else
	{
		snprintf(test_data, sizeof(test_data), "%s/%s", cwd, TEST_DATA_PATH);
	}
}

SETUP()
{
	static int max_undo_levels = 0;

	view_setup(&lwin);
	view_setup(&rwin);

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);

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
	char path[PATH_MAX];

	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);
	strcpy(rwin.curr_dir, "..");

	assert_success(exec_commands("cd read rename", &lwin, CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/read", test_data);
	assert_true(paths_are_equal(lwin.curr_dir, path));
	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_true(paths_are_equal(rwin.curr_dir, path));
}

TEST(cpmv_does_not_crash_on_wrong_list_access)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/existing-files", test_data);

	assert_success(chdir(path));

	strcpy(lwin.curr_dir, path);
	strcpy(rwin.curr_dir, sandbox);

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

	snprintf(path, sizeof(path), "%s/a", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/b", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/c", sandbox);
	assert_success(remove(path));
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
	char path[PATH_MAX];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);

	snprintf(path, sizeof(path), "%s/a b", sandbox);
	create_file(path);

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a b");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;

	(void)exec_commands("tr/ ?<>\\\\:*|\"/_", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a_b", sandbox);
	assert_success(remove(path));
}

TEST(put_bg_cmd_is_parsed_correctly)
{
	/* Simulate custom view to force failure of the command. */
	lwin.curr_dir[0] = '\0';

	assert_int_equal(0, exec_commands("put \" &", &lwin, CIT_COMMAND));
}

TEST(wincmd_can_switch_views)
{
	init_modes();
	opt_handlers_setup();

	curr_view = &rwin;
	other_view = &lwin;
	assert_int_equal(0, exec_commands("wincmd h", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_int_equal(0, exec_commands("execute 'wincmd h'", curr_view,
				CIT_COMMAND));
	assert_true(curr_view == &lwin);

	init_builtin_functions();

	curr_view = &rwin;
	other_view = &lwin;
	assert_int_equal(0,
			exec_commands("if paneisat('left') == 0 | execute 'wincmd h' | endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_int_equal(0,
			exec_commands("if paneisat('left') == 0 "
			             "|    execute 'wincmd h' "
			             "|    let $a = paneisat('left') "
			             "|endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);
	assert_string_equal("1", env_get("a"));

	function_reset_all();

	opt_handlers_teardown();
}

TEST(yank_works_with_ranges)
{
	reg_t *reg;

	regs_init();

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	reg = regs_find(DEFAULT_REG_NAME);
	assert_non_null(reg);

	assert_int_equal(0, reg->nfiles);
	(void)exec_commands("%yank", &lwin, CIT_COMMAND);
	assert_int_equal(1, reg->nfiles);

	regs_reset();
}

TEST(filetype_accepts_negated_patterns)
{
	ft_init(&prog_exists);

	assert_success(exec_commands("filetype !{*.tar} prog", &lwin, CIT_COMMAND));

	check_filetype();

	ft_reset(0);
}

TEST(filextype_accepts_negated_patterns)
{
	ft_init(&prog_exists);
	curr_stats.exec_env_type = EET_EMULATOR_WITH_X;

	assert_success(exec_commands("filextype !{*.tar} prog", &lwin, CIT_COMMAND));

	check_filetype();

	curr_stats.exec_env_type = EET_LINUX_NATIVE;
	ft_reset(1);
}

TEST(fileviewer_accepts_negated_patterns)
{
	const char *viewer;

	ft_init(&prog_exists);

	assert_success(exec_commands("fileviewer !{*.tar} view", &lwin, CIT_COMMAND));

	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_string_equal("view", viewer);

	ft_reset(0);
}

TEST(select_fails_for_wrong_pattern)
{
	assert_failure(exec_commands("select /**/", &lwin, CIT_COMMAND));
}

TEST(select_fails_for_pattern_and_range)
{
	assert_failure(exec_commands("1,$select *.c", &lwin, CIT_COMMAND));
}

TEST(select_selects_matching_files)
{
	add_some_files_to_view(&lwin);

	assert_success(exec_commands("select *.c", &lwin, CIT_COMMAND));

	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(select_appends_matching_files_to_selection)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	assert_success(exec_commands("select *.c", &lwin, CIT_COMMAND));

	assert_int_equal(3, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(select_bang_unselects_nonmatching_files)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	assert_success(exec_commands("select! *.c", &lwin, CIT_COMMAND));

	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(select_noargs_selects_current_file)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;
	lwin.list_pos = 2;

	assert_success(exec_commands("select", &lwin, CIT_COMMAND));
	assert_int_equal(3, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	assert_success(exec_commands("select", &lwin, CIT_COMMAND));
	assert_int_equal(3, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	assert_success(exec_commands("select!", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(select_can_select_range)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;

	assert_success(exec_commands("2,$select", &lwin, CIT_COMMAND));
	assert_int_equal(3, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	assert_success(exec_commands("2,$select!", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(unselect_fails_for_wrong_pattern)
{
	assert_failure(exec_commands("unselect /**/", &lwin, CIT_COMMAND));
}

TEST(unselect_fails_for_pattern_and_range)
{
	assert_failure(exec_commands("1,$unselect *.c", &lwin, CIT_COMMAND));
}

TEST(unselect_unselects_matching_files)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	assert_success(exec_commands("unselect *.c", &lwin, CIT_COMMAND));

	assert_int_equal(1, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(unselect_noargs_unselects_current_file)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	lwin.list_pos = 2;
	assert_success(exec_commands("unselect", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);

	lwin.list_pos = 1;
	assert_success(exec_commands("unselect", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(unselect_can_unselect_range)
{
	add_some_files_to_view(&lwin);
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;

	assert_success(exec_commands("2,$unselect", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(select_and_unselect_and_pattern_ambiguity)
{
	add_some_files_to_view(&lwin);

	assert_success(exec_commands("select !{*.c}", &lwin, CIT_COMMAND));
	assert_success(exec_commands("unselect !/\\.c/", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

TEST(select_and_unselect_use_last_pattern)
{
	add_some_files_to_view(&lwin);

	cfg_resize_histories(5);

	cfg_save_search_history(".*\\.C");
	assert_success(exec_commands("select! //I", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);

	cfg_save_search_history(".*\\.c$");
	assert_success(exec_commands("select //", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	cfg_save_search_history("a.c");
	assert_success(exec_commands("unselect ////", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	cfg_resize_histories(0);
}

TEST(select_and_unselect_accept_external_command)
{
	strcpy(lwin.curr_dir, cwd);
	assert_success(chdir(cwd));

	add_some_files_to_view(&lwin);

#ifdef _WIN32
	/* Work around `echo` in cmd.exe, which outputs trailing spaces... */
	replace_string(&lwin.dir_entry[0].name, "a.c ");
	replace_string(&lwin.dir_entry[1].name, "b.cc ");
	replace_string(&lwin.dir_entry[2].name, "c.c ");
#endif

	assert_success(exec_commands("select !echo a.c", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);

	assert_success(exec_commands("select!!echo c.c", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	assert_success(exec_commands("unselect !echo c.c", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

static void
check_filetype(void)
{
	assoc_records_t ft;

	ft = ft_get_all_programs("file.version.tar");
	assert_int_equal(0, ft.count);
	ft_assoc_records_free(&ft);

	ft = ft_get_all_programs("file.version.tar.bz");
	assert_int_equal(1, ft.count);
	assert_string_equal("prog", ft.list[0].command);
	ft_assoc_records_free(&ft);
}

static int
prog_exists(const char name[])
{
	return 1;
}

static void
add_some_files_to_view(FileView *view)
{
	view->list_rows = 3;
	view->list_pos = 0;
	view->dir_entry = dynarray_cextend(NULL,
			view->list_rows*sizeof(*view->dir_entry));
	view->dir_entry[0].name = strdup("a.c");
	view->dir_entry[0].origin = &view->curr_dir[0];
	view->dir_entry[1].name = strdup("b.cc");
	view->dir_entry[1].origin = &view->curr_dir[0];
	view->dir_entry[2].name = strdup("c.c");
	view->dir_entry[2].origin = &view->curr_dir[0];
	view->selected_files = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
