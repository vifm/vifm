#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
test_multichar_selector(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"di"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dif")));
}

void
long_motions_wait(void)
{
	test_fixture_start();

	run_test(test_multichar_selector);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
