#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* pathconf() rmdir() symlink() unlink() */

#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_common.h"
#include "../../src/fops_rename.h"
#include "../../src/status.h"

static void broken_link_name(const char prompt[], const char filename[],
		fo_prompt_cb cb, fo_complete_cmd_func complete, int allow_ee);

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);

	curr_view = &lwin;
}

TEARDOWN()
{
	view_teardown(&lwin);
	restore_cwd(saved_cwd);
}

TEST(generally_renames_files)
{
	char file[] = "file";
	char dir[] = "dir";
	char *names[] = { file, dir };

	create_file(SANDBOX_PATH "/file");
	create_dir(SANDBOX_PATH "/dir");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	(void)fops_rename(&lwin, names, 2, 0);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(rmdir(SANDBOX_PATH "/file"));
	assert_success(unlink(SANDBOX_PATH "/dir"));
}

TEST(renames_files_recursively)
{
	char file1[] = "dir2/file1";
	char file2[] = "dir1/file2";
	char *names[] = { file2, file1 };

	create_dir(SANDBOX_PATH "/dir1");
	create_dir(SANDBOX_PATH "/dir2");
	create_file(SANDBOX_PATH "/dir1/file1");
	create_file(SANDBOX_PATH "/dir2/file2");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	(void)fops_rename(&lwin, names, 2, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/dir1/file2"));
	assert_success(unlink(SANDBOX_PATH "/dir2/file1"));
	assert_success(rmdir(SANDBOX_PATH "/dir1"));
	assert_success(rmdir(SANDBOX_PATH "/dir2"));
}

TEST(renames_files_recursively_in_cv)
{
	char dir[PATH_MAX + 1];
	char new_name[PATH_MAX + 1];
	char *names[] = { new_name };

	to_canonic_path("dir", lwin.curr_dir, dir, sizeof(dir));
	to_canonic_path("dir/file2", lwin.curr_dir, new_name, sizeof(new_name));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "",
			saved_cwd);

	create_dir(SANDBOX_PATH "/dir");
	create_file(SANDBOX_PATH "/dir/file1");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, dir);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(1, lwin.list_rows);

	lwin.dir_entry[0].marked = 1;

	(void)fops_rename(&lwin, names, 1, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/dir/file2"));
	assert_success(rmdir(dir));
}

TEST(interdependent_rename)
{
	char file2[] = "file2";
	char file3[] = "file3";
	char *names[] = { file2, file3 };

	create_file(SANDBOX_PATH "/file1");
	create_file(SANDBOX_PATH "/file2");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	(void)fops_rename(&lwin, names, 2, 1);

	/* Make sure reloading doesn't fail with an assert of duplicated file name. */
	populate_dir_list(&lwin, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/file2"));
	assert_success(unlink(SANDBOX_PATH "/file3"));
}

TEST(incdec)
{
	create_file(SANDBOX_PATH "/file1");
	create_file(SANDBOX_PATH "/file2");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	lwin.list_pos = 0;
	(void)fops_incdec(&lwin, 1);
	/* Make sure reloading doesn't fail with an assert of duplicated file name. */
	populate_dir_list(&lwin, 1);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	lwin.list_pos = 1;
	(void)fops_incdec(&lwin, 1);
	/* Make sure reloading doesn't fail with an assert of duplicated file name. */
	populate_dir_list(&lwin, 1);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/file3"));
	assert_success(unlink(SANDBOX_PATH "/file4"));
}

TEST(rename_to_broken_symlink_name, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("no-such-file", SANDBOX_PATH "/broken-link"));
#endif

	create_file(SANDBOX_PATH "/a-file");

	populate_dir_list(&lwin, 0);
	lwin.list_pos = 0;
	fops_init(&broken_link_name, NULL);
	fops_rename_current(&lwin, 0);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/a-file"));
	assert_success(unlink(SANDBOX_PATH "/broken-link"));
}

static void
broken_link_name(const char prompt[], const char filename[], fo_prompt_cb cb,
		fo_complete_cmd_func complete, int allow_ee)
{
	cb("broken-link");
}

TEST(file_list_can_be_edited_including_long_fnames, IF(not_windows))
{
	assert_success(chdir(SANDBOX_PATH));

#ifndef _WIN32
	long name_max = pathconf(".", _PC_NAME_MAX);
#else
	long name_max = -1;
#endif
	if(name_max == -1)
	{
		name_max = NAME_MAX;
	}
	char long_name[name_max + 1];

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	FILE *fp = fopen("script", "w");
	fputs("#!/bin/sh\n", fp);
	fputs("sed 'y/1/2/' < $2 > $2_out\n", fp);
	fputs("mv $2_out $2\n", fp);
	fclose(fp);
	assert_success(chmod("script", 0777));

	curr_stats.exec_env_type = EET_LINUX_NATIVE;
	update_string(&cfg.vi_command, "./script");

	memset(long_name, '1', sizeof(long_name) - 1U);
	long_name[sizeof(long_name) - 1U] = '\0';

	create_file(long_name);

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_rename(&lwin, NULL, 0, 0);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	memset(long_name, '2', sizeof(long_name) - 1U);
	long_name[sizeof(long_name) - 1U] = '\0';
	assert_success(unlink(long_name));

	assert_success(unlink("script"));

	update_string(&cfg.vi_command, NULL);

	update_string(&cfg.shell, NULL);
	stats_update_shell_type("/bin/sh");
}

TEST(substitution_works)
{
	create_file(SANDBOX_PATH "/001");
	create_file(SANDBOX_PATH "/002");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	(void)fops_subst(&lwin, "1", "z", 0, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/00z"));
	assert_success(unlink(SANDBOX_PATH "/002"));
}

TEST(substitution_ignores_case)
{
	create_file(SANDBOX_PATH "/abc");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_subst(&lwin, "B", "z", 1, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/azc"));
}

TEST(substitution_respects_case)
{
	create_file(SANDBOX_PATH "/Aba");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_subst(&lwin, "a", "z", -1, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/Abz"));
}

TEST(global_substitution_with_broken_pattern)
{
	create_file(SANDBOX_PATH "/001");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_subst(&lwin, "*", "1", 0, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/001"));
}

TEST(global_substitution_of_caret_pattern)
{
	create_file(SANDBOX_PATH "/001");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_subst(&lwin, "^0", "", 0, 1);
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/01"));
}

/* No tests for custom/tree view, because control doesn't reach necessary checks
 * when new filenames are provided beforehand (only when user edits them). */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
