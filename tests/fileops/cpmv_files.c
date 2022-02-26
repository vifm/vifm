#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() unlink() */

#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_cpmv.h"
#include "../../src/trash.h"

static void check_directory_clash(int parent_to_child, CopyMoveLikeOp op);

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	/* lwin */
	view_setup(&lwin);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	view_setup(&rwin);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
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

	create_file(lwin.dir_entry[0].name);

	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, CMLF_NONE);

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
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_LINK_REL, CMLF_NONE);

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
	(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_LINK_ABS, CMLF_NONE);

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
	rwin.pending_marking = 1;

	check_marking(curr_view, 0, NULL);

	(void)fops_cpmv(&rwin, NULL, 0, CMLO_COPY, CMLF_NONE);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_COPY, CMLF_FORCE);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, CMLF_NONE);
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, CMLF_FORCE);

	assert_false(path_exists("a", NODEREF));
}

TEST(operation_with_rename_of_identically_named_files_is_allowed)
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
	rwin.pending_marking = 1;

	check_marking(curr_view, 0, NULL);

	char *list[] = { "a", "b" };
	char a_path[PATH_MAX + 1], b_path[PATH_MAX + 1];
	make_abs_path(a_path, sizeof(a_path), SANDBOX_PATH, "a", saved_cwd);
	make_abs_path(b_path, sizeof(b_path), SANDBOX_PATH, "b", saved_cwd);

	/* Not testing moving for simplicity. */
	(void)fops_cpmv(&rwin, list, ARRAY_LEN(list), CMLO_COPY, CMLF_NONE);
	remove_file(a_path);
	remove_file(b_path);
	(void)fops_cpmv(&rwin, list, ARRAY_LEN(list), CMLO_COPY, CMLF_FORCE);
	remove_file(a_path);
	remove_file(b_path);
}

TEST(cpmv_crash_on_wrong_list_access)
{
	char *list[] = { "." };

	view_teardown(&lwin);
	view_setup(&lwin);

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
	assert_failure(fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY, CMLF_NONE));
	assert_failure(fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY,
				CMLF_FORCE));

	assert_success(remove(SANDBOX_PATH "/a"));
	assert_success(remove(SANDBOX_PATH "/b"));
	assert_success(remove(SANDBOX_PATH "/c"));
}

TEST(cpmv_considers_tree_structure)
{
	char new_fname[] = "new_name";
	char *list[] = { &new_fname[0] };

	cfg.dot_dirs = DD_TREE_LEAFS_PARENT;
	create_dir("dir");

	/* Move from tree root to nested dir. */
	create_file("file");
	flist_load_tree(&rwin, rwin.curr_dir, INT_MAX);
	rwin.list_pos = 1;
	lwin.dir_entry[0].marked = 1;
	(void)fops_cpmv(&lwin, list, 1, CMLO_MOVE, CMLF_NONE);
	assert_success(unlink("dir/new_name"));

	/* Move back. */
	curr_view = &rwin;
	other_view = &lwin;
	create_file("dir/file");
	flist_load_tree(&lwin, flist_get_dir(&lwin), INT_MAX);
	flist_load_tree(&rwin, flist_get_dir(&rwin), INT_MAX);
	lwin.list_pos = 0;
	rwin.dir_entry[1].marked = 1;
	(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, CMLF_NONE);
	assert_success(unlink("file"));

	assert_success(rmdir("dir"));
}

TEST(cpmv_can_move_files_from_and_out_of_trash_at_the_same_time)
{
	int bg;

	strcat(lwin.curr_dir, "/trash");
	trash_set_specs(lwin.curr_dir);
	assert_success(rmdir("trash"));
	remove_last_path_component(lwin.curr_dir);

	strcat(lwin.curr_dir, "/dir");

	curr_view = &rwin;
	other_view = &lwin;

	for(bg = 0; bg < 2; ++bg)
	{
		create_dir("trash");
		create_file("trash/000_a");
		create_dir("trash/nested");
		create_file("trash/nested/000_file");
		create_dir("dir");
		create_file("000_b");

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
			(void)fops_cpmv(&rwin, NULL, 0, CMLO_MOVE, CMLF_NONE);
		}
		else
		{
			(void)fops_cpmv_bg(&rwin, NULL, 0, CMLO_MOVE, CMLF_NONE);
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

TEST(copying_is_aborted_if_we_can_not_read_a_file, IF(regular_unix_user))
{
	create_file("can-read");
	create_file("can-not-read");
	assert_success(chmod("can-not-read", 0000));
	populate_dir_list(&lwin, 0);

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	assert_int_equal(0, fops_cpmv(&lwin, NULL, 0, CMLO_COPY, CMLF_NONE));

	assert_success(unlink("can-read"));
	assert_success(unlink("can-not-read"));
}

TEST(bail_out_if_source_matches_destination)
{
	create_file("file");

	populate_dir_list(&lwin, 0);
	populate_dir_list(&rwin, 0);

	lwin.dir_entry[0].marked = 1;

	(void)fops_cpmv(&lwin, NULL, 0, CMLO_COPY, CMLF_NONE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_COPY, CMLF_FORCE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_MOVE, CMLF_NONE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_MOVE, CMLF_FORCE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_LINK_REL, CMLF_NONE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_LINK_REL, CMLF_FORCE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_LINK_ABS, CMLF_NONE);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_LINK_ABS, CMLF_FORCE);

	(void)fops_cpmv_bg(&lwin, NULL, 0, /*move=*/0, CMLF_NONE);
	wait_for_bg();
	(void)fops_cpmv_bg(&lwin, NULL, 0, /*move=*/0, CMLF_FORCE);
	wait_for_bg();
	(void)fops_cpmv_bg(&lwin, NULL, 0, /*move=*/1, CMLF_NONE);
	wait_for_bg();
	(void)fops_cpmv_bg(&lwin, NULL, 0, /*move=*/1, CMLF_FORCE);
	wait_for_bg();

	remove_file("file");
}

TEST(cpmv_can_copy_or_move_files_to_a_subdirectory)
{
	char dir[] = "dir";
	char *list[] = { dir };

	int bg;
	for(bg = 0; bg < 2; ++bg)
	{
		create_dir("dir");
		create_file("file1");
		create_file("file2");

		populate_dir_list(&lwin, 0);
		assert_int_equal(3, lwin.list_rows);
		assert_string_equal("file1", lwin.dir_entry[1].name);
		assert_string_equal("file2", lwin.dir_entry[2].name);

		/* Copy. */

		lwin.dir_entry[1].marked = 1;
		lwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_COPY, CMLF_NONE);
		}
		else
		{
			(void)fops_cpmv_bg(&lwin, list, ARRAY_LEN(list), CMLO_COPY, CMLF_NONE);
			wait_for_bg();
		}

		assert_success(unlink("dir/file1"));
		assert_success(unlink("dir/file2"));

		/* Move. */

		lwin.dir_entry[1].marked = 1;
		lwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, CMLF_NONE);
		}
		else
		{
			(void)fops_cpmv_bg(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, CMLF_NONE);
			wait_for_bg();
		}

		assert_success(unlink("dir/file1"));
		assert_success(unlink("dir/file2"));
		assert_success(rmdir("dir"));
	}
}

TEST(can_skip_existing_files)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "dir",
			saved_cwd);

	int bg;
	for(bg = 0; bg < 2; ++bg)
	{
		create_dir("dir");
		make_file("dir/file", "aaa");
		create_file("file");

		CopyMoveLikeOp ops[] = {
			CMLO_COPY,
			CMLO_MOVE,
			CMLO_LINK_REL,
			CMLO_LINK_ABS,
		};

		size_t op;
		for(op = 0; op < ARRAY_LEN(ops); ++op)
		{
			lwin.dir_entry[0].marked = 1;

			if(!bg)
			{
				(void)fops_cpmv(&lwin, NULL, 0, ops[op], CMLF_SKIP);
			}
			else
			{
				(void)fops_cpmv_bg(&lwin, NULL, 0, ops[op], CMLF_SKIP);
				wait_for_bg();
			}

			assert_int_equal(0, get_file_size("file"));
		}

		remove_file("file");
		remove_file("dir/file");
		remove_dir("dir");
	}
}

TEST(broken_link_behaves_like_a_regular_file_on_conflict, IF(not_windows))
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "src",
			saved_cwd);
	replace_string(&lwin.dir_entry[0].name, "symlink");

	int bg;
	for(bg = 0; bg < 2; ++bg)
	{
		create_dir("src");
		create_file("src/symlink");
		assert_success(make_symlink("notarget", "symlink"));

		lwin.dir_entry[0].marked = 1;

		if(!bg)
		{
			(void)fops_cpmv(&lwin, NULL, 0, CMLO_MOVE, CMLF_SKIP);
		}
		else
		{
			(void)fops_cpmv_bg(&lwin, NULL, 0, CMLO_MOVE, CMLF_SKIP);
			wait_for_bg();
		}

		remove_file("src/symlink");
		remove_dir("src");
		remove_file("symlink");
	}
}

static void
check_directory_clash(int parent_to_child, CopyMoveLikeOp op)
{
	create_dir("dir");
	create_dir("dir/dir");
	create_file("dir/dir/file");

	strcat(parent_to_child ? rwin.curr_dir : lwin.curr_dir, "/dir");

	populate_dir_list(&lwin, 0);
	populate_dir_list(&rwin, 0);

	lwin.dir_entry[0].marked = 1;
	assert_string_equal("dir", lwin.dir_entry[0].name);
	(void)fops_cpmv(&lwin, NULL, 0, CMLO_MOVE, CMLF_FORCE);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(remove(SANDBOX_PATH "/dir/dir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir/dir"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
