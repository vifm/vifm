#include <stic.h>

#include <unistd.h> /* rmdir() */

#include <string.h> /* strcat() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"
#include "../../src/trash.h"

#include "utils.h"

static char *saved_cwd;

SETUP()
{
	view_setup(&lwin);
	set_to_sandbox_path(lwin.curr_dir, sizeof(lwin.curr_dir));

	create_empty_file(SANDBOX_PATH "/file");
	saved_cwd = save_cwd();
	populate_dir_list(&lwin, 0);
	restore_cwd(saved_cwd);

	cfg.use_trash = 1;
	set_trash_dir(SANDBOX_PATH "/trash");
	lwin.dir_entry[0].marked = 1;
	(void)fops_delete(&lwin, 'a', 1);
}

TEARDOWN()
{
	view_teardown(&lwin);
	assert_success(rmdir(SANDBOX_PATH "/trash"));
}

TEST(files_not_directly_in_trash_are_not_restored)
{
	set_trash_dir(SANDBOX_PATH);

	strcat(lwin.curr_dir, "/trash");
	saved_cwd = save_cwd();
	populate_dir_list(&lwin, 0);
	restore_cwd(saved_cwd);

	lwin.dir_entry[0].marked = 1;
	(void)fops_restore(&lwin);

	assert_success(unlink(SANDBOX_PATH "/trash/000_file"));
}

TEST(generally_restores_files)
{
	set_trash_dir(SANDBOX_PATH "/trash");

	strcat(lwin.curr_dir, "/trash");
	saved_cwd = save_cwd();
	populate_dir_list(&lwin, 0);
	restore_cwd(saved_cwd);

	lwin.dir_entry[0].marked = 1;
	(void)fops_restore(&lwin);

	assert_success(unlink(SANDBOX_PATH "/file"));
}

TEST(works_with_custom_view)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, SANDBOX_PATH "/trash/000_file");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.dir_entry[0].marked = 1;
	(void)fops_restore(&lwin);

	assert_success(unlink(SANDBOX_PATH "/file"));
}

TEST(works_with_tree_view)
{
	flist_load_tree(&lwin, SANDBOX_PATH);

	lwin.dir_entry[1].marked = 1;
	(void)fops_restore(&lwin);

	assert_success(unlink(SANDBOX_PATH "/file"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
