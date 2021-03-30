#include <stic.h>

#include <unistd.h> /* rmdir() unlink() */

#include <stdio.h> /* fclose() fopen() fprintf() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/int/fuse.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static char *saved_cwd;

static void populate(view_t *view);
static void mount(view_t *view, const char cmd[]);
static int unmount(view_t *view);
static int can_fuse(void);
static int can_fuse_and_emulate_errors(void);

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	char cwd[PATH_MAX + 1];
	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	strcpy(rwin.curr_dir, lwin.curr_dir);

	char fuse_home[PATH_MAX + 1];
	make_abs_path(fuse_home, sizeof(fuse_home), SANDBOX_PATH, ".fuse", cwd);

	replace_string(&cfg.fuse_home, fuse_home);
	replace_string(&cfg.shell, "/bin/sh");
	replace_string(&cfg.slow_fs_list, "");
	curr_stats.fuse_umount_cmd = "true";

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	fuse_unmount_all();
	assert_success(rmdir(cfg.fuse_home));

	view_teardown(&lwin);
	view_teardown(&rwin);

	update_string(&cfg.fuse_home, NULL);
	update_string(&cfg.shell, NULL);
	update_string(&cfg.slow_fs_list, NULL);
	curr_stats.fuse_umount_cmd = NULL;

	restore_cwd(saved_cwd);
}

TEST(bad_mounter_is_handled, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|false");
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(fuse_mount_works, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	assert_int_equal(1, unmount(&lwin));

	assert_success(unlink(SANDBOX_PATH "/mount.me/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(fuse_mount_2_works, IF(can_fuse))
{
	FILE *const f = fopen(SANDBOX_PATH "/mount.spec", "w");
	fprintf(f, "%s/mount.me", lwin.curr_dir);
	fclose(f);

	populate(&lwin);
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);

	mount(&lwin, "FUSE_MOUNT2|rmdir %DESTINATION_DIR && "
	             "ln -s %PARAM %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	assert_int_equal(1, unmount(&lwin));

	assert_success(unlink(SANDBOX_PATH "/mount.me/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
	assert_success(unlink(SANDBOX_PATH "/mount.spec"));
}

TEST(fuse_mount_2_fails_on_nonexistent_file, IF(can_fuse))
{
	create_file(SANDBOX_PATH "/mount.spec");
	populate(&lwin);
	assert_success(unlink(SANDBOX_PATH "/mount.spec"));
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);

	mount(&lwin, "FUSE_MOUNT2|rmdir %DESTINATION_DIR && "
	             "ln -s %PARAM %DESTINATION_DIR");
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(fuse_mount_2_fails_on_empty_file, IF(can_fuse))
{
	create_file(SANDBOX_PATH "/mount.spec");
	populate(&lwin);
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);

	mount(&lwin, "FUSE_MOUNT2|rmdir %DESTINATION_DIR && "
	             "ln -s %PARAM %DESTINATION_DIR");
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
	assert_success(unlink(SANDBOX_PATH "/mount.spec"));
}

TEST(fuse_mount_2_fails_on_file_with_newline_only, IF(can_fuse))
{
	FILE *const f = fopen(SANDBOX_PATH "/mount.spec", "w");
	fputs("\n", f);
	fclose(f);

	populate(&lwin);
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);

	mount(&lwin, "FUSE_MOUNT2|rmdir %DESTINATION_DIR && "
	             "ln -s %PARAM %DESTINATION_DIR");
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
	assert_success(unlink(SANDBOX_PATH "/mount.spec"));
}

TEST(fuse_mount_3_works, IF(can_fuse))
{
	curr_stats.fuse_umount_cmd = "false";

	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT3|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	assert_int_equal(1, unmount(&lwin));

	assert_success(unlink(SANDBOX_PATH "/mount.me/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(multiple_mounts_work, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	os_mkdir(SANDBOX_PATH "/mount.me.too", 0777);
	populate(&rwin);
	rwin.list_pos = 2;
	assert_string_equal("mount.me.too", rwin.dir_entry[2].name);

	mount(&rwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(rwin.curr_dir));

	/* Checks that initial mount is still reported correctly. */
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	/* Try unmounting in original order (mounts are stored in reverse order). */
	assert_int_equal(1, unmount(&lwin));
	assert_int_equal(1, unmount(&rwin));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me.too"));
}

TEST(unmounting_failure_is_handled, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	curr_stats.fuse_umount_cmd = "false";
	assert_int_equal(-1, unmount(&lwin));
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	curr_stats.fuse_umount_cmd = "true";
	assert_int_equal(1, unmount(&lwin));
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(unlink(SANDBOX_PATH "/mount.me/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(can_unmount_all_mounts, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	os_mkdir(SANDBOX_PATH "/mount.me.too", 0777);
	populate(&rwin);
	rwin.list_pos = 2;
	assert_string_equal("mount.me.too", rwin.dir_entry[2].name);

	mount(&rwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(rwin.curr_dir));

	fuse_unmount_all();
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_false(fuse_is_mount_point(lwin.curr_dir));
	assert_false(fuse_is_mount_point(rwin.curr_dir));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me.too"));
}

TEST(nested_mount_is_possible, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	os_mkdir(SANDBOX_PATH "/mount.me/nested.mount", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	assert_int_equal(1, unmount(&lwin));
	assert_int_equal(1, unmount(&lwin));

	assert_success(unlink(SANDBOX_PATH "/mount.me/nested.mount/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me/nested.mount"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(mounts_are_reused, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);
	populate(&rwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	mount(&rwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(rwin.curr_dir));

	assert_true(paths_are_equal(lwin.curr_dir, rwin.curr_dir));

	assert_int_equal(1, unmount(&lwin));
	assert_int_equal(0, unmount(&rwin));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(updir_from_a_mount, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);
	populate(&rwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	mount(&rwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(rwin.curr_dir));

	assert_true(fuse_try_updir_from_a_mount(lwin.curr_dir, &lwin));
	assert_false(paths_are_equal(lwin.curr_dir, rwin.curr_dir));

	assert_int_equal(0, unmount(&lwin));
	assert_int_equal(1, unmount(&rwin));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(mounting_with_foreground_flag_works, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR %FOREGROUND");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/test", lwin.curr_dir);
	create_file(path);

	assert_int_equal(1, unmount(&lwin));

	assert_success(unlink(SANDBOX_PATH "/mount.me/test"));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(fuse_get_mount_file_works, IF(can_fuse))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	char full_path[PATH_MAX + 1];
	get_current_full_path(&lwin, sizeof(full_path), full_path);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_true(fuse_is_mount_point(lwin.curr_dir));

	assert_true(paths_are_equal(full_path, fuse_get_mount_file(lwin.curr_dir)));

	char nested[PATH_MAX + 1];
	snprintf(nested, sizeof(nested), "%s/nested", lwin.curr_dir);
	assert_true(paths_are_equal(full_path, fuse_get_mount_file(nested)));

	assert_int_equal(1, unmount(&lwin));

	assert_success(rmdir(SANDBOX_PATH "/mount.me"));
}

TEST(bad_fuse_home_is_handled, IF(can_fuse_and_emulate_errors))
{
	os_mkdir(SANDBOX_PATH "/mount.me", 0777);
	populate(&lwin);

	char fuse_home[PATH_MAX + 1];
	make_abs_path(fuse_home, sizeof(fuse_home), SANDBOX_PATH, "for/fuse",
			saved_cwd);
	replace_string(&cfg.fuse_home, fuse_home);

	os_mkdir(SANDBOX_PATH "/for", 0555);

	mount(&lwin, "FUSE_MOUNT|rmdir %DESTINATION_DIR && "
	             "ln -s %SOURCE_FILE %DESTINATION_DIR");
	assert_false(fuse_is_mount_point(lwin.curr_dir));

	assert_success(os_chmod(SANDBOX_PATH "/for", 0777));
	assert_success(rmdir(SANDBOX_PATH "/mount.me"));

	make_abs_path(fuse_home, sizeof(fuse_home), SANDBOX_PATH, "for", saved_cwd);
	replace_string(&cfg.fuse_home, fuse_home);
}

static void
populate(view_t *view)
{
	populate_dir_list(view, 0);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
}

static void
mount(view_t *view, const char cmd[])
{
	fuse_try_mount(view, cmd);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
}

static int
unmount(view_t *view)
{
	int result = fuse_try_unmount(view);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	return result;
}

static int
can_fuse(void)
{
#if !defined(_WIN32) && !defined(__APPLE__)
	// These tests fail in a weird way on OS X...  Failed to figure out why.
	return 1;
#else
	return 0;
#endif
}

static int
can_fuse_and_emulate_errors(void)
{
	return (can_fuse() && regular_unix_user());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
