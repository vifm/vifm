#include "utils.h"

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/bmarks.h"

static void bmarks_cb(const char path[], const char tags[], time_t timestamp,
		void *arg);

static int cb_called;

TEST(no_call_for_empty_list)
{
	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	assert_int_equal(0, cb_called);
}

TEST(one_call_for_one_item)
{
	assert_success(bmarks_set("fake/path", "tag"));

	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	assert_int_equal(1, cb_called);
}

TEST(two_calls_for_two_items)
{
	assert_success(bmarks_set("fake/path1", "tag1"));
	assert_success(bmarks_set("fake/path2", "tag2"));

	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	assert_int_equal(2, cb_called);
}

TEST(path_is_canonicalized)
{
	assert_success(bmarks_set("parent/../dir//another-dir", "tag1"));
	assert_success(bmarks_set("dir/another-dir", "tag2"));

	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	assert_int_equal(1, cb_called);
}

static void
bmarks_cb(const char path[], const char tags[], time_t timestamp, void *arg)
{
	++cb_called;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
