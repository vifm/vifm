#include <stic.h>

#include <unistd.h> /* symlink() */

#include <stdio.h> /* snprintf() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/trash.h"

static char sandbox[PATH_MAX + 1];
static char *saved_cwd;

SETUP_ONCE()
{
	saved_cwd = save_cwd();
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", saved_cwd);
	assert_success(trash_set_specs(sandbox));
}

TEARDOWN_ONCE()
{
	trash_empty_all();
	wait_for_bg();
}

SETUP()
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(chdir(SANDBOX_PATH));
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
	saved_cwd = NULL;
}

TEST(directory_tree_is_removed)
{
	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/sub", 0777));
	create_file("dir/sub/file");

	trash_empty_all();

	wait_for_bg();

	assert_failure(unlink("dir/sub/file"));
	assert_failure(rmdir("dir/sub"));
	assert_failure(rmdir("dir"));
}

TEST(nonempty_ro_dir_is_removed, IF(not_windows))
{
	assert_success(os_mkdir("dir", 0777));
	create_file("dir/a");
	assert_success(os_chmod("dir", 0555));

	trash_empty_all();

	wait_for_bg();

	assert_failure(os_chmod("dir", 0777));
	assert_failure(unlink("dir/a"));
	assert_failure(rmdir("dir"));
}

TEST(trash_allows_multiple_files_with_same_original_path)
{
	char path[PATH_MAX + 1];

	snprintf(path, sizeof(path), "%s/trashed_1", sandbox);
	assert_success(trash_add_entry("/some/path/src", path));
	assert_int_equal(1, trash_list_size);

	snprintf(path, sizeof(path), "%s/trashed_2", sandbox);
	assert_success(trash_add_entry("/some/path/src", path));
	assert_int_equal(2, trash_list_size);
}

TEST(trash_specs_are_expanded_correctly)
{
	init_variables();
	let_variables("$TEST_ENVVAR = '~'");

	strcpy(cfg.home_dir, sandbox);
	assert_success(trash_set_specs("~,$TEST_ENVVAR,~"));

	assert_int_equal(3, nspecs);
	assert_string_equal(sandbox, specs[0]);
	assert_string_equal(sandbox, specs[1]);
	assert_string_equal(sandbox, specs[2]);

	clear_variables();
}

TEST(trash_dir_can_be_a_symlink, IF(not_windows))
{
	create_dir("dir");

	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("dir", "dir-link"));
#endif

	char trash[PATH_MAX + 1];
	make_abs_path(trash, sizeof(trash), SANDBOX_PATH, "dir-link", saved_cwd);
	assert_success(trash_set_specs(trash));

	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/trashed", trash);
	assert_success(trash_add_entry("/some/path/src", path));
	assert_int_equal(3, trash_list_size);
	assert_true(trash_has_path(path));

	remove_file("dir-link");
	remove_dir("dir");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
