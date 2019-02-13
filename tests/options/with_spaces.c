#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern int tabstop;

TEST(bang)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 0;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);

	assert_true(vle_opts_set("fastrun!", OPT_GLOBAL) == 0);
	assert_true(fastrun);

	assert_true(vle_opts_set("fastrun !", OPT_GLOBAL) == 0);
	assert_false(fastrun);

	assert_false(vle_opts_set("fastrun !f", OPT_GLOBAL) == 0);
}

TEST(qmark)
{
	optval_t val = { .bool_val = 0 };
	fastrun = 8;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);

	assert_true(vle_opts_set("fastrun?", OPT_GLOBAL) == 0);
	assert_true(vle_opts_set("fastrun  ?", OPT_GLOBAL) == 0);
	assert_false(vle_opts_set("fastrun  !f", OPT_GLOBAL) == 0);
}

TEST(ampersand)
{
	optval_t val = { .bool_val = 1 };
	fastrun = 1;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);

	assert_true(vle_opts_set("fastrun &", OPT_GLOBAL) == 0);
	assert_false(fastrun);
}

TEST(assignment)
{
	optval_t val = { .int_val = 8 };
	tabstop = 4;
	vle_opts_assign("tabstop", val, OPT_GLOBAL);
	assert_int_equal(4, tabstop);

	assert_true(vle_opts_set("tabstop =5", OPT_GLOBAL) == 0);
	assert_int_equal(5, tabstop);

	assert_true(vle_opts_set("tabstop   :8", OPT_GLOBAL) == 0);
	assert_int_equal(8, tabstop);

	assert_true(vle_opts_set("tabstop +=2", OPT_GLOBAL) == 0);
	assert_int_equal(10, tabstop);

	assert_true(vle_opts_set("tabstop -=1", OPT_GLOBAL) == 0);
	assert_int_equal(9, tabstop);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
