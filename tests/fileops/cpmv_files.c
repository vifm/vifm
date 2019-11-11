#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() unlink() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/path.h"
#include "../../src/filelist.h"
#include "../../src/fops_cpmv.h"
#include "../../src/trash.h"

#include "utils.h"

static void check_directory_clash(int parent_to_child, CopyMoveLikeOp op);

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	set_to_sandbox_path(lwin.curr_dir, sizeof(lwin.curr_dir));
	set_to_sandbox_path(rwin.curr_dir, sizeof(rwin.curr_dir));

	assert_success(chdir(SANDBOX_PATH));

	/* lwin */
	view_setup(&lwin);
	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	view_setup(&rwin);
	rwin.filtered = 0;
	rwin.list_pos = 0;

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	restore_cwd(saved_cwd);
}

TEST(move_file)
{
	char new_fname[] = "new_name";
	char *list[] = { &new_fname[0] };

	create_empty_file(lwin.dir_entry[0].name);

	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_false(path_exists_at(SANDBOX_PATH, lwin.dir_entry[0].name, DEREF));
	assert_success(unlink(SANDBOX_PATH "/new_name"));
}

TEST(make_relative_link, IF(not_windows))
{
	char link_name[] = "link";
	char *list[] = { &link_name[0] };
	char link_value[PATH_MAX + 1];

	strcpy(lwin.curr_dir, "/fake/absolute/path");

	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_LINK_REL, 0);

	assert_true(path_exists(link_name, NODEREF));

	assert_success(get_link_target(link_name, link_value, sizeof(link_value)));
	assert_false(is_path_absolute(link_value));

	(void)unlink(link_name);
}

TEST(make_absolute_link, IF(not_windows))
{
	char link_name[] = "link";
	char *list[] = { &link_name[0] };
	char link_value[PATH_MAX + 1];

	strcpy(lwin.curr_dir, "/fake/absolute/path");

	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_LINK_ABS, 0);

	assert_true(path_exists(link_name, NODEREF));

	assert_success(get_link_target(link_name, link_value, sizeof(link_value)));
	assert_true(is_path_absolute(link_value));

	(void)unlink(link_name);
}

TEST(refuse_to_copy_or_move_to_source_files_with_the_same_name)
{
	assert_false(flist_custom_active(&rwin));

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	strcpy(rwin.curr_dir, saved_cwd);
	flist_custom_start(&rwin, "test");
	flist_custom_add(&rwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&rwin, TEST_DATA_PATH "/rename/a");
	assert_true(flist_custom_finish(&rwin, CV_REGULAR, 0) == 0);
	assert_int_equal(2, rwin.list_rows);

	curr_view = &rwin;
	other_view = &lwin;

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.selected_files = 2;

	check_marking(curr_view, 0, NULL);

	(void)fops_cpmv(&rwin, NULL, 0, CMLO_COPY, 0);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_COPY, 1);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, 0);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, 1);

	assert_false(path_exists("a", NODEREF));
}

TEST(cpmv_crash_on_wrong_list_access)
{
	char *list[] = { "." };

	view_teardown(&lwin);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/existing-files",
				TEST_DATA_PATH);
	}
	else
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s/existing-files",
				saved_cwd, TEST_DATA_PATH);
	}

	lwin.list_rows = 3;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].name = strdup("c");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;

	check_marking(&lwin, 0, NULL);

	/* cpmv used to use presence of the argument as indication of availability of
	 * file list and access memory beyond array boundaries. */
	assert_failure(fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 0));
	assert_failure(fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 1));

	assert_success(remove(SANDBOX_PATH "/a"));
	assert_success(remove(SANDBOX_PATH "/b"));
	assert_success(remove(SANDBOX_PATH "/c"));
}

TEST(cpmv_considers_tree_structure)
{
	char new_fname[] = "new_name";
	char *list[] = { &new_fname[0] };

	create_empty_dir("dir");

	/* Move from tree root to nested dir. */
	create_empty_file("file");
	flist_load_tree(&rwin, rwin.curr_dir);
	rwin.list_pos = 1;
	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, 1, CMLO_MOVE, 0);
	assert_success(unlink("dir/new_name"));

	/* Move back. */
	curr_view = &rwin;
	other_view = &lwin;
	create_empty_file("dir/file");
	flist_load_tree(&lwin, flist_get_dir(&lwin));
	flist_load_tree(&rwin, flist_get_dir(&rwin));
	lwin.list_pos = 0;
	rwin.dir_entry[1].marked = 1;
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, 0);
	assert_success(unlink("file"));

	assert_success(rmdir("dir"));
}

TEST(cpmv_can_move_files_from_and_out_of_trash_at_the_same_time)
{
	int bg;

	strcat(lwin.curr_dir, "/trash");
	set_trash_dir(lwin.curr_dir);
	remove_last_path_component(lwin.curr_dir);

	strcat(lwin.curr_dir, "/dir");

	curr_view = &rwin;
	other_view = &lwin;

	for(bg = 0; bg < 2; ++bg)
	{
		create_empty_dir("trash");
		create_empty_file("trash/000_a");
		create_empty_dir("trash/nested");
		create_empty_file("trash/nested/000_file");
		create_empty_dir("dir");
		create_empty_file("000_b");

		flist_custom_start(&rwin, "test");
		flist_custom_add(&rwin, "trash/000_a");
		flist_custom_add(&rwin, "000_b");
		flist_custom_add(&rwin, "trash/nested/000_file");
		assert_true(flist_custom_finish(&rwin, CV_REGULAR, 0) == 0);
		assert_int_equal(3, rwin.list_rows);

		rwin.dir_entry[0].marked = 1;
		rwin.dir_entry[1].marked = 1;
		rwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, 0);
		}
		else
		{
			(void)fops_cpmv_bg(&rwin, NULL, 0, CMLO_MOVE, 0);
			wait_for_bg();
		}

		assert_success(unlink("dir/a"));
		assert_success(unlink("dir/000_b"));
		assert_success(unlink("dir/000_file"));
		assert_success(rmdir("dir"));
		assert_success(rmdir("trash/nested"));
		assert_success(rmdir("trash"));
	}
}

TEST(child_overwrite_is_prevented_on_abs_link, IF(not_windows))
{
	check_directory_clash(1, CMLO_LINK_ABS);
}

TEST(parent_overwrite_is_prevented_on_abs_link, IF(not_windows))
{
	check_directory_clash(0, CMLO_LINK_ABS);
}

TEST(child_overwrite_is_prevented_on_rel_link, IF(not_windows))
{
	check_directory_clash(1, CMLO_LINK_REL);
}

TEST(parent_overwrite_is_prevented_on_rel_link, IF(not_windows))
{
	check_directory_clash(0, CMLO_LINK_REL);
}

TEST(child_overwrite_is_prevented_on_file_copy)
{
	check_directory_clash(1, CMLO_COPY);
}

TEST(parent_overwrite_is_prevented_on_file_copy)
{
	check_directory_clash(0, CMLO_COPY);
}

TEST(child_overwrite_is_prevented_on_file_move)
{
	check_directory_clash(1, CMLO_MOVE);
}

TEST(parent_overwrite_is_prevented_on_file_move)
{
	check_directory_clash(0, CMLO_MOVE);
}

TEST(copying_is_aborted_if_we_can_not_read_a_file, IF(not_windows))
{
	create_empty_file("can-read");
	create_empty_file("can-not-read");
	assert_success(chmod("can-not-read", 0000));
	populate_dir_list(&lwin, 0);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	assert_int_equal(0, fops_cpmv(&lwin, NULL, 0, CMLO_COPY, 0));

	assert_success(unlink("can-read"));
	assert_success(unlink("can-not-read"));
}

TEST(cpmv_can_copy_or_move_files_to_a_subdirectory)
{
	char dir[] = "dir";
	char *list[] = { dir };

	int bg;
	for(bg = 0; bg < 2; ++bg)
	{
		create_empty_dir("dir");
		create_empty_file("file1");
		create_empty_file("file2");

		populate_dir_list(&lwin, 0);
		assert_int_equal(3, lwin.list_rows);
		assert_string_equal("file1", lwin.dir_entry[1].name);
		assert_string_equal("file2", lwin.dir_entry[2].name);

		/* Copy. */

		lwin.dir_entry[1].marked = 1;
		lwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 0);
		}
		else
		{
			(void)fops_cpmv_bg(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 0);
			wait_for_bg();
		}

		assert_success(unlink("dir/file1"));
		assert_success(unlink("dir/file2"));

		/* Move. */

		lwin.dir_entry[1].marked = 1;
		lwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);
		}
		else
		{
			(void)fops_cpmv_bg(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);
			wait_for_bg();
		}

		assert_success(unlink("dir/file1"));
		assert_success(unlink("dir/file2"));
		assert_success(rmdir("dir"));
	}
}

static void
check_directory_clash(int parent_to_child, CopyMoveLikeOp op)
{
	create_empty_dir("dir");
	create_empty_dir("dir/dir");
	create_empty_file("dir/dir/file");

	strcat(parent_to_child ? rwin.curr_dir : lwin.curr_dir, "/dir");

	populate_dir_list(&lwin, 0);
	populate_dir_list(&rwin, 0);

	lwin.dir_entry[0].marked = 1;
	assert_string_equal("dir", lwin.dir_entry[0].name);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_MOVE, 1);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(remove(SANDBOX_PATH "/dir/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir/dir"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
