#include <stic.h>

#include <sys/time.h> /* timeval utimes() */

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/utils/filemon.h"

#include "utils.h"

TEST(equal_after_two_independent_reads)
{
	filemon_t mon1;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", &mon1);

	filemon_t mon2;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", &mon2);

	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(equal_after_assignment)
{
	filemon_t mon1;
	filemon_from_file(TEST_DATA_PATH "/existing-files/a", &mon1);

	filemon_t mon2;
	filemon_assign(&mon2, &mon1);

	assert_true(filemon_equal(&mon1, &mon2));
}

TEST(modification_is_detected, IF(not_windows))
{
	FILE *f = fopen(SANDBOX_PATH "/file", "w");
	fclose(f);

	filemon_t mon1;
	filemon_from_file(SANDBOX_PATH "/file", &mon1);

#ifndef _WIN32
	struct timeval tvs[2] = {};
	assert_success(utimes(SANDBOX_PATH "/file", tvs));
#endif

	filemon_t mon2;
	filemon_from_file(SANDBOX_PATH "/file", &mon2);

	assert_false(filemon_equal(&mon1, &mon2));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
