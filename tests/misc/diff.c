#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* EOF FILE fclose() fgetc() fopen() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/compare.h"
#include "../../src/event_loop.h"
#include "../../src/ops.h"

static void column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info);
static int files_are_identical(const char a[], const char b[]);

static char *saved_cwd;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	cfg.use_system_calls = 1;
	cfg.sizefmt.base = 1;

	undo_setup();

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();

	undo_teardown();
}

TEST(moving_does_not_work_in_non_diff)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	(void)compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, 0);

	(void)compare_move(&lwin, &rwin);

	assert_true(files_are_identical(
				TEST_DATA_PATH "/compare/a/same-content-different-name-1",
				TEST_DATA_PATH "/compare/b/same-content-different-name-1"));
}

TEST(moving_fake_entry_removes_the_other_file)
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	create_file(SANDBOX_PATH "/empty");

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 4;
	lwin.list_pos = 4;
	(void)compare_move(&lwin, &rwin);

	assert_failure(remove(SANDBOX_PATH "/empty"));
}

TEST(moving_to_fake_entry_creates_the_other_file_and_entry_is_updated)
{
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"compare/b", saved_cwd);

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 3;
	lwin.list_pos = 3;
	(void)compare_move(&lwin, &rwin);

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_ID);
	columns_set_line_print_func(&column_line_print);
	lwin.columns = columns_create();
	rwin.columns = columns_create();
	lwin.column_count = 1;
	rwin.column_count = 1;
	curr_stats.load_stage = 2;

	assert_true(process_scheduled_updates_of_view(&lwin));
	assert_true(process_scheduled_updates_of_view(&rwin));

	curr_stats.load_stage = 0;
	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_free(rwin.columns);
	rwin.columns = NULL;
	columns_teardown();

	assert_true(lwin.dir_entry[3].id == rwin.dir_entry[3].id);
	assert_true(lwin.dir_entry[3].size == rwin.dir_entry[3].size);
	assert_true(lwin.dir_entry[3].mtime == rwin.dir_entry[3].mtime);

	assert_success(remove(SANDBOX_PATH "/same-name-same-content"));
}

static void
column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

TEST(moving_mismatched_entry_makes_files_equal)
{
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"compare/b", saved_cwd);

	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-different-content");

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 2;
	lwin.list_pos = 2;
	(void)compare_move(&lwin, &rwin);

	assert_true(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	assert_success(remove(SANDBOX_PATH "/same-name-different-content"));
}

TEST(moving_equal_does_nothing)
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content");

	assert_true(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);

	/* Replace file unbeknownst to main code. */
	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-same-content");
	assert_false(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	rwin.list_pos = 3;
	lwin.list_pos = 3;
	(void)compare_move(&lwin, &rwin);

	/* Check that file wasn't replaced. */
	assert_false(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	assert_success(remove(SANDBOX_PATH "/same-name-same-content"));
}

TEST(file_id_is_not_updated_on_failed_move, IF(regular_unix_user))
{
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "",
			saved_cwd);
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"compare/b", saved_cwd);

	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-different-content");

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 2;
	lwin.list_pos = 2;

	assert_success(chmod(SANDBOX_PATH, 0000));
	(void)compare_move(&lwin, &rwin);
	assert_success(chmod(SANDBOX_PATH, 0777));

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));
	assert_false(lwin.dir_entry[2].id == rwin.dir_entry[2].id);

	assert_success(remove(SANDBOX_PATH "/same-name-different-content"));
}

static int
files_are_identical(const char a[], const char b[])
{
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");
	int a_data, b_data;

	if(a_file == NULL || b_file == NULL)
	{
		if(a_file != NULL)
		{
			fclose(a_file);
		}
		if(b_file != NULL)
		{
			fclose(b_file);
		}
		return 0;
	}

	do
	{
		a_data = fgetc(a_file);
		b_data = fgetc(b_file);
	}
	while(a_data != EOF && b_data != EOF);

	fclose(b_file);
	fclose(a_file);

	return a_data == b_data && a_data == EOF;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
