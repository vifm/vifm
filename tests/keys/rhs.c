#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int last;

static void
add_custom_keys(void)
{
	add_user_keys(L"abc", L"", NORMAL_MODE, 0);
}

static void
test_nop_no_follow_ok(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"abc")));
}

static void
test_nop_followed_ok(void)
{
	last = 0;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"abcj")));
	assert_int_equal(2, last);
}

void
rhs_tests(void)
{
	test_fixture_start();

	fixture_setup(&add_custom_keys);

	run_test(test_nop_no_follow_ok);
	run_test(test_nop_followed_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
