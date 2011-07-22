#include "seatest.h"

#include "../../src/utils.h"

static void
test_problem_1024(void)
{
	char buf[16];
	friendly_size_notation(1024*1024 - 2, sizeof(buf), buf);
	assert_string_equal("1.0 M", buf);
}

void
friendly_size(void)
{
	test_fixture_start();

	run_test(test_problem_1024);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
