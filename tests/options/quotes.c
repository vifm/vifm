#include "seatest.h"

#include "../../src/engine/options.h"

extern const char *value;

static void
test_no_quotes(void)
{
	value = NULL;
	set_options("fusehome=a\\ b");
	assert_string_equal("a b", value);

	value = NULL;
	set_options("fh=a\\ b\\ c");
	assert_string_equal("a b c", value);

	set_options("so=name");
}

static void
test_single_quotes(void)
{
	set_options("fusehome='a b'");
	assert_string_equal("a b", value);
}

static void
test_double_quotes(void)
{
	set_options("fusehome=\"a b\"");
	assert_string_equal("a b", value);

	set_options("fusehome=\"a \\\" b\"");
	assert_string_equal("a \" b", value);
}

void
test_quotes(void)
{
	test_fixture_start();

	run_test(test_no_quotes);
	run_test(test_single_quotes);
	run_test(test_double_quotes);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
