#include <string.h> /* memset() */

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/sort.h"
#include "../../src/ui.h"

#define SIGN(n) ({__typeof(n) _n = (n); (_n < 0) ? -1 : (_n > 0);})
#define ASSERT_STRCMP_EQUAL(a, b) \
		do { assert_int_equal(SIGN(a), SIGN(b)); } while(0)

static void
setup(void)
{
	cfg.sort_numbers = 1;

	lwin.list_rows = 3;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[1].name = strdup("_");
	lwin.dir_entry[2].name = strdup("A");
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
	{
		free(lwin.dir_entry[i].name);
	}
	free(lwin.dir_entry);
}

static void
test_special_chars_ignore_case_sort(void)
{
	lwin.sort[0] = SORT_BY_INAME;
	memset(&lwin.sort[1], NO_SORT_OPTION, sizeof(lwin.sort) - 1);

	sort_view(&lwin);

	assert_string_equal("_", lwin.dir_entry[0].name);
}

static void
test_versort_without_numbers(void)
{
	const char *s, *t;

	s = "abc";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abc";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdeg";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdeg";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));
}

static void
test_versort_with_numbers(void)
{
	const char *s, *t;

	s = "abcdef0";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef0";
	t = "abcdef1";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef1";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef9";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) < 0);

	s = "abcdef10";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) == 0);

	s = "abcdef10";
	t = "abcdef9";
	assert_true(strnumcmp(s, t) > 0);

	s = "abcdef1.20.0";
	t = "abcdef1.5.1";
	assert_true(strnumcmp(s, t) > 0);

	s = "x001";
	t = "x1";
	assert_true(strnumcmp(s, t) < 0);

	s = "x1";
	t = "x001";
	assert_true(strnumcmp(s, t) > 0);
}

static void
test_versort_numbers_only(void)
{
	const char *s, *t;

	s = "00";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);

	s = "10";
	t = "11";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "00";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "01";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "13";
	t = "100";
	assert_true(strnumcmp(s, t) < 0);

	s = "100";
	t = "13";
	assert_true(strnumcmp(s, t) > 0);

	s = "09";
	t = "3";
	assert_true(strnumcmp(s, t) > 0);

	s = "3";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);
}

static void
test_versort_numbers_only_and_letters_only(void)
{
	const char *s, *t;

	s = "A";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "A";
	assert_true(strnumcmp(s, t) < 0);
}

static void
test_versort_zero_and_zerox(void)
{
	const char *s, *t;

	s = "0";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

static void
test_versort_zerox_and_one(void)
{
	const char *s, *t;

	s = "00_";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

void
sort_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_special_chars_ignore_case_sort);

	run_test(test_versort_without_numbers);
	run_test(test_versort_with_numbers);
	run_test(test_versort_numbers_only);
	run_test(test_versort_numbers_only_and_letters_only);
	run_test(test_versort_zero_and_zerox);
	run_test(test_versort_zerox_and_one);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
