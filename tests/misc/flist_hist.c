#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/flist_hist.h"

#define INITIAL_SIZE 10

/* This should be a macro to see what test have failed. */
#define VALIDATE_HISTORY(i, str) \
	assert_string_equal(str, curr_stats.cmd_hist.items[i].text); \
	assert_string_equal(str, curr_stats.menucmd_hist.items[i].text); \
	assert_string_equal(str, curr_stats.search_hist.items[i].text); \
	assert_string_equal(str, curr_stats.prompt_hist.items[i].text); \
	assert_string_equal(str, curr_stats.filter_hist.items[i].text); \
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
	lwin.top_line = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	/* rwin */
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 1;
	rwin.list_pos = 0;
	rwin.top_line = 0;
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
	hists_commands_save(str);
	hists_menucmd_save(str);
	hists_search_save(str);
	hists_prompt_save(str);
	hists_filter_save(str);

	flist_hist_setup(&lwin, str, str + 1, 0, 1);
	flist_hist_setup(&rwin, str, str + 1, 0, 1);
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

	flist_hist_save(&lwin);
	flist_hist_save(&rwin);

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

	flist_hist_go_back(&lwin);

	/* This overwrites previously active history entries and should not result in
	 * memory leaks. */
	save_to_history("somewhere");
}

TEST(specified_file_position_is_unaffected_by_top_line)
{
	lwin.top_line = 3;
	flist_hist_setup(&lwin, "/dir", "file", 0, 1);
	assert_string_equal("/dir", lwin.history[1].dir);
	assert_string_equal("file", lwin.history[1].file);
	assert_int_equal(0, lwin.history[1].rel_pos);
}

TEST(history_size_reduction_leaves_correct_number_of_elements)
{
	assert_int_equal(1, lwin.history_num);
	flist_hist_setup(&lwin, "/dir1", "file1", 1, 1);
	flist_hist_setup(&lwin, "/dir2", "file2", 2, 1);
	assert_int_equal(2, lwin.history_pos);
	assert_int_equal(3, lwin.history_num);

	cfg_resize_histories(2);

	assert_int_equal(1, lwin.history_pos);
	assert_int_equal(2, lwin.history_num);

	assert_string_equal("/dir1", lwin.history[0].dir);
	assert_string_equal("file1", lwin.history[0].file);
	assert_int_equal(1, lwin.history[0].rel_pos);
	assert_string_equal("/dir2", lwin.history[1].dir);
	assert_string_equal("file2", lwin.history[1].file);
	assert_int_equal(2, lwin.history[1].rel_pos);
}

TEST(find_with_disabled_history)
{
	entries_t entries = {};
	int top, pos;

	cfg_resize_histories(0);
	pos = flist_hist_find(&lwin, entries, "/bin", &top);

	assert_int_equal(0, top);
	assert_int_equal(0, pos);
}

TEST(defaults_are_returned_on_dir_lookup_failure)
{
	entries_t entries = {};
	int top, pos;

	flist_hist_setup(&lwin, "/bin", "sh", 0, 1);

	pos = flist_hist_find(&lwin, entries, "/etc", &top);

	assert_int_equal(0, top);
	assert_int_equal(0, pos);
}

TEST(defaults_are_is_returned_on_file_lookup_failure)
{
	dir_entry_t entry_list[] = { { .name = "a" }, { .name = "b" } };
	entries_t entries = { entry_list, 2 };
	int top, pos;

	flist_hist_setup(&lwin, "/bin", "x", 0, 1);

	pos = flist_hist_find(&lwin, entries, "/bin", &top);

	assert_int_equal(0, top);
	assert_int_equal(0, pos);
}

TEST(history_item_is_found)
{
	dir_entry_t entry_list[] = { { .name = "a" }, { .name = "b" } };
	entries_t entries = { entry_list, 2 };
	int top, pos;

	flist_hist_setup(&lwin, "/bin", "b", 0, 1);

	pos = flist_hist_find(&lwin, entries, "/bin", &top);

	assert_int_equal(1, top);
	assert_int_equal(1, pos);
}

TEST(top_value_can_not_be_negative)
{
	dir_entry_t entry_list[] = { { .name = "a" }, { .name = "b" } };
	entries_t entries = { entry_list, 2 };
	int top, pos;

	flist_hist_setup(&lwin, "/bin", "b", 2, 1);

	pos = flist_hist_find(&lwin, entries, "/bin", &top);

	assert_int_equal(0, top);
	assert_int_equal(1, pos);
}

TEST(hist_update_performs_update_but_does_not_move_entry)
{
	flist_hist_setup(&lwin, "/bin", "a", 2, 1);
	flist_hist_setup(&lwin, "/etc", "b", 4, 1);

	flist_hist_update(&lwin, "/bin", "g", 8);

	assert_string_equal("/bin", lwin.history[1].dir);
	assert_string_equal("g", lwin.history[1].file);
	assert_int_equal(8, lwin.history[1].rel_pos);
	assert_string_equal("/etc", lwin.history[2].dir);
	assert_string_equal("b", lwin.history[2].file);
	assert_int_equal(4, lwin.history[2].rel_pos);
}

TEST(history_without_suffix_is_cloned)
{
	assert_int_equal(1, lwin.history_num);
	flist_hist_setup(&lwin, "/dir1", "file1", 1, 1);
	flist_hist_setup(&lwin, "/dir2", "file2", 2, 1);

	flist_hist_clone(&rwin, &lwin);

	assert_int_equal(2, rwin.history_pos);
	assert_int_equal(3, rwin.history_num);

	assert_string_equal("/lwin", rwin.history[0].dir);
	assert_string_equal("lfile0", rwin.history[0].file);
	assert_int_equal(0, rwin.history[0].rel_pos);
	assert_string_equal("/dir1", rwin.history[1].dir);
	assert_string_equal("file1", rwin.history[1].file);
	assert_int_equal(1, rwin.history[1].rel_pos);
	assert_string_equal("/dir2", rwin.history[2].dir);
	assert_string_equal("file2", rwin.history[2].file);
	assert_int_equal(2, rwin.history[2].rel_pos);
}

TEST(history_with_suffix_is_cloned)
{
	assert_int_equal(1, lwin.history_num);
	flist_hist_setup(&lwin, "/dir1", "file1", 1, 1);
	flist_hist_setup(&lwin, "/dir2", "file2", 2, 1);

	lwin.history_pos = 1;

	flist_hist_clone(&rwin, &lwin);

	assert_int_equal(1, rwin.history_pos);
	assert_int_equal(3, rwin.history_num);

	assert_string_equal("/lwin", rwin.history[0].dir);
	assert_string_equal("lfile0", rwin.history[0].file);
	assert_int_equal(0, rwin.history[0].rel_pos);
	assert_string_equal("/dir1", rwin.history[1].dir);
	assert_string_equal("file1", rwin.history[1].file);
	assert_int_equal(1, rwin.history[1].rel_pos);
	assert_string_equal("/dir2", rwin.history[2].dir);
	assert_string_equal("file2", rwin.history[2].file);
	assert_int_equal(2, rwin.history[2].rel_pos);
}

TEST(reducing_history_does_not_leak)
{
	flist_hist_setup(&lwin, "/dir1", "file1", 1, 1);
	flist_hist_setup(&lwin, "/dir2", "file2", 2, 1);
	flist_hist_setup(&lwin, "/dir3", "file3", 2, 1);
	flist_hist_setup(&lwin, "/dir4", "file4", 2, 1);
	flist_hist_setup(&lwin, "/dir5", "file5", 2, 1);

	/* The leak happened when new_size < old_size - new_size.  old_size == 5,
	 * old_size - new_size == 3. */
	cfg_resize_histories(2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
