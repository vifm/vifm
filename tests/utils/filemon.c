#include <stic.h>

#include <sys/time.h> /* timeval utimes() */

#include <stdio.h> /* FILE fclose() fopen() */

#include <test-utils.h>

#include "../../src/utils/filemon.h"

TEST(uninitialized_is_not_equal_even_to_itself)
{
	filemon_t mon = {};
	assert_false(filemon_equal(&mon, &mon));
}

TEST(uninitialized_are_not_equal)
{
	filemon_t mon1 = {};
	filemon_t mon2 = {};
	assert_false(filemon_equal(&mon1, &mon2));
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

	filemon_t mon2;
	filemon_assign(&mon2, &mon1);

	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(modification_is_detected, IF(not_windows))
{
	FILE *f = fopen(SANDBOX_PATH "/file", "w");
	fclose(f);

	filemon_t mon1;
	filemon_from_file(SANDBOX_PATH "/file", FMT_MODIFIED, &mon1);

#ifndef _WIN32
	struct timeval tvs[2] = {};
	assert_success(utimes(SANDBOX_PATH "/file", tvs));
#endif

	filemon_t mon2;
	filemon_from_file(SANDBOX_PATH "/file", FMT_MODIFIED, &mon2);

	assert_false(filemon_equal(&mon1, &mon2));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
