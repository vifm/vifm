#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* pathconf() rmdir() unlink() */

#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/modes/dialogs/msg_dialog.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_common.h"
#include "../../src/fops_rename.h"
#include "../../src/status.h"

static void broken_link_name(const char prompt[], const char filename[],
		fo_prompt_cb cb, void *cb_arg, fo_complete_cmd_func complete);
static int allow_move_dlg_cb(const char type[], const char title[],
		const char message[]);
static int deny_move_dlg_cb(const char type[], const char title[],
		const char message[]);
static int on_case_sensitive_fs(void);

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

TEST(fail_to_renames_files)
{
	char file[] = "file";
	char *names[] = { file, file };

	create_file(SANDBOX_PATH "/file");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	ui_sb_msg("");
	(void)fops_rename(&lwin, names, /*nlines=*/2, /*recursive=*/0);
	assert_string_equal("Too many file names (2/1)", ui_sb_last());

	assert_success(unlink(SANDBOX_PATH "/file"));
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
	assert_success(make_symlink("no-such-file", SANDBOX_PATH "/broken-link"));
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
		void *cb_arg, fo_complete_cmd_func complete)
{
	cb("broken-link", cb_arg);
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

TEST(rename_can_move_files)
{
	create_file(SANDBOX_PATH "/file");
	create_dir(SANDBOX_PATH "/dir");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[1].marked = 1;

	char file[] = "dir/file";
	char *names[] = { file };

	assert_success(chdir(SANDBOX_PATH));

	dlg_set_callback(&allow_move_dlg_cb);

	ui_sb_msg("");
	(void)fops_rename(&lwin, names, /*nlines=*/1, /*recursive=*/0);
	assert_string_equal("1 file renamed", ui_sb_last());

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(rename_accepts_rejection_to_move_files)
{
	create_file(SANDBOX_PATH "/file");
	create_dir(SANDBOX_PATH "/dir");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[1].marked = 1;

	char file[] = "dir/file";
	char *names[] = { file };

	assert_success(chdir(SANDBOX_PATH));

	dlg_set_callback(&deny_move_dlg_cb);

	ui_sb_msg("");
	(void)fops_rename(&lwin, names, /*nlines=*/1, /*recursive=*/0);
	assert_string_equal("1 unintended move", ui_sb_last());

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(rename_wont_create_target_dir)
{
	create_file(SANDBOX_PATH "/file");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	char file[] = "dir/file";
	char *names[] = { file };

	assert_success(chdir(SANDBOX_PATH));

	ui_sb_msg("");
	(void)fops_rename(&lwin, names, /*nlines=*/1, /*recursive=*/0);
	assert_string_equal("Won't create directory for \"dir/file\"", ui_sb_last());

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(unlink(SANDBOX_PATH "/file"));
}

TEST(case_change_works, IF(on_case_sensitive_fs))
{
	create_file(SANDBOX_PATH "/fIlE1");
	create_file(SANDBOX_PATH "/FiLe2");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	ui_sb_msg("");
	(void)fops_case(&lwin, /*to_upper=*/0);
	assert_string_equal("2 files renamed", ui_sb_last());

	assert_failure(unlink(SANDBOX_PATH "/fIlE1"));
	assert_failure(unlink(SANDBOX_PATH "/FiLe2"));
	assert_success(unlink(SANDBOX_PATH "/file1"));
	assert_success(unlink(SANDBOX_PATH "/file2"));
}

TEST(case_change_detect_dups, IF(on_case_sensitive_fs))
{
	create_file(SANDBOX_PATH "/fIlE");
	create_file(SANDBOX_PATH "/FiLe");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	ui_sb_msg("");
	(void)fops_case(&lwin, /*to_upper=*/0);
	assert_string_equal("Name \"file\" duplicates", ui_sb_last());

	assert_success(unlink(SANDBOX_PATH "/fIlE"));
	assert_success(unlink(SANDBOX_PATH "/FiLe"));
	assert_failure(unlink(SANDBOX_PATH "/file"));
}

TEST(substitution_and_idential_names_in_different_dirs)
{
	create_dir(SANDBOX_PATH "/adir");
	create_file(SANDBOX_PATH "/adir/abc");
	create_dir(SANDBOX_PATH "/bdir");
	create_file(SANDBOX_PATH "/bdir/abc");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "adir/abc");
	flist_custom_add(&lwin, "bdir/abc");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(2, lwin.list_rows);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	ui_sb_msg("");
	(void)fops_subst(&lwin, "$", "d", /*ic=*/0, /*glob=*/0);
	assert_string_equal("2 files renamed", ui_sb_last());

	assert_failure(unlink(SANDBOX_PATH "/adir/abc"));
	assert_success(unlink(SANDBOX_PATH "/adir/abcd"));
	assert_success(rmdir(SANDBOX_PATH "/adir"));
	assert_failure(unlink(SANDBOX_PATH "/bdir/abc"));
	assert_success(unlink(SANDBOX_PATH "/bdir/abcd"));
	assert_success(rmdir(SANDBOX_PATH "/bdir"));
}

TEST(translation_and_idential_names_in_different_dirs)
{
	create_dir(SANDBOX_PATH "/adir");
	create_file(SANDBOX_PATH "/adir/abc");
	create_dir(SANDBOX_PATH "/bdir");
	create_file(SANDBOX_PATH "/bdir/abc");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "adir/abc");
	flist_custom_add(&lwin, "bdir/abc");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(2, lwin.list_rows);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	ui_sb_msg("");
	(void)fops_tr(&lwin, "abc", "xyz");
	assert_string_equal("2 files renamed", ui_sb_last());

	assert_failure(unlink(SANDBOX_PATH "/adir/abc"));
	assert_success(unlink(SANDBOX_PATH "/adir/xyz"));
	assert_success(rmdir(SANDBOX_PATH "/adir"));
	assert_failure(unlink(SANDBOX_PATH "/bdir/abc"));
	assert_success(unlink(SANDBOX_PATH "/bdir/xyz"));
	assert_success(rmdir(SANDBOX_PATH "/bdir"));
}

TEST(case_change_and_idential_names_in_different_dirs, IF(on_case_sensitive_fs))
{
	create_dir(SANDBOX_PATH "/adir");
	create_file(SANDBOX_PATH "/adir/abc");
	create_dir(SANDBOX_PATH "/bdir");
	create_file(SANDBOX_PATH "/bdir/abc");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "adir/abc");
	flist_custom_add(&lwin, "bdir/abc");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(2, lwin.list_rows);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;

	ui_sb_msg("");
	(void)fops_case(&lwin, /*to_upper=*/1);
	assert_string_equal("2 files renamed", ui_sb_last());

	assert_failure(unlink(SANDBOX_PATH "/adir/abc"));
	assert_success(unlink(SANDBOX_PATH "/adir/ABC"));
	assert_success(rmdir(SANDBOX_PATH "/adir"));
	assert_failure(unlink(SANDBOX_PATH "/bdir/abc"));
	assert_success(unlink(SANDBOX_PATH "/bdir/ABC"));
	assert_success(rmdir(SANDBOX_PATH "/bdir"));
}

static int
allow_move_dlg_cb(const char type[], const char title[], const char message[])
{
	assert_string_equal("prompt", type);
	assert_string_equal("Rename", title);
	assert_string_starts_with("It appears that the rename list has 1 file move,",
			message);

	dlg_set_callback(NULL);
	return 1;
}

static int
deny_move_dlg_cb(const char type[], const char title[], const char message[])
{
	assert_string_equal("prompt", type);
	assert_string_equal("Rename", title);
	assert_string_starts_with("It appears that the rename list has 1 file move,",
			message);

	dlg_set_callback(NULL);
	return 0;
}

/* No tests for custom/tree view, because control doesn't reach necessary checks
 * when new filenames are provided beforehand (only when user edits them). */

/* Checks that sandbox directory is located on a file-system that allows files
 * that differ only by character case.  Returns non-zero if so. */
static int
on_case_sensitive_fs(void)
{
	/* Use of SANDBOX_PATH prevents this function from being in test utilities. */
	int result = (os_mkdir(SANDBOX_PATH "/tEsT", 0700) == 0)
	          && (os_rmdir(SANDBOX_PATH "/test") != 0);
	(void)os_rmdir(SANDBOX_PATH "/tEsT");
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
