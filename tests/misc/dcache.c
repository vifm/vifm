#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* memset() strcpy() */
#include <time.h> /* time() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

SETUP()
{
	update_string(&cfg.shell, "");
	assert_success(stats_init(&cfg));
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
}

TEST(size_does_not_clobber_nitems)
{
	uint64_t size;
	uint64_t nitems;

	dcache_set_at(TEST_DATA_PATH, 0, 10, 11);
	dcache_set_at(TEST_DATA_PATH, 0, 13, DCACHE_UNKNOWN);

	/* Tests are executed fast, so decrease mtime. */
	dcache_get_at(TEST_DATA_PATH, time(NULL) - 10, 0, &size, &nitems);
	assert_ulong_equal(13, size);
	assert_ulong_equal(11, nitems);
}

TEST(nitems_does_not_clobber_size)
{
	uint64_t size;
	uint64_t nitems;

	dcache_set_at(TEST_DATA_PATH, 0, 10, 11);
	dcache_set_at(TEST_DATA_PATH, 0, DCACHE_UNKNOWN, 12);

	/* Tests are executed fast, so decrease mtime. */
	dcache_get_at(TEST_DATA_PATH, time(NULL) - 10, 0, &size, &nitems);
	assert_ulong_equal(10, size);
	assert_ulong_equal(12, nitems);
}

TEST(outdated_data_is_detected)
{
	dcache_result_t size, nitems;

	dir_entry_t entry = { .name = "read", .origin = TEST_DATA_PATH };

	dcache_set_at(TEST_DATA_PATH "/read", 0, 10, 11);

	/* Entry was updated *while* it was being cached. */
	entry.mtime = time(NULL);
	dcache_get_of(&entry, &size, &nitems);
	assert_false(size.is_valid);
	assert_false(nitems.is_valid);

	/* Entry was updated *after* it was cached. */
	entry.mtime = time(NULL) + 1;
	dcache_get_of(&entry, &size, &nitems);
	assert_false(size.is_valid);
	assert_false(nitems.is_valid);
}

TEST(can_query_one_parameter_at_a_time)
{
	dcache_result_t data;
	dir_entry_t entry = { .name = "read", .origin = TEST_DATA_PATH };

	dcache_set_at(TEST_DATA_PATH "/read", 0, 10, 11);

	dcache_get_of(&entry, &data, NULL);
	assert_true(data.is_valid);
	assert_ulong_equal(10, data.value);

	dcache_get_of(&entry, NULL, &data);
	assert_true(data.is_valid);
	assert_ulong_equal(11, data.value);
}

#ifndef _WIN32

TEST(inode_is_taken_into_account)
{
	dcache_result_t size, nitems;

	dir_entry_t entry = { .name = "read", .origin = TEST_DATA_PATH, .inode = 1 };

	dcache_set_at(TEST_DATA_PATH "/read", 1, 10, 11);

	dcache_get_of(&entry, &size, &nitems);
	assert_true(size.is_valid);
	assert_true(nitems.is_valid);

	entry.inode = 2;
	dcache_get_of(&entry, &size, &nitems);
	assert_false(size.is_valid);
	assert_false(nitems.is_valid);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
