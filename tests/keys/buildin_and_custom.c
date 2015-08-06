#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	add_user_keys(L"jo", L"k", NORMAL_MODE, 0);
	add_user_keys(L"jl", L"k", NORMAL_MODE, 0);

	add_user_keys(L"S", L"dd", NORMAL_MODE, 0);
	add_user_keys(L"Sj", L"k", NORMAL_MODE, 0);

	add_user_keys(L"dp", L"k", NORMAL_MODE, 0);

	add_user_keys(L"ZD", L"k", NORMAL_MODE, 0);

	add_user_keys(L"abc", L"", NORMAL_MODE, 0);
}

TEST(builtin_key_at_sequence_begin)
{
	assert_true(execute_keys(L"j") == KEYS_WAIT_SHORT);
	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"j")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jl")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jo")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"jjo")));
}

TEST(increase_counter_right)
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

	counter = get_key_counter();
	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"abc")));
	assert_int_equal(counter + 3, get_key_counter());
}

TEST(cancel_reg_with_esc_or_ctrl_c)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"\""));
	assert_int_equal(0, execute_keys(L"\""L"\x03"));
	assert_int_equal(0, execute_keys(L"\""L"\x1b"));
}

TEST(udm_and_builtin_selector)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
}

TEST(udm_and_builtin_wait_point)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"Z"));
}

TEST(counter_no_change_on_reenter)
{
	size_t counter;

	counter = get_key_counter();
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"norm")));
	assert_int_equal(counter + 4, get_key_counter());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
