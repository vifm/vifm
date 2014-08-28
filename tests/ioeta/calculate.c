#include "seatest.h"

#include "../../src/io/ior.h"

static void
test_(void)
{
}

void
calculate_tests(void)
{
	test_fixture_start();

	run_test(test_);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
