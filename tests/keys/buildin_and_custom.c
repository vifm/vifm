#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"jo", L"k", NORMAL_MODE, 0);
	add_user_keys(L"jl", L"k", NORMAL_MODE, 0);

	add_user_keys(L"S", L"dd", NORMAL_MODE, 0);
	add_user_keys(L"Sj", L"k", NORMAL_MODE, 0);

	add_user_keys(L"dp", L"k", NORMAL_MODE, 0);

	add_user_keys(L"ZD", L"k", NORMAL_MODE, 0);
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
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"j"));
	assert_int_equal(counter, get_key_counter());

	counter = get_key_counter();
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"jj"));
	assert_int_equal(counter + 1, get_key_counter());

	counter = get_key_counter();
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"S"));
	assert_int_equal(counter, get_key_counter());

	counter = get_key_counter();
	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"S")));
	assert_int_equal(counter + 1, get_key_counter());
}

static void
test_cancel_reg_with_esc_or_ctrl_c(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"\""));
	assert_int_equal(0, execute_keys(L"\""L"\x03"));
	assert_int_equal(0, execute_keys(L"\""L"\x1b"));
}

static void
test_udm_and_builtin_selector(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
}

static void
test_udm_and_builtin_wait_point(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"Z"));
}

static void
test_counter_no_change_on_reenter(void)
{
	size_t counter;

	counter = get_key_counter();
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"norm")));
	assert_int_equal(counter + 4, get_key_counter());
}

void
builtin_and_custom(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(builtin_key_at_sequence_begin);
	run_test(increase_counter_right);
	run_test(test_cancel_reg_with_esc_or_ctrl_c);
	run_test(test_udm_and_builtin_selector);
	run_test(test_udm_and_builtin_wait_point);
	run_test(test_counter_no_change_on_reenter);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
