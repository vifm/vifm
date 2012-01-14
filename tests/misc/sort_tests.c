#include <string.h>

#include "seatest.h"

#include "../../src/config.h"
#include "../../src/sort.h"
#include "../../src/ui.h"

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
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);
}

static void
test_special_chars_ignore_case_sort(void)
{
	int i;

	lwin.sort[0] = SORT_BY_INAME;
	for(i = 1; i < NUM_SORT_OPTIONS; i++)
		lwin.sort[i] = NUM_SORT_OPTIONS + 1;

	sort_view(&lwin);

	assert_string_equal("_", lwin.dir_entry[0].name);
}

#if defined(_WIN32) || defined(__APPLE__)
static void
test_versort_without_numbers(void)
{
#define SIGN(n) ({int _n = (n); _n < 0 ? -1 : ((_n > 0) ? 1 : 0);})

	const char *s, *t;

	s = "abc";
	t = "abcdef";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "abcdef";
	t = "abc";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "";
	t = "abc";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "abc";
	t = "";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "";
	t = "";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "abcdef";
	t = "abcdef";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "abcdef";
	t = "abcdeg";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

	s = "abcdeg";
	t = "abcdef";
	assert_int_equal(SIGN(strcmp(s, t)), SIGN(vercmp(s, t)));

#undef SIGN
}

static void
test_versort_with_numbers(void)
{
	const char *s, *t;

	s = "abcdef0";
	t = "abcdef0";
	assert_int_equal(strcmp(s, t), vercmp(s, t));

	s = "abcdef0";
	t = "abcdef1";
	assert_int_equal(strcmp(s, t), vercmp(s, t));

	s = "abcdef1";
	t = "abcdef0";
	assert_int_equal(strcmp(s, t), vercmp(s, t));

	s = "abcdef9";
	t = "abcdef10";
	assert_true(vercmp(s, t) < 0);

	s = "abcdef10";
	t = "abcdef10";
	assert_true(vercmp(s, t) == 0);

	s = "abcdef10";
	t = "abcdef9";
	assert_true(vercmp(s, t) > 0);

	s = "abcdef1.20.0";
	t = "abcdef1.5.1";
	assert_true(vercmp(s, t) > 0);

	s = "x001";
	t = "x1";
	assert_true(vercmp(s, t) < 0);

	s = "x1";
	t = "x001";
	assert_true(vercmp(s, t) > 0);
}

static void
test_versort_numbers_only(void)
{
	const char *s, *t;

	s = "00";
	t = "01";
	assert_true(vercmp(s, t) < 0);

	s = "01";
	t = "10";
	assert_true(vercmp(s, t) < 0);

	s = "10";
	t = "11";
	assert_true(vercmp(s, t) < 0);

	s = "01";
	t = "00";
	assert_true(vercmp(s, t) > 0);

	s = "10";
	t = "01";
	assert_true(vercmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(vercmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(vercmp(s, t) > 0);

	s = "13";
	t = "100";
	assert_true(vercmp(s, t) < 0);

	s = "100";
	t = "13";
	assert_true(vercmp(s, t) > 0);
}

static void
test_versort_numbers_only_and_letters_only(void)
{
	const char *s, *t;

	s = "A";
	t = "10";
	assert_true(vercmp(s, t) > 0);

	s = "10";
	t = "A";
	assert_true(vercmp(s, t) < 0);
}
#endif

void
sort_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_special_chars_ignore_case_sort);

#if defined(_WIN32) || defined(__APPLE__)
	run_test(test_versort_without_numbers);
	run_test(test_versort_with_numbers);
	run_test(test_versort_numbers_only);
	run_test(test_versort_numbers_only_and_letters_only);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
