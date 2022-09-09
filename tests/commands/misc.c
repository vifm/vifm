#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* F_OK access() chdir() rmdir() symlink() unlink() */

#include <limits.h> /* INT_MAX */
#include <stdio.h> /* FILE fclose() fopen() fprintf() remove() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"
#include "../../src/plugins.h"
#include "../../src/registers.h"

static char *saved_cwd;

static char cwd[PATH_MAX + 1];
static char sandbox[PATH_MAX + 1];
static char test_data[PATH_MAX + 1];

static void strings_list_is(const strlist_t expected, const strlist_t actual);

SETUP_ONCE()
{
	cfg.sizefmt.base = 1;
	cfg.dot_dirs = DD_TREE_LEAFS_PARENT;

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

	cfg.cd_path = strdup("");
	cfg.fuse_home = strdup("");
	cfg.slow_fs_list = strdup("");
	cfg.use_system_calls = 1;

#ifndef _WIN32
	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
#else
	replace_string(&cfg.shell, "cmd");
	update_string(&cfg.shell_cmd_flag, "/C");
#endif

	stats_update_shell_type(cfg.shell);

	init_commands();

	undo_setup();

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	update_string(&cfg.cd_path, NULL);
	update_string(&cfg.fuse_home, NULL);
	update_string(&cfg.slow_fs_list, NULL);

	stats_update_shell_type("/bin/sh");
	update_string(&cfg.shell_cmd_flag, NULL);
	update_string(&cfg.shell, NULL);

	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();

	undo_teardown();
}

TEST(cd_in_root_works)
{
	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);

	assert_false(is_root_dir(lwin.curr_dir));
	assert_success(exec_commands("cd /", &lwin, CIT_COMMAND));
	assert_true(is_root_dir(lwin.curr_dir));
}

TEST(double_cd_uses_same_base_for_rel_paths)
{
	char path[PATH_MAX + 1];

	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);
	strcpy(rwin.curr_dir, "..");

	assert_success(exec_commands("cd read rename", &lwin, CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/read", test_data);
	assert_true(paths_are_equal(lwin.curr_dir, path));
	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_true(paths_are_equal(rwin.curr_dir, path));
}

TEST(tr_extends_second_field)
{
	char path[PATH_MAX + 1];

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

	(void)exec_commands("tr/ ?<>\\\\:*|\"/_", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a_b", sandbox);
	assert_success(remove(path));
}

TEST(substitute_works)
{
	char path[PATH_MAX + 1];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);

	snprintf(path, sizeof(path), "%s/a b b", sandbox);
	create_file(path);
	snprintf(path, sizeof(path), "%s/B c", sandbox);
	create_file(path);

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a b b");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("B c");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	(void)exec_commands("%substitute/b/c/Iig", &lwin, CIT_COMMAND);

	snprintf(path, sizeof(path), "%s/a c c", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/c c", sandbox);
	assert_success(remove(path));
}

TEST(chmod_works, IF(not_windows))
{
	char path[PATH_MAX + 1];

	assert_success(chdir(sandbox));

	strcpy(lwin.curr_dir, sandbox);

	snprintf(path, sizeof(path), "%s/file1", sandbox);
	create_file(path);
	snprintf(path, sizeof(path), "%s/file2", sandbox);
	create_file(path);

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file1");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("file2");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	(void)exec_commands("1,2chmod +x", &lwin, CIT_COMMAND);

	populate_dir_list(&lwin, 1);
	assert_int_equal(FT_EXEC, lwin.dir_entry[0].type);
	assert_int_equal(FT_EXEC, lwin.dir_entry[1].type);

	snprintf(path, sizeof(path), "%s/file1", sandbox);
	assert_success(remove(path));
	snprintf(path, sizeof(path), "%s/file2", sandbox);
	assert_success(remove(path));
}

TEST(edit_handles_ranges, IF(not_windows))
{
	create_file(SANDBOX_PATH "/file1");
	create_file(SANDBOX_PATH "/file2");

	char script_path[PATH_MAX + 1];
	make_abs_path(script_path, sizeof(script_path), sandbox, "script", NULL);
	update_string(&cfg.vi_command, script_path);
	update_string(&cfg.vi_x_command, "");

	FILE *fp = fopen(SANDBOX_PATH "/script", "w");
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "for arg; do echo \"$arg\" >> %s/vi-list; done\n", SANDBOX_PATH);
	fclose(fp);
	assert_success(chmod(SANDBOX_PATH "/script", 0777));

	strcpy(lwin.curr_dir, sandbox);
	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file1");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("file2");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	(void)exec_commands("%edit", &lwin, CIT_COMMAND);

	const char *lines[] = { "file1", "file2" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/script"));
	assert_success(remove(SANDBOX_PATH "/file1"));
	assert_success(remove(SANDBOX_PATH "/file2"));
	assert_success(remove(SANDBOX_PATH "/vi-list"));
}

TEST(putting_files_works)
{
	char path[PATH_MAX + 1];

	regs_init();

	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));
	assert_success(flist_load_tree(&lwin, sandbox, INT_MAX));

	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/binary-data", cwd);
	assert_success(regs_append(DEFAULT_REG_NAME, path));
	lwin.list_pos = 1;

	assert_true(exec_commands("put", &lwin, CIT_COMMAND) != 0);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/empty-dir/binary-data"));
	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));

	regs_reset();
}

TEST(yank_works_with_ranges)
{
	char path[PATH_MAX + 1];
	reg_t *reg;

	regs_init();

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "existing-files/a");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	reg = regs_find(DEFAULT_REG_NAME);
	assert_non_null(reg);

	assert_int_equal(0, reg->nfiles);
	(void)exec_commands("%yank", &lwin, CIT_COMMAND);
	assert_int_equal(1, reg->nfiles);

	regs_reset();
}

TEST(symlinks_in_paths_are_not_resolved, IF(not_windows))
{
	char canonic_path[PATH_MAX + 1];
	char buf[PATH_MAX + 1];

	assert_success(os_mkdir(SANDBOX_PATH "/dir1", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir1/dir2", 0700));

	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	{
		char src[PATH_MAX + 1], dst[PATH_MAX + 1];
		make_abs_path(src, sizeof(src), SANDBOX_PATH, "dir1/dir2", saved_cwd);
		make_abs_path(dst, sizeof(dst), SANDBOX_PATH, "dir-link", saved_cwd);
		assert_success(symlink(src, dst));
	}
#endif

	assert_success(chdir(SANDBOX_PATH "/dir-link"));
	make_abs_path(buf, sizeof(buf), SANDBOX_PATH, "dir-link", saved_cwd);
	to_canonic_path(buf, "/fake-root", lwin.curr_dir,
			sizeof(lwin.curr_dir));
	to_canonic_path(sandbox, "/fake-root", canonic_path,
			sizeof(canonic_path));

	/* :mkdir */
	(void)exec_commands("mkdir ../dir", &lwin, CIT_COMMAND);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(rmdir(SANDBOX_PATH "/dir"));

	/* :clone file name. */
	create_file(SANDBOX_PATH "/dir-link/file");
	populate_dir_list(&lwin, 1);
	(void)exec_commands("clone ../file-clone", &lwin, CIT_COMMAND);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(remove(SANDBOX_PATH "/file-clone"));
	assert_success(remove(SANDBOX_PATH "/dir-link/file"));

	/* :colorscheme */
	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"scripts/", saved_cwd);
	snprintf(buf, sizeof(buf), "colorscheme set-env %s/../dir-link/..",
			sandbox);
	assert_success(exec_commands(buf, &lwin, CIT_COMMAND));
	cs_load_defaults();

	/* :cd */
	assert_success(exec_commands("cd ../dir-link/..", &lwin, CIT_COMMAND));
	assert_string_equal(canonic_path, lwin.curr_dir);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(remove(SANDBOX_PATH "/dir-link"));
	assert_success(rmdir(SANDBOX_PATH "/dir1/dir2"));
	assert_success(rmdir(SANDBOX_PATH "/dir1"));
}

TEST(grep_command, IF(not_windows))
{
	opt_handlers_setup();

	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");

	assert_success(chdir(TEST_DATA_PATH "/scripts"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	assert_success(exec_commands("set grepprg='grep -n -H -r %i %a %s %u'", &lwin,
				CIT_COMMAND));

	/* Nothing to repeat. */
	assert_failure(exec_commands("grep", &lwin, CIT_COMMAND));

	assert_success(exec_commands("grep command", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("Grep command", lwin.custom.title);

	/* Repeat last grep, but add inversion. */
	assert_success(exec_commands("grep!", &lwin, CIT_COMMAND));
	assert_int_equal(5, lwin.list_rows);
	assert_string_equal("Grep command", lwin.custom.title);

	opt_handlers_teardown();
}

TEST(touch)
{
	to_canonic_path(SANDBOX_PATH, cwd, lwin.curr_dir, sizeof(lwin.curr_dir));
	(void)exec_commands("touch file", &lwin, CIT_COMMAND);

	assert_success(remove(SANDBOX_PATH "/file"));
}

TEST(compare)
{
	opt_handlers_setup();

	create_file(SANDBOX_PATH "/file");

	to_canonic_path(SANDBOX_PATH, cwd, lwin.curr_dir, sizeof(lwin.curr_dir));

	/* The file is empty so nothing to do when "skipempty" is specified. */
	assert_success(exec_commands("compare ofone skipempty", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));

	(void)exec_commands("compare byname bysize bycontents listall listdups "
			"listunique ofboth ofone groupids grouppaths", &lwin, CIT_COMMAND);
	assert_true(flist_custom_active(&lwin));
	assert_int_equal(CV_REGULAR, lwin.custom.type);

	assert_success(remove(SANDBOX_PATH "/file"));

	opt_handlers_teardown();
}

TEST(screen)
{
	assert_false(cfg.use_term_multiplexer);

	/* :screen toggles the option. */
	assert_success(exec_commands("screen", &lwin, CIT_COMMAND));
	assert_true(cfg.use_term_multiplexer);
	assert_success(exec_commands("screen", &lwin, CIT_COMMAND));
	assert_false(cfg.use_term_multiplexer);

	/* :screen! sets it to on. */
	assert_success(exec_commands("screen!", &lwin, CIT_COMMAND));
	assert_true(cfg.use_term_multiplexer);
	assert_success(exec_commands("screen!", &lwin, CIT_COMMAND));
	assert_true(cfg.use_term_multiplexer);

	cfg.use_term_multiplexer = 0;
}

TEST(hist_next_and_prev)
{
	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(10);
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	flist_hist_setup(&lwin, sandbox, ".", 0, 1);
	flist_hist_setup(&lwin, test_data, ".", 0, 1);

	assert_success(exec_commands("histprev", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, sandbox));
	assert_success(exec_commands("histnext", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));

	cfg_resize_histories(0);
}

TEST(normal_command_does_not_reset_selection)
{
	init_modes();
	opt_handlers_setup();

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 0;
	lwin.selected_files = 1;

	assert_success(exec_commands(":normal! t", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	assert_success(exec_commands(":normal! vG\r", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	assert_success(exec_commands(":normal! t", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	opt_handlers_teardown();
	vle_keys_reset();
}

TEST(keepsel_preserves_selection)
{
	init_view_list(&lwin);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	assert_failure(exec_commands("echo 'hi'", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	assert_failure(exec_commands("keepsel echo 'hi'", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
}

TEST(goto_command)
{
	assert_failure(exec_commands("goto /", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("goto /no-such-path", &lwin, CIT_COMMAND));

	char cmd[PATH_MAX*2];
	snprintf(cmd, sizeof(cmd), "goto %s/compare", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("compare", get_current_file_name(&lwin));

	assert_success(exec_commands("goto tree", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("tree", get_current_file_name(&lwin));
}

TEST(goto_normalizes_slashes, IF(windows))
{
	char cmd[PATH_MAX*2];
	snprintf(cmd, sizeof(cmd), "goto %s\\\\compare", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("compare", get_current_file_name(&lwin));
}

TEST(echo_reports_all_errors)
{
	const char *expected;

	expected = "Expression is missing closing quote: \"hi\n"
	           "Invalid expression: \"hi";

	ui_sb_msg("");
	assert_failure(exec_commands("echo \"hi", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());

	expected = "Expression is missing closing parenthesis: (1\n"
	           "Invalid expression: (1";

	ui_sb_msg("");
	assert_failure(exec_commands("echo (1", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());

	char zeroes[8192] = "echo ";
	memset(zeroes + strlen(zeroes), '0', sizeof(zeroes) - (strlen(zeroes) + 1U));
	zeroes[sizeof(zeroes) - 1U] = '\0';

	ui_sb_msg("");
	assert_failure(exec_commands(zeroes, &lwin, CIT_COMMAND));
	assert_true(strchr(ui_sb_last(), '\n') != NULL);
}

TEST(zero_count_is_rejected)
{
	const char *expected = "Count argument can't be zero";

	ui_sb_msg("");
	assert_failure(exec_commands("delete a 0", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());

	ui_sb_msg("");
	assert_failure(exec_commands("yank a 0", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());
}

TEST(tree_command)
{
	strcpy(lwin.curr_dir, sandbox);

	/* Invalid input. */
	assert_failure(exec_commands("tree nesting=0", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));
	assert_string_equal("Invalid argument: nesting=0", ui_sb_last());
	assert_failure(exec_commands("tree depth=0", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));
	assert_string_equal("Invalid depth: 0", ui_sb_last());

	/* :tree enters tree mode. */
	assert_success(exec_commands("tree", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
	assert_true(cv_tree(lwin.custom.type));

	/* Repeating :tree leaves view in tree mode. */
	assert_success(exec_commands("tree", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
	assert_true(cv_tree(lwin.custom.type));

	/* :tree! can leave tree mode. */
	assert_success(exec_commands("tree!", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));

	/* :tree! can enter tree mode. */
	assert_success(exec_commands("tree!", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
	assert_true(cv_tree(lwin.custom.type));

	/* Limited nesting. */

	char sub_path[PATH_MAX + 1];
	snprintf(sub_path, sizeof(sub_path), "%s/sub", sandbox);
	create_dir(sub_path);

	char sub_sub_path[PATH_MAX + 1];
	snprintf(sub_sub_path, sizeof(sub_sub_path), "%s/sub/sub", sandbox);
	create_dir(sub_sub_path);

	assert_success(exec_commands("tree depth=1", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
	assert_true(cv_tree(lwin.custom.type));
	assert_int_equal(1, lwin.list_rows);

	remove_dir(sub_sub_path);
	remove_dir(sub_path);
}

TEST(regular_command)
{
	strcpy(lwin.curr_dir, sandbox);

	/* :tree enters tree mode. */
	assert_success(exec_commands("tree", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));
	assert_true(cv_tree(lwin.custom.type));

	/* :regular leaves tree mode. */
	assert_success(exec_commands("regular", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));

	/* Repeated :regular does nothing. */
	assert_success(exec_commands("regular", &lwin, CIT_COMMAND));
	assert_false(flist_custom_active(&lwin));
}

TEST(plugin_command)
{
	curr_stats.vlua = vlua_init();
	curr_stats.plugs = plugs_create(curr_stats.vlua);

	ui_sb_msg("");
	assert_failure(exec_commands("plugin load all", &lwin, CIT_COMMAND));
	assert_string_equal("Trailing characters", ui_sb_last());
	assert_failure(exec_commands("plugin wrong arg", &lwin, CIT_COMMAND));
	assert_string_equal("Unknown subcommand: wrong", ui_sb_last());
	assert_failure(exec_commands("plugin args-count", &lwin, CIT_COMMAND));
	assert_string_equal("Too few arguments", ui_sb_last());

	assert_success(exec_commands("plugin load", &lwin, CIT_COMMAND));

	strlist_t empty_list = {};
	char *plug_items[] = { "plug" };
	strlist_t plug_list = { .items = plug_items, .nitems = 1 };

	ui_sb_msg("");
	assert_success(exec_commands("plugin blacklist plug", &lwin, CIT_COMMAND));
	assert_string_equal("", ui_sb_last());

	strings_list_is(plug_list, plugs_get_blacklist(curr_stats.plugs));
	strings_list_is(empty_list, plugs_get_whitelist(curr_stats.plugs));

	ui_sb_msg("");
	assert_success(exec_commands("plugin whitelist plug", &lwin, CIT_COMMAND));
	assert_success(exec_commands("plugin whitelist plug", &lwin, CIT_COMMAND));
	assert_string_equal("", ui_sb_last());

	strings_list_is(plug_list, plugs_get_blacklist(curr_stats.plugs));
	strings_list_is(plug_list, plugs_get_whitelist(curr_stats.plugs));

	plugs_free(curr_stats.plugs);
	curr_stats.plugs = NULL;
	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(help_command)
{
	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "#vifmtest#editor");
	cfg.config_dir[0] = '\0';

	curr_stats.vlua = vlua_init();

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info) ginfo = info; return { success = false } end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'editor', handler = handler }"));

	cfg.use_vim_help = 0;

	assert_success(exec_commands("help", &lwin, CIT_COMMAND));

	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
	assert_string_equal("edit-one", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.path)"));
	assert_string_equal("/vifm-help.txt", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.mustwait)"));
	assert_string_equal("false", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.line)"));
	assert_string_equal("nil", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.column)"));
	assert_string_equal("nil", ui_sb_last());

	cfg.use_vim_help = 1;

	assert_success(exec_commands("help", &lwin, CIT_COMMAND));

	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
	assert_string_equal("open-help", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.topic)"));
	assert_string_equal("vifm-app.txt", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.vimdocdir)"));
	assert_true(ends_with(ui_sb_last(), "/vim-doc"));

	cfg.use_vim_help = 0;

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(edit_command)
{
	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "#vifmtest#editor");
	cfg.config_dir[0] = '\0';

	curr_stats.vlua = vlua_init();

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info)"
				"  local s = ginfo ~= nil"
				"  ginfo = info"
				"  return { success = s }"
				"end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'editor', handler = handler }"));

	int i;
	for(i = 0; i < 2; ++i)
	{
		assert_success(exec_commands("edit a b", &lwin, CIT_COMMAND));

		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
		assert_string_equal("edit-many", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(#ginfo.paths)"));
		assert_string_equal("2", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.paths[1])"));
		assert_string_equal("a", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.paths[2])"));
		assert_string_equal("b", ui_sb_last());

		assert_success(vlua_run_string(curr_stats.vlua, "ginfo = {}"));
	}

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(view_command)
{
	opt_handlers_setup();

	curr_stats.preview.on = 0;

	assert_success(exec_commands("view", &lwin, CIT_COMMAND));
	assert_true(curr_stats.preview.on);

	assert_success(exec_commands("view", &lwin, CIT_COMMAND));
	assert_false(curr_stats.preview.on);

	assert_success(exec_commands("view!", &lwin, CIT_COMMAND));
	assert_true(curr_stats.preview.on);

	assert_success(exec_commands("view!", &lwin, CIT_COMMAND));
	assert_true(curr_stats.preview.on);

	assert_success(exec_commands("view", &lwin, CIT_COMMAND));
	assert_false(curr_stats.preview.on);

	opt_handlers_teardown();
}

TEST(invert_command)
{
	opt_handlers_setup();

	ui_sb_msg("");
	assert_failure(exec_commands("set sort? sortorder?", &lwin, CIT_COMMAND));
	assert_string_equal("  sort=+name\n  sortorder=ascending", ui_sb_last());

	assert_success(exec_commands("invert o", &lwin, CIT_COMMAND));

	ui_sb_msg("");
	assert_failure(exec_commands("set sort? sortorder?", &lwin, CIT_COMMAND));
	assert_string_equal("  sort=-name\n  sortorder=descending", ui_sb_last());

	opt_handlers_teardown();
}

static void
strings_list_is(const strlist_t expected, const strlist_t actual)
{
	assert_int_equal(expected.nitems, actual.nitems);

	int i;
	for(i = 0; i < MIN(expected.nitems, actual.nitems); ++i)
	{
		assert_string_equal(expected.items[i], actual.items[i]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
