#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() rmdir() symlink() usleep() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* FILE fclose() fopen() fprintf() remove() snprintf() */
#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/running.h"
#include "../../src/status.h"

static int prog_exists(const char name[]);
static void start_use_script(void);
static void stop_use_script(void);
static void assoc_a(char macro);
static void assoc_b(char macro);
static void assoc_common(void);
static void assoc(const char pattern[], const char cmd[]);

static char cwd[PATH_MAX + 1];
static char script_path[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
#ifndef _WIN32
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
#else
	update_string(&cfg.shell, "cmd");
	update_string(&cfg.shell_cmd_flag, "/C");
#endif

	update_string(&cfg.vi_command, "echo");
	update_string(&cfg.slow_fs_list, "");
	update_string(&cfg.fuse_home, "");

	stats_update_shell_type(cfg.shell);

	view_setup(&lwin);

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	curr_view = &lwin;
	other_view = &rwin;

	ft_init(&prog_exists);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
	stats_update_shell_type("/bin/sh");

	view_teardown(&lwin);

	ft_reset(0);
	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);
}

TEST(full_path_regexps_are_handled_for_selection)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "echo %f >> " SANDBOX_PATH "/run", 0, 1);

	rn_open(&lwin, FHE_NO_RUN);

	/* Checking for file existence on its removal. */
	assert_success(remove(SANDBOX_PATH "/run"));
}

TEST(full_path_regexps_are_handled_for_selection2)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
#ifndef _WIN32
	ft_set_programs(ms, "echo > /dev/null %c &", 0, 1);
#else
	ft_set_programs(ms, "echo > NUL %c &", 0, 1);
#endif

	rn_open(&lwin, FHE_NO_RUN);

	/* If we don't crash, then everything is fine. */
}

TEST(can_open_via_plugin)
{
	curr_stats.vlua = vlua_init();

	assert_success(vlua_run_string(curr_stats.vlua,
				"function open(info) name = info.entry.name\n end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'open', handler = open }"));

	char *error;
	matchers_t *ms = matchers_alloc("*", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "#vifmtest#open", /*for_x=*/0, /*in_x=*/1);

	rn_open(&lwin, FHE_NO_RUN);

	ui_sb_msg("");
	assert_success(vlua_run_string(curr_stats.vlua, "print(name)"));
	assert_string_equal("a", ui_sb_last());

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(following_resolves_links_in_origin, IF(not_windows))
{
	assert_success(os_mkdir(SANDBOX_PATH "/A", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/A/B", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/A/C", 0700));
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("../C", SANDBOX_PATH "/A/B/c"));
	assert_success(symlink("A/B", SANDBOX_PATH "/B"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "B", cwd);
	populate_dir_list(&lwin, 0);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("c", lwin.dir_entry[0].name);
	lwin.list_pos = 0;

	char *saved_cwd = save_cwd();
	rn_follow(&lwin, 0);
	restore_cwd(saved_cwd);

	assert_true(paths_are_same(lwin.curr_dir, SANDBOX_PATH "/A"));
	assert_string_equal("C", lwin.dir_entry[lwin.list_pos].name);

	assert_success(remove(SANDBOX_PATH "/B"));
	assert_success(remove(SANDBOX_PATH "/A/B/c"));
	assert_success(rmdir(SANDBOX_PATH "/A/C"));
	assert_success(rmdir(SANDBOX_PATH "/A/B"));
	assert_success(rmdir(SANDBOX_PATH "/A"));
}

TEST(following_to_a_broken_symlink_is_possible, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("no-file", SANDBOX_PATH "/bad"));
	assert_success(symlink("bad", SANDBOX_PATH "/to-bad"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	populate_dir_list(&lwin, 0);

	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("to-bad", lwin.dir_entry[1].name);
	lwin.list_pos = 1;

	char *saved_cwd = save_cwd();
	rn_follow(&lwin, 0);
	restore_cwd(saved_cwd);

	assert_true(paths_are_same(lwin.curr_dir, SANDBOX_PATH));
	assert_string_equal("bad", lwin.dir_entry[lwin.list_pos].name);

	assert_success(remove(SANDBOX_PATH "/bad"));
	assert_success(remove(SANDBOX_PATH "/to-bad"));
}

TEST(selection_uses_vim_on_all_undefs, IF(not_windows))
{
	start_use_script();

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a", "b" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_uses_vim_on_at_least_one_undef_non_current, IF(not_windows))
{
	start_use_script();
	assoc_a('c');

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a", "b" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_failure(remove(SANDBOX_PATH "/a-list"));
	assert_success(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_uses_vim_on_at_least_one_undef_current, IF(not_windows))
{
	start_use_script();
	assoc_b('c');

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a", "b" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_failure(remove(SANDBOX_PATH "/b-list"));
	assert_success(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_uses_common_handler, IF(not_windows))
{
	start_use_script();
	assoc_common();

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a", "b" };
	file_is(SANDBOX_PATH "/common-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/common-list"));
	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_is_incompatible, IF(not_windows))
{
	start_use_script();
	assoc_a('c');
	assoc_b('f');

	rn_open(&lwin, FHE_NO_RUN);

	assert_failure(remove(SANDBOX_PATH "/a-list"));
	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_is_compatible, IF(not_windows))
{
	start_use_script();
	assoc("{a}", "echo > /dev/null %c &");
	assoc("{b}", "echo > /dev/null %c &");

	rn_open(&lwin, FHE_NO_RUN);

	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(inconsistent_selection, IF(not_windows))
{
	start_use_script();
	lwin.dir_entry[0].type = FT_DIR;

	rn_open(&lwin, FHE_NO_RUN);

	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(selection_with_broken_link, IF(not_windows))
{
	start_use_script();
	/* Simulate broken link using incorrect name of a file. */
	update_string(&lwin.dir_entry[0].name, "no-such-file");

	rn_open(&lwin, FHE_NO_RUN);

	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(current_file_not_part_of_selection, IF(not_windows))
{
	start_use_script();
	lwin.dir_entry[0].selected = 0;
	--lwin.selected_files;

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a" };
	file_is(SANDBOX_PATH "/vi-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(entering_parent_directory, IF(not_windows))
{
	start_use_script();
	update_string(&lwin.dir_entry[0].name, "..");
	lwin.dir_entry[0].type = FT_DIR;

	rn_open(&lwin, FHE_NO_RUN);

	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(entering_a_selected_directory, IF(not_windows))
{
	start_use_script();
	update_string(&lwin.dir_entry[0].name, "existing-files");
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	lwin.dir_entry[0].type = FT_DIR;
	lwin.dir_entry[1].selected = 0;
	--lwin.selected_files;

	rn_open(&lwin, FHE_NO_RUN);

	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(entering_an_unselected_directory, IF(not_windows))
{
	view_teardown(&lwin);
	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	lwin.list_rows = 3;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("existing-files");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 0;
	lwin.dir_entry[0].type = FT_DIR;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].name = strdup("c");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;

	char out_file[PATH_MAX + 1];
	make_abs_path(out_file, sizeof(out_file), SANDBOX_PATH, "chosen", cwd);
	curr_stats.chosen_files_out = out_file;

	start_use_script();

	rn_open(&lwin, FHE_NO_RUN);

	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "existing-files", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	curr_stats.chosen_files_out = NULL;
	assert_failure(remove(SANDBOX_PATH "/chosen"));
	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(macro_can_be_added_implicitly, IF(not_windows))
{
	start_use_script();
	lwin.dir_entry[1].selected = 0;
	--lwin.selected_files;

	char *error;
	matchers_t *ms = matchers_alloc("{a}", 0, 1, "", &error);
	assert_non_null(ms);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "%s a", script_path);
	ft_set_programs(ms, cmd, 0, 0);

	rn_open(&lwin, FHE_NO_RUN);

	const char *lines[] = { "a" };
	file_is(SANDBOX_PATH "/a-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/a-list"));
	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(handler_can_be_matched_by_a_prefix, IF(not_windows))
{
	start_use_script();
	lwin.dir_entry[1].selected = 0;
	--lwin.selected_files;

	char *error;
	matchers_t *ms = matchers_alloc("{a}", 0, 1, "", &error);
	assert_non_null(ms);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "{wrong}no-such-cmd a, {right}%s a", script_path);
	ft_set_programs(ms, cmd, 0, 0);

	rn_open_with_match(&lwin, script_path, 0);

	const char *lines[] = { "a" };
	file_is(SANDBOX_PATH "/a-list", lines, ARRAY_LEN(lines));

	assert_success(remove(SANDBOX_PATH "/a-list"));
	assert_failure(remove(SANDBOX_PATH "/vi-list"));

	stop_use_script();
}

TEST(provide_input_on_statusbar_redirection, IF(have_cat))
{
	view_setup(&rwin);

	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "", cwd);
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	setup_grid(&rwin, 20, 2, /*init=*/1);
	replace_string(&rwin.dir_entry[0].name, "a");
	replace_string(&rwin.dir_entry[1].name, "b");

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.pending_marking = 1;

	rn_open_with(&rwin, "cat %Pl%S", /*dont_execute=*/0, /*force_bg=*/0);

	char a[PATH_MAX + 1];
	char b[PATH_MAX + 1];
	get_full_path_of(&rwin.dir_entry[0], sizeof(a), a);
	get_full_path_of(&rwin.dir_entry[1], sizeof(b), b);

	char buf[PATH_MAX*2 + 16];
	snprintf(buf, sizeof(buf), "%s\n%s", a, b);

	assert_string_equal(buf, ui_sb_last());

	remove_file(SANDBOX_PATH "/a");
	remove_file(SANDBOX_PATH "/b");

	view_teardown(&rwin);
}

TEST(provide_input_on_nowhere_redirection, IF(have_cat))
{
	view_setup(&rwin);

	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "", cwd);
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	setup_grid(&rwin, 20, 2, /*init=*/1);
	replace_string(&rwin.dir_entry[0].name, "a");
	replace_string(&rwin.dir_entry[1].name, "b");

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.pending_marking = 1;

	char *saved_dir = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	rn_open_with(&rwin, "cat > file && touch confirmwrite %Pl%i",
			/*dont_execute=*/0, /*force_bg=*/0);

	restore_cwd(saved_dir);

	char a[PATH_MAX + 1];
	char b[PATH_MAX + 1];
	make_abs_path(a, sizeof(a), SANDBOX_PATH, "a", cwd);
	make_abs_path(b, sizeof(b), SANDBOX_PATH, "b", cwd);

	int i;
	for(i = 0; i < 100; ++i)
	{
		usleep(5000);
		if(path_exists(SANDBOX_PATH "/confirmwrite", DEREF))
		{
			break;
		}
	}

	const char *lines[] = { a, b };
	file_is(SANDBOX_PATH "/file", lines, ARRAY_LEN(lines));

	remove_file(SANDBOX_PATH "/confirmwrite");
	remove_file(SANDBOX_PATH "/file");
	remove_file(SANDBOX_PATH "/a");
	remove_file(SANDBOX_PATH "/b");

	view_teardown(&rwin);
}

TEST(provide_input_to_fg_process, IF(have_cat))
{
	view_setup(&rwin);

	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "", cwd);
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	setup_grid(&rwin, 20, 2, /*init=*/1);
	replace_string(&rwin.dir_entry[0].name, "a");
	replace_string(&rwin.dir_entry[1].name, "b");

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.pending_marking = 1;

	char *saved_dir = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	rn_open_with(&rwin, "cat > file %Pl", /*dont_execute=*/0, /*force_bg=*/0);

	restore_cwd(saved_dir);

	char a[PATH_MAX + 1];
	char b[PATH_MAX + 1];
	make_abs_path(a, sizeof(a), SANDBOX_PATH, "a", cwd);
	make_abs_path(b, sizeof(b), SANDBOX_PATH, "b", cwd);

	const char *lines[] = { a, b };
	file_is(SANDBOX_PATH "/file", lines, ARRAY_LEN(lines));

	remove_file(SANDBOX_PATH "/file");
	remove_file(SANDBOX_PATH "/a");
	remove_file(SANDBOX_PATH "/b");

	view_teardown(&rwin);
}

TEST(provide_input_to_bg_process, IF(have_cat))
{
	view_setup(&rwin);

	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "", cwd);
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	setup_grid(&rwin, 20, 2, /*init=*/1);
	replace_string(&rwin.dir_entry[0].name, "a");
	replace_string(&rwin.dir_entry[1].name, "b");

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.pending_marking = 1;

	char *saved_dir = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	rn_open_with(&rwin, "cat > file %Pl &", /*dont_execute=*/0, /*force_bg=*/0);

	restore_cwd(saved_dir);

	wait_for_all_bg();

	char a[PATH_MAX + 1];
	char b[PATH_MAX + 1];
	make_abs_path(a, sizeof(a), SANDBOX_PATH, "a", cwd);
	make_abs_path(b, sizeof(b), SANDBOX_PATH, "b", cwd);

	const char *lines[] = { a, b };
	file_is(SANDBOX_PATH "/file", lines, ARRAY_LEN(lines));

	remove_file(SANDBOX_PATH "/file");
	remove_file(SANDBOX_PATH "/a");
	remove_file(SANDBOX_PATH "/b");

	view_teardown(&rwin);
}

static int
prog_exists(const char name[])
{
	return 1;
}

static void
start_use_script(void)
{
	make_abs_path(script_path, sizeof(script_path), SANDBOX_PATH, "script", cwd);

	char vi_cmd[PATH_MAX + 1];
	snprintf(vi_cmd, sizeof(vi_cmd), "%s vi", script_path);
	update_string(&cfg.vi_command, vi_cmd);

	FILE *fp = fopen(SANDBOX_PATH "/script", "w");
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "prefix=$1\n");
	fprintf(fp, "shift\n");
	fprintf(fp, "for arg; do echo \"$arg\" >> %s/${prefix}-list; done\n",
			SANDBOX_PATH);
	fclose(fp);
	assert_success(chmod(SANDBOX_PATH "/script", 0777));
}

static void
stop_use_script(void)
{
	assert_success(remove(script_path));
}

static void
assoc_a(char macro)
{
	char *error;
	matchers_t *ms = matchers_alloc("{a}", 0, 1, "", &error);
	assert_non_null(ms);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "%s a %%%c", script_path, macro);
	ft_set_programs(ms, cmd, 0, 0);
}

static void
assoc_b(char macro)
{
	char *error;
	matchers_t *ms = matchers_alloc("{b}", 0, 1, "", &error);
	assert_non_null(ms);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "%s b %%%c", script_path, macro);
	ft_set_programs(ms, cmd, 0, 0);
}

static void
assoc_common(void)
{
	char *error;
	matchers_t *ms = matchers_alloc("{a,b}", 0, 1, "", &error);
	assert_non_null(ms);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "%s common %%f", script_path);
	ft_set_programs(ms, cmd, 0, 0);
}

static void
assoc(const char pattern[], const char cmd[])
{
	char *error;
	matchers_t *ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);

	ft_set_programs(ms, cmd, 0, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
