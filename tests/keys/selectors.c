#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

#include "builtin_keys.h"

/* This should be a macro to see what test have failed. */
#define check(wait, full, sel_count, cmd_count) \
	{ \
		assert_int_equal(KEYS_WAIT, vle_keys_exec(wait)); \
		assert_int_equal(0, vle_keys_exec(full)); \
		assert_int_equal((sel_count), last_selector_count); \
		assert_int_equal((cmd_count), last_command_count); \
	}

static void key_tail(key_info_t key_info, keys_info_t *keys_info);
static void key_cmd(key_info_t key_info, keys_info_t *keys_info);
static void selector_a(key_info_t key_info, keys_info_t *keys_info);

static int counter;
static int tail_mark, cmd_mark, selector_mark;

SETUP()
{
	keys_add_info_t keys[] = {
		{WK_x, {{&key_tail}}},
		{WK_z, {{&key_cmd}, FOLLOWED_BY_SELECTOR}},
	};
	keys_add_info_t selectors[] = {
		{WK_a,      {{&selector_a}}},
		{WK_y WK_a, {{&selector_a}}},
		{WK_x,      {{&selector_a}}},
		{WK_x WK_a, {{&selector_a}}},
	};

	vle_keys_add(keys, 2U, NORMAL_MODE);
	vle_keys_add_selectors(selectors, 4U, NORMAL_MODE);

	counter = 0;
}

TEST(no_number_ok)
{
	check(L"d", L"dk", NO_COUNT_GIVEN, NO_COUNT_GIVEN);
}

TEST(with_number_ok)
{
	check(L"d1",   L"d1k",   1*1,   NO_COUNT_GIVEN);
	check(L"d12",  L"d12k",  1*12,  NO_COUNT_GIVEN);
	check(L"d123", L"d123k", 1*123, NO_COUNT_GIVEN);
}

TEST(with_zero_number_fail)
{
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d0"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d0k"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d01"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d01k"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d012"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"d012k"));
}

TEST(with_number_before_and_in_the_middle_ok)
{
	check(L"2d1",   L"2d1k",   2*1,   NO_COUNT_GIVEN);
	check(L"3d12",  L"3d12k",  3*12,  NO_COUNT_GIVEN);
	check(L"2d123", L"2d123k", 2*123, NO_COUNT_GIVEN);
}

TEST(long_waiting_for_multichar_selector)
{
	const size_t counter = vle_keys_counter();

	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"zy"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"zyax")));
	assert_int_equal(1, selector_mark);
	assert_int_equal(2, cmd_mark);
	assert_int_equal(3, tail_mark);

	assert_int_equal(counter + 4U, vle_keys_counter());
}

TEST(short_waiting_for_multichar_selector)
{
	const size_t counter = vle_keys_counter();

	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"zx"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"zxax")));
	assert_int_equal(1, selector_mark);
	assert_int_equal(2, cmd_mark);
	assert_int_equal(3, tail_mark);

	assert_int_equal(counter + 4U, vle_keys_counter());
}

TEST(call_order_is_correct_for_singlechar_tail)
{
	const size_t counter = vle_keys_counter();

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"zax")));
	assert_int_equal(1, selector_mark);
	assert_int_equal(2, cmd_mark);
	assert_int_equal(3, tail_mark);

	assert_int_equal(counter + 3U, vle_keys_counter());
}

TEST(call_order_is_correct_for_multichar_tail)
{
	const size_t counter = vle_keys_counter();

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"zaxx")));
	assert_int_equal(1, selector_mark);
	assert_int_equal(2, cmd_mark);
	assert_int_equal(4, tail_mark);

	assert_int_equal(counter + 4U, vle_keys_counter());
}

static void
key_tail(key_info_t key_info, keys_info_t *keys_info)
{
	tail_mark = ++counter;
}

static void
key_cmd(key_info_t key_info, keys_info_t *keys_info)
{
	cmd_mark = ++counter;
}

static void
selector_a(key_info_t key_info, keys_info_t *keys_info)
{
	selector_mark = ++counter;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
