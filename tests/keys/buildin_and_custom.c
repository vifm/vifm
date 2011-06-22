#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"jo", L"k", NORMAL_MODE);
	add_user_keys(L"jj", L"k", NORMAL_MODE);
}

static void
builtin_key_at_sequence_begin(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"j")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jj")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jo")));
}

void
buildin_and_custom(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(builtin_key_at_sequence_begin);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
