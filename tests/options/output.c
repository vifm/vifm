#include "seatest.h"

#include "../../src/engine/options.h"

extern int fastrun;
extern const char *value;

static void
test_get_option_value_empty_for_bool(void)
{
	assert_true(set_options("fastrun") == 0);
	assert_string_equal("", get_option_value("fastrun"));

	assert_true(set_options("nofastrun") == 0);
	assert_string_equal("", get_option_value("fastrun"));
}

static void
test_get_option_value_works_for_int_no_leading_zeroes(void)
{
	assert_true(set_options("tabstop=8") == 0);
	assert_string_equal("8", get_option_value("tabstop"));

	assert_true(set_options("tabstop=08") == 0);
	assert_string_equal("8", get_option_value("tabstop"));
}

static void
test_get_option_value_works_for_str(void)
{
	assert_true(set_options("fusehome=/") == 0);
	assert_string_equal("/", get_option_value("fusehome"));

	assert_true(set_options("fusehome=") == 0);
	assert_string_equal("", get_option_value("fusehome"));
}

static void
test_get_option_value_works_for_strlist(void)
{
	assert_true(set_options("cdpath=a,b,c") == 0);
	assert_string_equal("a,b,c", get_option_value("cdpath"));

	assert_true(set_options("cdpath-=b") == 0);
	assert_string_equal("a,c", get_option_value("cdpath"));
}

static void
test_get_option_value_works_for_enum(void)
{
	assert_true(set_options("sort=name") == 0);
	assert_string_equal("name", get_option_value("sort"));
}

static void
test_get_option_value_works_for_set(void)
{
	assert_true(set_options("vifminfo=tui") == 0);
	assert_string_equal("tui", get_option_value("vifminfo"));
}

static void
test_get_option_value_works_for_charset(void)
{
	assert_true(set_options("cpoptions=a") == 0);
	assert_string_equal("a", get_option_value("cpoptions"));
}

void
output_tests(void)
{
	test_fixture_start();

	run_test(test_get_option_value_empty_for_bool);
	run_test(test_get_option_value_works_for_int_no_leading_zeroes);
	run_test(test_get_option_value_works_for_str);
	run_test(test_get_option_value_works_for_strlist);
	run_test(test_get_option_value_works_for_enum);
	run_test(test_get_option_value_works_for_set);
	run_test(test_get_option_value_works_for_charset);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
