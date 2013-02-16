#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/utils/utils.h"

static void
test_removing_useless_trailing_zero(void)
{
	char buf[16];

	friendly_size_notation(4*1024, sizeof(buf), buf);
	assert_string_equal("4 K", buf);

	friendly_size_notation(1024*1024 - 1, sizeof(buf), buf);
	assert_string_equal("1 M", buf);
}

static void
test_problem_1024(void)
{
	char buf[16];

	friendly_size_notation(1024*1024 - 2, sizeof(buf), buf);
	assert_string_equal("1 M", buf);
}

void
friendly_size(void)
{
	test_fixture_start();

	cfg.use_iec_prefixes = 0;

	run_test(test_removing_useless_trailing_zero);
	run_test(test_problem_1024);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
