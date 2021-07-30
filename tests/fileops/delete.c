#include <stic.h>

#include <unistd.h> /* rmdir() unlink() */

#include <limits.h> /* INT_MAX */
#include <string.h> /* strcat() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/filelist.h"
#include "../../src/fops_common.h"
#include "../../src/fops_misc.h"
#include "../../src/registers.h"
#include "../../src/trash.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
	view_setup(&rwin);

	cfg.use_trash = 0;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
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
			create_file("a");
			create_file("b");

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

			assert_failure(unlink("a"));
			assert_success(unlink("b"));
		}
	}
}

TEST(files_in_trash_are_not_removed_to_trash)
{
	cfg.use_trash = 1;
	trash_set_specs(lwin.curr_dir);

	create_file("a");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink("a"));
}

TEST(trash_is_not_removed_to_trash)
{
	cfg.use_trash = 1;
	trash_set_specs("trash");

	create_dir("trash");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(rmdir("trash"));
}

TEST(marked_files_are_removed_to_trash)
{
	char trash_dir[PATH_MAX + 1];
	make_abs_path(trash_dir, sizeof(trash_dir), SANDBOX_PATH, "trash", saved_cwd);

	cfg.use_trash = 1;
	trash_set_specs(trash_dir);
	assert_success(rmdir("trash"));

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		int bg;
		for(bg = 0; bg < 2; ++bg)
		{
			create_dir("trash");

			create_file("a");
			create_file("b");

			populate_dir_list(&lwin, 0);
			lwin.dir_entry[2].marked = 1;

			restore_cwd(saved_cwd);
			saved_cwd = save_cwd();
			assert_success(chdir(SANDBOX_PATH));

			if(!bg)
			{
				(void)fops_delete(&lwin, 'x', 1);
			}
			else
			{
				(void)fops_delete_bg(&lwin, 1);
				wait_for_bg();
			}

			assert_success(unlink("a"));
			assert_failure(unlink("b"));

			assert_success(unlink("trash/000_b"));
			assert_success(rmdir("trash"));
		}
	}
}

TEST(nested_file_is_removed)
{
	cfg.use_trash = 1;
	trash_set_specs("trash");

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
					create_dir("trash");
				}
				create_dir("dir");

				create_file("dir/a");
				create_file("dir/b");

				make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
						saved_cwd);
				assert_success(flist_load_tree(&lwin, lwin.curr_dir, INT_MAX));
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

				restore_cwd(saved_cwd);
				saved_cwd = save_cwd();
				assert_success(chdir(SANDBOX_PATH));

				assert_success(unlink("dir/a"));
				assert_failure(unlink("dir/b"));
				assert_success(rmdir("dir"));

				if(to_trash)
				{
					assert_success(unlink("trash/000_b"));
					assert_success(rmdir("trash"));
				}
			}
		}
	}
}

TEST(files_in_trash_are_not_removed_to_trash_in_cv)
{
	cfg.use_trash = 1;
	strcat(lwin.curr_dir, "/dir");
	trash_set_specs(lwin.curr_dir);
	remove_last_path_component(lwin.curr_dir);

	create_file("dir/a");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "dir");
	flist_custom_add(&lwin, "dir/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.dir_entry[1].marked = 1;
	lwin.list_pos = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink("dir/a"));
	assert_success(rmdir("dir"));
}

TEST(files_in_trash_are_not_removed_to_trash_in_tree)
{
	cfg.use_trash = 1;
	trash_set_specs("dir");

	create_dir("dir");
	create_file("dir/a");

	assert_success(flist_load_tree(&lwin, ".", INT_MAX));
	lwin.dir_entry[1].marked = 1;
	lwin.list_pos = 1;

	(void)fops_delete(&lwin, '\0', 1);
	(void)fops_delete_bg(&lwin, 1);
	wait_for_bg();

	assert_success(unlink("dir/a"));
	assert_success(rmdir("dir"));
}

TEST(trash_is_not_checked_on_permanent_bg_remove)
{
	assert_success(trash_set_specs(lwin.curr_dir));
	create_dir("dir");

	populate_dir_list(&lwin, 0);
	lwin.dir_entry[0].marked = 1;

	(void)fops_delete_bg(&lwin, 0);
	wait_for_bg();

	assert_failure(rmdir("dir"));
}

TEST(empty_directory_is_removed)
{
	/* Make sure I/O notification is registered. */
	fops_init(NULL, NULL);

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		int bg;
		for(bg = 0; bg < 2; ++bg)
		{
			create_dir("dir");

			populate_dir_list(&lwin, 0);
			lwin.dir_entry[0].marked = 1;

			if(!bg)
			{
				(void)fops_delete(&lwin, 'x', 0);
			}
			else
			{
				(void)fops_delete_bg(&lwin, 0);
				wait_for_bg();
			}

			assert_failure(rmdir("dir"));
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
