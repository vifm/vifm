#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/completion.h"

static void
test_general(void)
{
	char *buf;

	assert_int_equal(0, add_completion("abc"));
	assert_int_equal(0, add_completion("acd"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("acd", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("acd", buf);
	free(buf);
}

static void
test_sorting(void)
{
	char *buf;

	assert_int_equal(0, add_completion("acd"));
	assert_int_equal(0, add_completion("abc"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("acd", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);
}

static void
test_one_element_completion(void)
{
	char *buf;

	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
}

static void
test_one_unambiguous_completion(void)
{
	char *buf;

	assert_int_equal(0, add_completion("abcd"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("abcd", buf);
	free(buf);
}

static void
test_rewind_one_unambiguous_completion(void)
{
	char *buf;

	assert_int_equal(0, add_completion("abcd"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("abcd", buf);
	free(buf);

	rewind_completion();

	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("abcd", buf);
	free(buf);
}

static void
test_rewind(void)
{
	char *buf;

	assert_int_equal(0, add_completion("acd"));
	assert_int_equal(0, add_completion("abc"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);

	rewind_completion();
	free(next_completion());

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("acd", buf);
	free(buf);

	rewind_completion();
	free(next_completion());

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("acd", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("abc", buf);
	free(buf);
}

static void
test_order(void)
{
	char *buf;

	assert_int_equal(0, add_completion("aa"));
	assert_int_equal(0, add_completion("az"));
	completion_group_end();

	assert_int_equal(0, add_completion("a"));

	set_completion_order(1);

	buf = next_completion();
	assert_string_equal("az", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("aa", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("a", buf);
	free(buf);
}

static void
test_umbiguous_begin(void)
{
	char *buf;

	assert_int_equal(0, add_completion("sort"));
	assert_int_equal(0, add_completion("sortorder"));
	assert_int_equal(0, add_completion("sortnumbers"));
	completion_group_end();

	assert_int_equal(0, add_completion("sort"));

	buf = next_completion();
	assert_string_equal("sort", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("sortnumbers", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("sortorder", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("sort", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("sort", buf);
	free(buf);
}

static void
test_two_matches(void)
{
	char *buf;

	assert_int_equal(0, add_completion("mountpoint"));
	completion_group_end();
	assert_int_equal(0, add_completion("mount"));
	completion_group_end();

	assert_int_equal(0, add_completion("mount"));

	buf = next_completion();
	assert_string_equal("mountpoint", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("mount", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("mount", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("mountpoint", buf);
	free(buf);
}

static void
test_removes_duplicates(void)
{
	char *buf;

	assert_int_equal(0, add_completion("mou"));
	assert_int_equal(0, add_completion("mount"));
	assert_int_equal(0, add_completion("mount"));
	completion_group_end();

	assert_int_equal(0, add_completion("m"));

	buf = next_completion();
	assert_string_equal("mou", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("mount", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("m", buf);
	free(buf);

	buf = next_completion();
	assert_string_equal("mou", buf);
	free(buf);
}

void
completion_tests(void)
{
	test_fixture_start();

	run_test(test_general);
	run_test(test_sorting);
	run_test(test_one_element_completion);
	run_test(test_one_unambiguous_completion);
	run_test(test_rewind_one_unambiguous_completion);
	run_test(test_rewind);
	run_test(test_order);
	run_test(test_umbiguous_begin);
	run_test(test_two_matches);
	run_test(test_removes_duplicates);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
