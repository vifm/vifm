#include <stic.h>

#include <unistd.h> /* chdir() */

#include "../../src/cfg/config.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fileops.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	cfg.use_trash = 0;
}

TEARDOWN()
{
	view_teardown(&lwin);
	restore_cwd(saved_cwd);
}

TEST(files_are_cloned)
{
	create_empty_file(SANDBOX_PATH "/a");
	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)clone_files(&lwin, NULL, 0, 0, 1);
	(void)clone_files(&lwin, NULL, 0, 0, 2);

	assert_success(unlink(SANDBOX_PATH "/a"));
	assert_success(unlink(SANDBOX_PATH "/a(1)"));
	assert_success(unlink(SANDBOX_PATH "/a(2)"));
	assert_success(unlink(SANDBOX_PATH "/a(3)"));
}

TEST(files_are_cloned_with_custom_name)
{
	char *names[] = { "b" };

	create_empty_file(SANDBOX_PATH "/a");
	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)clone_files(&lwin, names, 1, 0, 1);

	assert_success(unlink(SANDBOX_PATH "/a"));
	assert_success(unlink(SANDBOX_PATH "/b"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
