#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern int tabstop;

TEST(bang)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 0;
	set_option("fastrun", val, OPT_GLOBAL);

	assert_true(set_options("fastrun!", OPT_GLOBAL) == 0);
	assert_true(fastrun);

	assert_true(set_options("fastrun !", OPT_GLOBAL) == 0);
	assert_false(fastrun);

	assert_false(set_options("fastrun !f", OPT_GLOBAL) == 0);
}

TEST(qmark)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 8;
	set_option("fastrun", val, OPT_GLOBAL);

	assert_true(set_options("fastrun?", OPT_GLOBAL) == 0);
	assert_true(set_options("fastrun  ?", OPT_GLOBAL) == 0);
	assert_false(set_options("fastrun  !f", OPT_GLOBAL) == 0);
}

TEST(ampersand)
{
	optval_t val = { .bool_val = 1 };
	fastrun = 1;
	set_option("fastrun", val, OPT_GLOBAL);

	assert_true(set_options("fastrun &", OPT_GLOBAL) == 0);
	assert_false(fastrun);
}

TEST(assignment)
{
	optval_t val = { .int_val = 8 };
	tabstop = 4;
	set_option("tabstop", val, OPT_GLOBAL);
	assert_int_equal(4, tabstop);

	assert_true(set_options("tabstop =5", OPT_GLOBAL) == 0);
	assert_int_equal(5, tabstop);

	assert_true(set_options("tabstop   :8", OPT_GLOBAL) == 0);
	assert_int_equal(8, tabstop);

	assert_true(set_options("tabstop +=2", OPT_GLOBAL) == 0);
	assert_int_equal(10, tabstop);

	assert_true(set_options("tabstop -=1", OPT_GLOBAL) == 0);
	assert_int_equal(9, tabstop);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
