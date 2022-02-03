#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

SETUP()
{
	vle_keys_user_add(L"jo", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"jl", L"k", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"S", L"dd", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"Sj", L"k", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"dp", L"k", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"ZD", L"k", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"abc", L"", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"ZT", L"ykk", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEST(builtin_key_at_sequence_begin)
{
	assert_true(vle_keys_exec(L"j") == KEYS_WAIT_SHORT);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"j")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"jl")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"jo")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"jjo")));
}

TEST(increase_counter_right)
{
	size_t counter;

	counter = vle_keys_counter();
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"j"));
	assert_int_equal(counter, vle_keys_counter());

	counter = vle_keys_counter();
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"jj"));
	assert_int_equal(counter + 1, vle_keys_counter());

	counter = vle_keys_counter();
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"S"));
	assert_int_equal(counter, vle_keys_counter());

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"S")));
	assert_int_equal(counter + 1, vle_keys_counter());

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"abc")));
	assert_int_equal(counter + 3, vle_keys_counter());
}

TEST(counter_and_count)
{
	size_t counter;

	counter = vle_keys_counter();
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"2j"));
	assert_int_equal(counter, vle_keys_counter());

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"2k")));
	assert_int_equal(counter + 2, vle_keys_counter());

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"3" WK_C_w L"2" WK_LT)));
	assert_int_equal(counter + 4, vle_keys_counter());
}

TEST(counter_and_register)
{
	size_t counter;

	counter = vle_keys_counter();
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"\"aj"));
	assert_int_equal(counter, vle_keys_counter());

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"\"ak")));
	assert_int_equal(counter + 3, vle_keys_counter());
}

TEST(cancel_reg_with_esc_or_ctrl_c)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"\""));
	assert_int_equal(0, vle_keys_exec(L"\""L"\x03"));
	assert_int_equal(0, vle_keys_exec(L"\""L"\x1b"));
}

TEST(udm_and_builtin_selector)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
}

TEST(udm_and_builtin_wait_point)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"Z"));
}

TEST(counter_no_change_on_reenter)
{
	size_t counter;

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"norm")));
	assert_int_equal(counter + 4, vle_keys_counter());
}

TEST(counter_udm_and_selectors)
{
	size_t counter;

	counter = vle_keys_counter();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ZT")));
	assert_int_equal(counter + 2, vle_keys_counter());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
