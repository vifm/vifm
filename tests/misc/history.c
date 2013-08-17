#include "seatest.h"

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/commands.h"
#include "../../src/filelist.h"

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

static void
setup(void)
{
	/* lwin */
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");

	/* rwin */
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 1;
	rwin.list_pos = 0;
	rwin.dir_entry = calloc(rwin.list_rows, sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");

	hist_clear(&cfg.cmd_hist);
	hist_clear(&cfg.prompt_hist);
	hist_clear(&cfg.search_hist);

	resize_history(INITIAL_SIZE);
}

static void
teardown(void)
{
	int i;

	resize_history(0);

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	free(rwin.dir_entry);
}

static void
save_to_history(const char str[])
{
	save_command_history(str);
	save_search_history(str);
	save_prompt_history(str);

	save_view_history(&lwin, str, str + 1, 0);
	save_view_history(&rwin, str, str + 1, 0);
}

static void
test_view_history_after_reset_contains_valid_data(void)
{
	assert_string_equal("/lwin", lwin.history[0].dir);
	assert_string_equal("lfile0", lwin.history[0].file);

	assert_string_equal("/rwin", rwin.history[0].dir);
	assert_string_equal("rfile0", rwin.history[0].file);
}

static void
test_view_history_avoids_duplicates(void)
{
	assert_int_equal(1, lwin.history_num);
	assert_int_equal(1, rwin.history_num);

	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);

	assert_int_equal(1, lwin.history_num);
	assert_int_equal(1, rwin.history_num);
}

static void
test_after_history_reset_ok(void)
{
	const char *const str = "string";
	save_to_history(str);
	VALIDATE_HISTORY(0, str);
}

static void
test_add_after_decreasing_ok(void)
{
	const char *const str = "longstringofmeaninglesstext";
	int i;

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}

	resize_history(INITIAL_SIZE/2);

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}
}

static void
test_add_after_increasing_ok(void)
{
	const char *const str = "longstringofmeaninglesstext";
	int i;

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}

	resize_history(INITIAL_SIZE*2);

	for(i = 0; i < INITIAL_SIZE; i++)
	{
		save_to_history(str + i);
	}
}

void
history_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_view_history_after_reset_contains_valid_data);
	run_test(test_view_history_avoids_duplicates);
	run_test(test_after_history_reset_ok);
	run_test(test_add_after_decreasing_ok);
	run_test(test_add_after_increasing_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
