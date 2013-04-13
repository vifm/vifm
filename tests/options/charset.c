#include "seatest.h"

#include "../../src/engine/options.h"

extern char cpoptions[10];
extern int cpoptions_handler_calls;

static void
test_assignment_to_something_calls_handler_only_once(void)
{
	int res;

	cpoptions[0] = '\0';

	cpoptions_handler_calls = 0;
	res = set_options("cpoptions=abc");
	assert_int_equal(1, cpoptions_handler_calls);
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);
}

static void
test_assignment_to_something(void)
{
	int res;

	cpoptions[0] = '\0';

	res = set_options("cpoptions=ac");
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);
}

static void
test_assignment_to_same_handler_not_called(void)
{
	int res;

	cpoptions[0] = '\0';

	res = set_options("cpoptions=ac");
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	cpoptions_handler_calls = 0;
	res = set_options("cpoptions=ac");
	assert_int_equal(0, cpoptions_handler_calls);
	assert_int_equal(0, res);
}

static void
test_assignment_to_empty(void)
{
	int res;

	res = set_options("cpoptions=ac");
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = set_options("cpoptions=");
	assert_int_equal(0, res);
	assert_string_equal("", cpoptions);
}

static void
test_char_addition(void)
{
	int res;

	res = set_options("cpoptions=ac");
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = set_options("cpoptions+=b");
	assert_int_equal(0, res);
	assert_string_equal("acb", cpoptions);
}

static void
test_char_removal(void)
{
	int res;

	res = set_options("cpoptions=ac");
	assert_int_equal(0, res);
	assert_string_equal("ac", cpoptions);

	res = set_options("cpoptions-=a");
	assert_int_equal(0, res);
	assert_string_equal("c", cpoptions);
}

static void
test_multiple_char_addition(void)
{
	int res;

	res = set_options("cpoptions=b");
	assert_int_equal(0, res);
	assert_string_equal("b", cpoptions);

	res = set_options("cpoptions+=ac");
	assert_int_equal(0, res);
	assert_string_equal("bac", cpoptions);
}

static void
test_multiple_char_removal(void)
{
	int res;

	res = set_options("cpoptions=abc");
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = set_options("cpoptions-=ac");
	assert_int_equal(0, res);
	assert_string_equal("b", cpoptions);
}

static void
test_chars_not_from_the_list_are_rejected_on_assignment(void)
{
	const int res = set_options("cpoptions=yxz");
	assert_false(res == 0);
}

static void
test_chars_not_from_the_list_are_rejected_on_addition(void)
{
	int res;

	res = set_options("cpoptions=abc");
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = set_options("cpoptions+=az");
	assert_false(res == 0);
}

static void
test_chars_not_from_the_list_are_ignored_on_removal(void)
{
	int res;

	res = set_options("cpoptions=abc");
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);

	res = set_options("cpoptions-=xyz");
	assert_int_equal(0, res);
	assert_string_equal("abc", cpoptions);
}

void
charset_tests(void)
{
	test_fixture_start();

	run_test(test_assignment_to_something_calls_handler_only_once);
	run_test(test_assignment_to_something);
	run_test(test_assignment_to_same_handler_not_called);
	run_test(test_assignment_to_empty);
	run_test(test_char_addition);
	run_test(test_char_removal);
	run_test(test_multiple_char_addition);
	run_test(test_multiple_char_removal);
	run_test(test_chars_not_from_the_list_are_rejected_on_assignment);
	run_test(test_chars_not_from_the_list_are_rejected_on_addition);
	run_test(test_chars_not_from_the_list_are_ignored_on_removal);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
