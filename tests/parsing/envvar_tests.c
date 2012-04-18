#include <string.h>

#include "seatest.h"

#include "../../src/engine/parsing.h"

static const char *
ge(const char *name)
{
	return name + 1;
}

static void
setup(void)
{
	init_parser(ge);
}

static void
test_simple_ok(void)
{
	const char input[] = "$ENV";
	assert_string_equal("NV", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_leading_spaces_ok(void)
{
	const char input[] = " $ENV";
	assert_string_equal("NV", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_trailing_spaces_ok(void)
{
	const char input[] = "$ENV ";
	assert_string_equal("NV", parse(input));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_space_in_the_middle_fail(void)
{
	const char input[] = "$ENV $VAR";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_concatenation(void)
{
	const char input_1[] = "$ENV.$VAR";
	const char input_2[] = "$ENV .$VAR";
	const char input_3[] = "$ENV. $VAR";
	const char input_4[] = "$ENV . $VAR";

	assert_string_equal("NVAR", parse(input_1));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_2));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_3));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());

	assert_string_equal("NVAR", parse(input_4));
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

void
envvar_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_simple_ok);
	run_test(test_leading_spaces_ok);
	run_test(test_trailing_spaces_ok);
	run_test(test_space_in_the_middle_fail);
	run_test(test_concatenation);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
