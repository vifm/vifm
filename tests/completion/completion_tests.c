#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/completion.h"

static void
test_general(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abc"));
	assert_int_equal(0, vle_compl_add_match("acd"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
}

static void
test_sorting(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("acd"));
	assert_int_equal(0, vle_compl_add_match("abc"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
}

static void
test_one_element_completion(void)
{
	char *buf;

	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_match("a"));

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
}

static void
test_one_unambiguous_completion(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abcd"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
}

static void
test_rewind_one_unambiguous_completion(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("abcd"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);

	vle_compl_rewind();

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("abcd", buf);
	free(buf);
}

static void
test_rewind(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("acd"));
	assert_int_equal(0, vle_compl_add_match("abc"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	vle_compl_rewind();
	free(vle_compl_next());

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);

	vle_compl_rewind();
	free(vle_compl_next());

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("acd", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("abc", buf);
	free(buf);
}

static void
test_order(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("aa"));
	assert_int_equal(0, vle_compl_add_match("az"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("a"));

	vle_compl_set_order(1);

	buf = vle_compl_next();
	assert_string_equal("az", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("aa", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("a", buf);
	free(buf);
}

static void
test_umbiguous_begin(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("sort"));
	assert_int_equal(0, vle_compl_add_match("sortorder"));
	assert_int_equal(0, vle_compl_add_match("sortnumbers"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("sort"));

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sortnumbers", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sortorder", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("sort", buf);
	free(buf);
}

static void
test_two_matches(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("mountpoint"));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_match("mount"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("mount"));

	buf = vle_compl_next();
	assert_string_equal("mountpoint", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mountpoint", buf);
	free(buf);
}

static void
test_removes_duplicates(void)
{
	char *buf;

	assert_int_equal(0, vle_compl_add_match("mou"));
	assert_int_equal(0, vle_compl_add_match("mount"));
	assert_int_equal(0, vle_compl_add_match("mount"));
	vle_compl_finish_group();

	assert_int_equal(0, vle_compl_add_last_match("m"));

	buf = vle_compl_next();
	assert_string_equal("mou", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("mount", buf);
	free(buf);

	buf = vle_compl_next();
	assert_string_equal("m", buf);
	free(buf);

	buf = vle_compl_next();
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
