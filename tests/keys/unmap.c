#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
add_custom_keys(void)
{
	assert_int_equal(0, add_user_keys(L",h", L"k", NORMAL_MODE));
	assert_int_equal(0, add_user_keys(L",s", L"j", NORMAL_MODE));
}

static void
test_unmap(void)
{
	assert_true(IS_KEYS_RET_CODE(execute_keys(L",")));

	assert_false(IS_KEYS_RET_CODE(execute_keys(L",h")));
	assert_int_equal(0, remove_user_keys(L",h", NORMAL_MODE));
	assert_true(IS_KEYS_RET_CODE(execute_keys(L",h")));

	assert_false(IS_KEYS_RET_CODE(execute_keys(L",s")));
	assert_int_equal(0, remove_user_keys(L",s", NORMAL_MODE));
	assert_true(IS_KEYS_RET_CODE(execute_keys(L",s")));
}

void
unmap_tests(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(test_unmap);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
