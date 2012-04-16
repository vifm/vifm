#include <string.h>

#include "seatest.h"

#include "../../src/engine/parsing.h"

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_empty_ok(void)
{
	const char input[] = "\"\"";
	assert_string_equal("", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_simple_ok(void)
{
	const char input[] = "\"test\"";
	assert_string_equal("test", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_not_closed_error(void)
{
	const char input[] = "\"test";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_MISSING_QUOTE, get_parsing_error());
}

static void
test_concatenation(void)
{
	const char input_1[] = "\"NV\".\"AR\"";
	const char input_2[] = "\"NV\" .\"AR\"";
	const char input_3[] = "\"NV\". \"AR\"";
	const char input_4[] = "\"NV\" . \"AR\"";

	assert_string_equal("NVAR", parse(input_1));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_2));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_3));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_4));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_double_quote_escaping_ok(void)
{
	const char input[] = "\"\\\"\"";
	assert_string_equal("\"", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_special_chars_ok(void)
{
	const char input[] = "\"\\t\"";
	assert_string_equal("\t", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_spaces_ok(void)
{
	const char input[] = "\" s y \"";
	assert_string_equal(" s y ", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_dot_ok(void)
{
	const char input[] = "\"a . c\"";
	assert_string_equal("a . c", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

void
double_quoted_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_empty_ok);
	run_test(test_simple_ok);
	run_test(test_not_closed_error);
	run_test(test_concatenation);
	run_test(test_double_quote_escaping_ok);
	run_test(test_special_chars_ok);
	run_test(test_spaces_ok);
	run_test(test_dot_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
