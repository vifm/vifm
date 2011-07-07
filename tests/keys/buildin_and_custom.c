#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"jo", L"k", NORMAL_MODE);
	add_user_keys(L"jl", L"k", NORMAL_MODE);
}

static void
builtin_key_at_sequence_begin(void)
{
	assert_true(execute_keys(L"j") == KEYS_WAIT_SHORT);
	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"j")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jl")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jo")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jjo")));
}

static void
increase_counter_right(void)
{
	size_t counter;

	counter = get_key_counter();
	assert_true(execute_keys(L"j") == KEYS_WAIT_SHORT);
	assert_int_equal(counter, get_key_counter());

	counter = get_key_counter();
	assert_true(execute_keys(L"jj") == KEYS_WAIT_SHORT);
	assert_int_equal(counter + 1, get_key_counter());
}

void
buildin_and_custom(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(builtin_key_at_sequence_begin);
	run_test(increase_counter_right);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
