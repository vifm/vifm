#include <stic.h>

#include <unistd.h> /* chdir() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"
#include "../../src/status.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);

	update_string(&cfg.fuse_home, "");
}

TEARDOWN()
{
	view_teardown(&lwin);
	update_string(&cfg.fuse_home, NULL);

	restore_cwd(saved_cwd);
}

TEST(not_writable_dir, IF(regular_unix_user))
{
	create_dir("sub");
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "sub",
			saved_cwd);

	assert_success(make_symlink("target1", "sub/symlink1"));
	assert_success(os_chmod("sub", 0555));

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	assert_int_equal(0, fops_retarget(&lwin));

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(chdir(SANDBOX_PATH));
	assert_success(os_chmod("sub", 0777));
	remove_file("sub/symlink1");
	remove_dir("sub");
}

TEST(not_symlink, IF(not_windows))
{
	create_file("file");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	assert_int_equal(1, fops_retarget(&lwin));

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	remove_file(SANDBOX_PATH "/file");
}

TEST(no_changes, IF(not_windows))
{
	assert_success(make_symlink("target1", "symlink1"));
	assert_success(make_symlink("target2", "symlink2"));

	populate_dir_list(&lwin, 0);

	create_executable("script");
	make_file("script", "#!/bin/sh\n");

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	curr_stats.exec_env_type = EET_LINUX_NATIVE;
	update_string(&cfg.vi_command, "./script");

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	assert_int_equal(1, fops_retarget(&lwin));

	char target[PATH_MAX + 1];
	assert_success(get_link_target("symlink1", target, sizeof(target)));
	assert_string_equal("target1", target);
	assert_success(get_link_target("symlink2", target, sizeof(target)));
	assert_string_equal("target2", target);

	remove_file("symlink1");
	remove_file("symlink2");
	remove_file("script");
}

TEST(retarget_files, IF(not_windows))
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		assert_success(make_symlink("target1", "symlink1"));
		assert_success(make_symlink("target2", "symlink2"));

		populate_dir_list(&lwin, 0);

		create_executable("script");
		make_file("script",
				"#!/bin/sh\n"
				"sed 's/get//' < $2 > $2_out\n"
				"mv $2_out $2\n");

		update_string(&cfg.shell, "/bin/sh");
		stats_update_shell_type(cfg.shell);

		curr_stats.exec_env_type = EET_LINUX_NATIVE;
		update_string(&cfg.vi_command, "./script");

		lwin.dir_entry[0].marked = 1;
		lwin.dir_entry[1].marked = 1;
		assert_int_equal(1, fops_retarget(&lwin));

		char target[PATH_MAX + 1];
		assert_success(get_link_target("symlink1", target, sizeof(target)));
		assert_string_equal("tar1", target);
		assert_success(get_link_target("symlink2", target, sizeof(target)));
		assert_string_equal("tar2", target);

		remove_file("symlink1");
		remove_file("symlink2");
		remove_file("script");
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
