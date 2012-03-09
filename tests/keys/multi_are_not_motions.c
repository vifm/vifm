#include "seatest.h"

#include "../../src/engine/keys.h"

static void
dont_treat_multikeys_as_motions(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"dm"));
}

void
multi_are_not_motions(void)
{
	test_fixture_start();

	run_test(dont_treat_multikeys_as_motions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
