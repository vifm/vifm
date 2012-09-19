#include <string.h>

#include "seatest.h"

#include "../../src/engine/parsing.h"

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_empty_fail(void)
{
	const char input[] = "";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_non_quoted_fail(void)
{
	const char input[] = "b";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_double_dot_fail(void)
{
	const char input[] = "'a'..'b'";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_starts_with_dot_fail(void)
{
	const char input[] = ".'b'";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_ends_with_dot_fail(void)
{
	const char input[] = "'a'.";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_fail_position_correct(void)
{
	const char input1[] = "'b' c";
	const char input2[] = "a b";

	assert_true(parse(input1) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
	assert_string_equal("'b' c", get_last_position());

	assert_true(parse(input2) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
	assert_string_equal("a b", get_last_position());
}

static void
test_spaces_and_fail_position_correct(void)
{
	const char input1[] = "  'b' c";
	const char input2[] = "  a b";

	assert_true(parse(input1) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
	assert_string_equal("'b' c", get_last_position());

	assert_true(parse(input2) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
	assert_string_equal("a b", get_last_position());
}

void
general_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_empty_fail);
	run_test(test_non_quoted_fail);
	run_test(test_double_dot_fail);
	run_test(test_starts_with_dot_fail);
	run_test(test_ends_with_dot_fail);
	run_test(test_fail_position_correct);
	run_test(test_spaces_and_fail_position_correct);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
