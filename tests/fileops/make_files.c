#include <stic.h>

#include <unistd.h> /* chdir() rmdir() unlink() */

#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fileops.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	set_to_sandbox_path(lwin.curr_dir, sizeof(lwin.curr_dir));
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
}

TEST(make_files_fails_on_empty_file_name)
{
	char name[] = "";
	char *names[] = { name };

	assert_true(make_files(&lwin, names, 1));
}

TEST(make_files_fails_on_name_with_slash)
{
	char name[] = "a/";
	char *names[] = { name };

	assert_true(make_files(&lwin, names, 1));
	assert_failure(unlink("a"));
}

TEST(make_files_fails_on_file_name_dups)
{
	char name[] = "name";
	char *names[] = { name, name };

	assert_true(make_files(&lwin, names, 2));
	assert_failure(unlink(name));
}

TEST(make_files_fails_if_file_exists)
{
	char name[] = "a";
	char *names[] = { name };

	create_empty_file("a");

	assert_true(make_files(&lwin, names, 1));

	assert_success(unlink("a"));
}

TEST(make_files_creates_files)
{
	char name_a[] = "a";
	char name_b[] = "b";
	char *names[] = { name_a, name_b };

	(void)make_files(&lwin, names, 2);

	assert_success(unlink("a"));
	assert_success(unlink("b"));
}

TEST(make_files_considers_tree_structure)
{
	char name[] = "new-file";
	char *names[] = { name };

	view_setup(&lwin);

	create_empty_dir("dir");

	flist_load_tree(&lwin, ".");

	lwin.list_pos = 0;
	(void)make_files(&lwin, names, 1);

	lwin.list_pos = 1;
	(void)make_files(&lwin, names, 1);

	/* Remove both files afterward to make sure they can both be created at the
	 * same time. */
	assert_success(unlink("new-file"));
	assert_success(unlink("dir/new-file"));

	assert_success(rmdir("dir"));

	view_teardown(&lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
