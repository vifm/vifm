#include "seatest.h"

#include "../../src/options.h"

extern int fastrun;

static void
test_bang(void)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 0;
	set_option("fastrun", val);

	assert_true(set_options("fastrun!") == 0);
	assert_true(fastrun);

	assert_true(set_options("fastrun !") == 0);
	assert_false(fastrun);

	assert_false(set_options("fastrun !f") == 0);
}

static void
test_ampersand(void)
{
	optval_t val = { .bool_val = 1 };
	fastrun = 1;
	set_option("fastrun", val);

	assert_true(set_options("fastrun &") == 0);
	assert_false(fastrun);
}

void
with_spaces_tests(void)
{
	test_fixture_start();

	run_test(test_bang);
	run_test(test_ampersand);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
