#include <stdlib.h>

#include "seatest.h"

#include "../../src/completion.h"

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

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
