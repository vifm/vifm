#include <stic.h>

#include <unistd.h> /* rmdir() unlink() */

#include <string.h> /* strcat() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"
#include "../../src/registers.h"
#include "../../src/trash.h"

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

TEST(marked_files_are_removed_permanently)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		int bg;
		for(bg = 0; bg < 2; ++bg)
		{
			create_empty_file(SANDBOX_PATH "/a");
			create_empty_file(SANDBOX_PATH "/b");

			populate_dir_list(&lwin, 0);
			lwin.dir_entry[0].marked = 1;

			if(!bg)
			{
				(void)fops_delete(&lwin, '\0', 0);
			}
			else
			{
				(void)fops_delete_bg(&lwin, 0);
				wait_for_bg();
			}

			assert_failure(unlink(SANDBOX_PATH "/a"));
			assert_success(unlink(SANDBOX_PATH "/b"));
		}
	}
}

TEST(files_in_trash_are_not_removed_to_trash)
{
	cfg.use_trash = 1;
	set_trash_dir(lwin.curr_dir);

	create_empty_file(SANDBOX_PATH "/a");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/a"));
}

TEST(trash_is_not_removed_to_trash)
{
	cfg.use_trash = 1;
	set_trash_dir(SANDBOX_PATH "/trash");

	create_empty_dir(SANDBOX_PATH "/trash");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(rmdir(SANDBOX_PATH "/trash"));
}

TEST(marked_files_are_removed_to_trash)
{
	cfg.use_trash = 1;
	set_trash_dir(SANDBOX_PATH "/trash");

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		int bg;
		for(bg = 0; bg < 2; ++bg)
		{
			create_empty_dir(SANDBOX_PATH "/trash");

			create_empty_file(SANDBOX_PATH "/a");
			create_empty_file(SANDBOX_PATH "/b");

			populate_dir_list(&lwin, 0);
			lwin.dir_entry[2].marked = 1;

			if(!bg)
			{
				(void)fops_delete(&lwin, 'x', 1);
			}
			else
			{
				(void)fops_delete_bg(&lwin, 1);
				wait_for_bg();
			}

			assert_success(unlink(SANDBOX_PATH "/a"));
			assert_failure(unlink(SANDBOX_PATH "/b"));

			assert_success(unlink(SANDBOX_PATH "/trash/000_b"));
			assert_success(rmdir(SANDBOX_PATH "/trash"));
		}
	}
}

TEST(nested_file_is_removed)
{
	cfg.use_trash = 1;
	set_trash_dir(SANDBOX_PATH "/trash");

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		int bg;
		for(bg = 0; bg < 2; ++bg)
		{
			int to_trash;
			for(to_trash = 0; to_trash < 2; ++to_trash)
			{
				if(to_trash)
				{
					create_empty_dir(SANDBOX_PATH "/trash");
				}
				create_empty_dir(SANDBOX_PATH "/dir");

				create_empty_file(SANDBOX_PATH "/dir/a");
				create_empty_file(SANDBOX_PATH "/dir/b");

				flist_load_tree(&lwin, SANDBOX_PATH);
				lwin.dir_entry[2].marked = 1;

				if(!bg)
				{
					(void)fops_delete(&lwin, 'x', to_trash);
				}
				else
				{
					(void)fops_delete_bg(&lwin, to_trash);
					wait_for_bg();
				}

				assert_success(unlink(SANDBOX_PATH "/dir/a"));
				assert_failure(unlink(SANDBOX_PATH "/dir/b"));
				assert_success(rmdir(SANDBOX_PATH "/dir"));

				if(to_trash)
				{
					assert_success(unlink(SANDBOX_PATH "/trash/000_b"));
					assert_success(rmdir(SANDBOX_PATH "/trash"));
				}
			}
		}
	}
}

TEST(files_in_trash_are_not_removed_to_trash_in_cv)
{
	cfg.use_trash = 1;
	strcat(lwin.curr_dir, "/dir");
	set_trash_dir(lwin.curr_dir);
	remove_last_path_component(lwin.curr_dir);

	create_empty_dir(SANDBOX_PATH "/dir");
	create_empty_file(SANDBOX_PATH "/dir/a");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, SANDBOX_PATH "/dir");
	flist_custom_add(&lwin, SANDBOX_PATH "/dir/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.dir_entry[1].marked = 1;
	lwin.list_pos = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(files_in_trash_are_not_removed_to_trash_in_tree)
{
	cfg.use_trash = 1;
	strcat(lwin.curr_dir, "/dir");
	set_trash_dir(lwin.curr_dir);

	create_empty_dir(SANDBOX_PATH "/dir");
	create_empty_file(SANDBOX_PATH "/dir/a");

	flist_load_tree(&lwin, SANDBOX_PATH);
	lwin.dir_entry[1].marked = 1;
	lwin.list_pos = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink(SANDBOX_PATH "/dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
