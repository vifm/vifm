#include <string.h>

#include "seatest.h"

#include "../../src/config.h"
#include "../../src/sort.h"

static void
setup(void)
{
	cfg.sort_numbers = 1;
}

static void
without_numbers(void)
{
	const char *s, *t;

	s = "abc";
	t = "abcdef";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef";
	t = "abc";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "";
	t = "abc";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abc";
	t = "";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "";
	t = "";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef";
	t = "abcdef";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef";
	t = "abcdeg";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdeg";
	t = "abcdef";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));
}

static void
with_numbers(void)
{
	const char *s, *t;

	s = "abcdef0";
	t = "abcdef0";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef0";
	t = "abcdef1";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef1";
	t = "abcdef0";
	assert_int_equal(strcmp(s, t), compare_file_names(s, t));

	s = "abcdef9";
	t = "abcdef10";
	assert_true(compare_file_names(s, t) < 0);

	s = "abcdef10";
	t = "abcdef10";
	assert_true(compare_file_names(s, t) == 0);

	s = "abcdef10";
	t = "abcdef9";
	assert_true(compare_file_names(s, t) > 0);

	s = "abcdef1.20.0";
	t = "abcdef1.5.1";
	assert_true(compare_file_names(s, t) > 0);
}

static void
numbers_only(void)
{
	const char *s, *t;

	s = "00";
	t = "01";
	assert_true(compare_file_names(s, t) < 0);

	s = "01";
	t = "10";
	assert_true(compare_file_names(s, t) < 0);

	s = "10";
	t = "11";
	assert_true(compare_file_names(s, t) < 0);

	s = "01";
	t = "00";
	assert_true(compare_file_names(s, t) > 0);

	s = "10";
	t = "01";
	assert_true(compare_file_names(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(compare_file_names(s, t) > 0);
}

static void
numbers_only_and_letters_only(void)
{
	const char *s, *t;

	s = "A";
	t = "10";
	assert_true(compare_file_names(s, t) > 0);

	s = "10";
	t = "A";
	assert_true(compare_file_names(s, t) < 0);
}

void
compare_file_names_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(without_numbers);
	run_test(with_numbers);
	run_test(numbers_only);
	run_test(numbers_only_and_letters_only);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
