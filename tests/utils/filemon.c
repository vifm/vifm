#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */

#include <test-utils.h>

#include "../../src/utils/filemon.h"

TEST(uninitialized_is_not_equal_even_to_itself)
{
	filemon_t mon = {};
	assert_false(filemon_equal(&mon, &mon));
	assert_false(filemon_is_set(&mon));
}

TEST(reset_filemon_is_not_equal_even_to_itself)
{
	filemon_t mon;
	filemon_reset(&mon);
	assert_false(filemon_equal(&mon, &mon));
	assert_false(filemon_is_set(&mon));
}

TEST(uninitialized_are_not_equal)
{
	filemon_t mon1 = {};
	filemon_t mon2 = {};
	assert_false(filemon_equal(&mon1, &mon2));
}

TEST(filemon_can_be_set)
{
	filemon_t mon;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_MODIFIED, &mon);
	assert_true(filemon_is_set(&mon));
}

TEST(equal_after_two_independent_reads)
{
	filemon_t mon1;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_MODIFIED, &mon1);

	filemon_t mon2;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_MODIFIED, &mon2);

	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(change_monitor_is_equal_after_two_independent_reads)
{
	filemon_t mon1;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_CHANGED, &mon1);

	filemon_t mon2;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_CHANGED, &mon2);

	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(equal_after_assignment)
{
	filemon_t mon1;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", FMT_MODIFIED, &mon1);

	filemon_t mon2 = mon1;
	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(modification_is_detected)
{
	FILE *f = fopen(SANDBOX_PATH "/file", "w");
	fclose(f);

	filemon_t mon1;
	filemon_from_file(SANDBOX_PATH "/file", FMT_MODIFIED, &mon1);

	reset_timestamp(SANDBOX_PATH "/file");

	filemon_t mon2;
	filemon_from_file(SANDBOX_PATH "/file", FMT_MODIFIED, &mon2);

	assert_false(filemon_equal(&mon1, &mon2));
}

TEST(failed_read_from_file_resets_filemon)
{
	filemon_t mon = {};
	assert_success(filemon_from_file(TEST_DATA_PATH "/existing-files/a",
				FMT_MODIFIED, &mon));
	assert_true(filemon_equal(&mon, &mon));

	assert_failure(filemon_from_file("there/is/no/such/file", FMT_MODIFIED,
				&mon));
	assert_false(filemon_equal(&mon, &mon));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
