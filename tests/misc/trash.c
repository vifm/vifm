#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/trash.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
}

TEST(nonempty_ro_dir_is_removed, IF(not_windows))
{
	char sandbox[PATH_MAX + 1];
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", saved_cwd);

	assert_success(set_trash_dir(sandbox));

	assert_success(os_mkdir("dir", 0777));
	create_file("dir/a");
	assert_success(os_chmod("dir", 0555));

	trash_empty_all();

	wait_for_bg();

	assert_failure(os_chmod("dir", 0777));
	assert_failure(unlink("dir/a"));
	assert_failure(rmdir("dir"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
