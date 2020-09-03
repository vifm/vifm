#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* F_OK access() chdir() rmdir() symlink() unlink() */

#include <stdio.h> /* FILE fclose() fopen() fprintf() remove() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/functions.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"
#include "../../src/registers.h"

static char *saved_cwd;

static char cwd[PATH_MAX + 1];
static char sandbox[PATH_MAX + 1];
static char test_data[PATH_MAX + 1];

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

TEST(cds_does_the_replacement)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds syntax-highlight rename", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
}

TEST(cds_aborts_on_broken_)
{
	char dst[PATH_MAX + 1];
	snprintf(dst, sizeof(dst), "%s/rename", test_data);

	assert_success(chdir(dst));

	strcpy(lwin.curr_dir, dst);

	assert_failure(exec_commands("cds/rename/read/t", &lwin, CIT_COMMAND));

	assert_string_equal(dst, lwin.curr_dir);
}

TEST(cds_acts_like_substitute)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds/SYNtax-?hi[a-z]*/rename/i", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
}

TEST(cds_can_change_path_of_both_panes)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/syntax-highlight", test_data);
	assert_success(chdir(path));
	strcpy(lwin.curr_dir, path);

	assert_success(exec_commands("cds! syntax-highlight rename", &lwin,
				CIT_COMMAND));

	snprintf(path, sizeof(path), "%s/rename", test_data);
	assert_string_equal(path, lwin.curr_dir);
	assert_string_equal(path, rwin.curr_dir);
}

TEST(cds_is_noop_when_pattern_not_found)
{
	assert_success(chdir(test_data));

	strcpy(lwin.curr_dir, test_data);
	strcpy(rwin.curr_dir, sandbox);

	assert_failure(exec_commands("cds asdlfkjasdlkfj rename", &lwin,
				CIT_COMMAND));

	assert_string_equal(test_data, lwin.curr_dir);
	assert_string_equal(sandbox, rwin.curr_dir);
}

TEST(cpmv_does_not_crash_on_wrong_list_access)
{
	char path[PATH_MAX + 1];
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
	assert_success(flist_load_tree(&lwin, sandbox));

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

TEST(wincmd_can_switch_views)
{
	init_modes();
	opt_handlers_setup();
	assert_success(stats_init(&cfg));

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("wincmd h", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("execute 'wincmd h'", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	init_builtin_functions();

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(
			exec_commands("if paneisat('left') == 0 | execute 'wincmd h' | endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(
			exec_commands("if paneisat('left') == 0 "
			             "|    execute 'wincmd h' "
			             "|    let $a = paneisat('left') "
			             "|endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);
	assert_string_equal("1", env_get("a"));

	function_reset_all();

	opt_handlers_teardown();
	assert_success(stats_reset(&cfg));
	vle_keys_reset();
}

TEST(wincmd_ignores_mappings)
{
	init_modes();
	opt_handlers_setup();

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("nnoremap <c-w> <nop>", curr_view, CIT_COMMAND));
	assert_success(exec_commands("wincmd H", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	opt_handlers_teardown();
	vle_keys_reset();
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

TEST(goto_command)
{
	char cmd[PATH_MAX*2];

	assert_failure(exec_commands("goto /", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("goto /no-such-path", &lwin, CIT_COMMAND));

	snprintf(cmd, sizeof(cmd), "goto %s/compare", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("compare", get_current_file_name(&lwin));

	assert_success(exec_commands("goto tree", &lwin, CIT_COMMAND));
	assert_true(paths_are_same(lwin.curr_dir, test_data));
	assert_string_equal("tree", get_current_file_name(&lwin));
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
