#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"

#include "utils.h"

#define INITIAL_SIZE 10

/* This should be a macro to see what test have failed. */
#define VALIDATE_HISTORY(i, str) \
	assert_string_equal(str, cfg.cmd_hist.items[i]); \
	assert_string_equal(str, cfg.search_hist.items[i]); \
	assert_string_equal(str, cfg.prompt_hist.items[i]); \
	\
	assert_string_equal(str, lwin.history[(i) + 1].dir); \
	assert_string_equal((str) + 1, lwin.history[(i) + 1].file); \
	\
	assert_string_equal(str, rwin.history[(i) + 1].dir); \
	assert_string_equal((str) + 1, rwin.history[(i) + 1].file);

SETUP()
{
	update_string(&cfg.slow_fs_list, "");

	view_setup(&lwin);

	/* lwin */
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 1;
	rwin.list_pos = 0;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(INITIAL_SIZE);
	cfg_resize_histories(0);

	cfg_resize_histories(INITIAL_SIZE);

	curr_view = NULL;
}

TEARDOWN()
{
	int i;

	update_string(&cfg.slow_fs_list, NULL);

	cfg_resize_histories(0);

	view_teardown(&lwin);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	dynarray_free(rwin.dir_entry);
}

static void
save_to_history(const char str[])
{
	cfg_save_command_history(str);
	cfg_save_search_history(str);
	cfg_save_prompt_history(str);

	save_view_history(&lwin, str, str + 1, 0);
	save_view_history(&rwin, str, str + 1, 0);
}

TEST(view_history_after_reset_contains_valid_data)
{
	assert_string_equal("/lwin", lwin.history[0].dir);
	assert_string_equal("lfile0", lwin.history[0].file);

	assert_string_equal("/rwin", rwin.history[0].dir);
	assert_string_equal("rfile0", rwin.history[0].file);
}

TEST(view_history_avoids_duplicates)
{
	assert_int_equal(1, lwin.history_num);
	assert_int_equal(1, rwin.history_num);

	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);

	assert_int_equal(1, lwin.history_num);
	assert_int_equal(1, rwin.history_num);
}

TEST(after_history_reset_ok)
{
	const char *const str = "string";
	save_to_history(str);
	VALIDATE_HISTORY(0, str);
}

TEST(add_after_decreasing_ok)
{
	const char *const str = "longstringofmeaninglesstext";
	int i;

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}

	cfg_resize_histories(INITIAL_SIZE/2);

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}
}

TEST(add_after_increasing_ok)
{
	const char *const str = "longstringofmeaninglesstext";
	int i;

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}

	cfg_resize_histories(INITIAL_SIZE*2);

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}
}

TEST(navigating_within_history)
{
	save_to_history(SANDBOX_PATH);
	save_to_history(TEST_DATA_PATH);

	navigate_backward_in_history(&lwin);

	/* This overwrites previously active history entries and should not result in
	 * memory leaks. */
	save_to_history("somewhere");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
