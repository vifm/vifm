#include <stic.h>

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
#include "../../src/fileops.h"
#include "../../src/trash.h"

#include "utils.h"

static int not_windows(void);

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	if(is_path_absolute(SANDBOX_PATH))
	{
		strcpy(lwin.curr_dir, SANDBOX_PATH);
		strcpy(rwin.curr_dir, SANDBOX_PATH);
	}
	else
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s", saved_cwd,
				SANDBOX_PATH);
		snprintf(rwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s", saved_cwd,
				SANDBOX_PATH);
	}

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

	FILE *const f = fopen(lwin.dir_entry[0].name, "w");
	fclose(f);

	assert_true(path_exists(lwin.dir_entry[0].name, DEREF));

	lwin.dir_entry[0].marked = 1;
	(void)cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_MOVE, 0);

	assert_false(path_exists(lwin.dir_entry[0].name, DEREF));
	assert_true(path_exists(new_fname, DEREF));

	(void)unlink(new_fname);
}

TEST(make_relative_link, IF(not_windows))
{
	char link_name[] = "link";
	char *list[] = { &link_name[0] };
	char link_value[PATH_MAX];

	strcpy(lwin.curr_dir, "/fake/absolute/path");

	lwin.dir_entry[0].marked = 1;
	(void)cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_LINK_REL, 0);

	assert_true(path_exists(link_name, NODEREF));

	assert_success(get_link_target(link_name, link_value, sizeof(link_value)));
	assert_false(is_path_absolute(link_value));

	(void)unlink(link_name);
}

TEST(make_absolute_link, IF(not_windows))
{
	char link_name[] = "link";
	char *list[] = { &link_name[0] };
	char link_value[PATH_MAX];

	strcpy(lwin.curr_dir, "/fake/absolute/path");

	lwin.dir_entry[0].marked = 1;
	(void)cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_LINK_ABS, 0);

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
	assert_true(flist_custom_finish(&rwin, 0, 0) == 0);
	assert_int_equal(2, rwin.list_rows);

	assert_success(chdir(SANDBOX_PATH));

	curr_view = &rwin;
	other_view = &lwin;

	rwin.dir_entry[0].marked = 1;
	rwin.dir_entry[1].marked = 1;
	rwin.selected_files = 2;

	check_marking(curr_view, 0, NULL);

	(void)cpmv_files(&rwin, NULL, 0, CMLO_COPY, 0);
	(void)cpmv_files(&rwin, NULL, 0, CMLO_COPY, 1);
	(void)cpmv_files(&rwin, NULL, 0, CMLO_MOVE, 0);
	(void)cpmv_files(&rwin, NULL, 0, CMLO_MOVE, 1);

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
	assert_failure(cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 0));
	assert_failure(cpmv_files(&lwin, list, ARRAY_LEN(list), CMLO_COPY, 1));

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
	(void)cpmv_files(&lwin, list, 1, CMLO_MOVE, 0);
	assert_success(unlink("dir/new_name"));

	/* Move back. */
	curr_view = &rwin;
	other_view = &lwin;
	create_empty_file("dir/file");
	flist_load_tree(&lwin, flist_get_dir(&lwin));
	flist_load_tree(&rwin, flist_get_dir(&rwin));
	lwin.list_pos = 0;
	rwin.dir_entry[1].marked = 1;
	(void)cpmv_files(&rwin, NULL, 0, CMLO_MOVE, 0);
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
		assert_true(flist_custom_finish(&rwin, 0, 0) == 0);
		assert_int_equal(3, rwin.list_rows);

		rwin.dir_entry[0].marked = 1;
		rwin.dir_entry[1].marked = 1;
		rwin.dir_entry[2].marked = 1;

		if(!bg)
		{
			(void)cpmv_files(&rwin, NULL, 0, CMLO_MOVE, 0);
		}
		else
		{
			(void)cpmv_files_bg(&rwin, NULL, 0, CMLO_MOVE, 0);
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

static int
not_windows(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
