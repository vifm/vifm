#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern int tabstop;

TEST(bang)
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

TEST(qmark)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 8;
	set_option("fastrun", val);

	assert_true(set_options("fastrun?") == 0);
	assert_true(set_options("fastrun  ?") == 0);
	assert_false(set_options("fastrun  !f") == 0);
}

TEST(ampersand)
{
	optval_t val = { .bool_val = 1 };
	fastrun = 1;
	set_option("fastrun", val);

	assert_true(set_options("fastrun &") == 0);
	assert_false(fastrun);
}

TEST(assignment)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
