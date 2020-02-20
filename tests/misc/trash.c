#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/trash.h"

#include <stdio.h> /* snprintf() */

#include "utils.h"

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
