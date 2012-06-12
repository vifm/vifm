#include "seatest.h"

#include "../../src/engine/options.h"

extern int fastrun;
extern int tabstop;

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
test_qmark(void)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 8;
	set_option("fastrun", val);

	assert_true(set_options("fastrun?") == 0);
	assert_true(set_options("fastrun  ?") == 0);
	assert_false(set_options("fastrun  !f") == 0);
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

static void
test_assignment(void)
{
	optval_t val = { .int_val = 8 };
	tabstop = 4;
	set_option("tabstop", val);
	assert_int_equal(4, tabstop);

	assert_true(set_options("tabstop =5") == 0);
	assert_int_equal(5, tabstop);

	assert_true(set_options("tabstop   :8") == 0);
	assert_int_equal(8, tabstop);

	assert_true(set_options("tabstop +=2") == 0);
	assert_int_equal(10, tabstop);

	assert_true(set_options("tabstop -=1") == 0);
	assert_int_equal(9, tabstop);
}

void
with_spaces_tests(void)
{
	test_fixture_start();

	run_test(test_bang);
	run_test(test_qmark);
	run_test(test_ampersand);
	run_test(test_assignment);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
