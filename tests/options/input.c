#include "seatest.h"

#include "../../src/engine/options.h"

extern int fastrun;
extern const char *value;

static void
test_colon(void)
{
	optval_t val = { .str_val = "/home/tmp" };
	fastrun = 0;
	set_option("fusehome", val);

	assert_true(set_options("fusehome=/tmp") == 0);
	assert_string_equal("/tmp", value);

	assert_true(set_options("fusehome:/var/fuse") == 0);
	assert_string_equal("/var/fuse", value);
}

void
input_tests(void)
{
	test_fixture_start();

	run_test(test_colon);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
