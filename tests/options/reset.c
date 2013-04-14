#include "seatest.h"

#include "../../src/engine/options.h"

extern int fusehome_handler_calls;
extern const char *value;

static void
test_reset_of_str_option_not_differ_no_callback(void)
{
	int res;

	fusehome_handler_calls = 0;
	res = set_options("fusehome&");
	assert_int_equal(0, res);
	assert_int_equal(0, fusehome_handler_calls);
}

static void
test_reset_of_str_option_differ_callback(void)
{
	int res;

	res = set_options("fusehome='abc'");
	assert_int_equal(0, res);
	assert_string_equal("abc", value);

	fusehome_handler_calls = 0;
	res = set_options("fusehome&");
	assert_int_equal(0, res);
	assert_int_equal(1, fusehome_handler_calls);
	assert_string_equal("fusehome-default", value);
}

void
reset_tests(void)
{
	test_fixture_start();

	run_test(test_reset_of_str_option_not_differ_no_callback);
	run_test(test_reset_of_str_option_differ_callback);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
