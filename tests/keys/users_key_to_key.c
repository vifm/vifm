#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

#include "builtin_keys.h"

static void key_selfremove(key_info_t key_info, keys_info_t *keys_info);

SETUP()
{
	vle_keys_user_add(L"hi", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"hi2", L"hi", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"ho", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"ha2", L"ho", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEST(user_key_sequence_cannot_be_empty_even)
{
	assert_false(vle_keys_user_exists(L"", NORMAL_MODE));
	vle_keys_reset();
	assert_false(vle_keys_user_exists(L"", NORMAL_MODE));
}

TEST(user_key_chain)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ha2")));
}

TEST(user_key_chain_wait)
{
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"hi2"));
}

TEST(count_is_passed_to_the_next_command)
{
	last_command_register = 0;
	vle_keys_user_add(L"J", L"dd", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"\"aJ")));
	assert_int_equal(last_command_register, 'a');
}

TEST(user_keys_are_cleared_on_request)
{
	last = 0;

	vle_keys_user_add(L"k", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"k")));
	assert_int_equal(2, last);

	vle_keys_user_clear();
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"k")));
	assert_int_equal(1, last);
}

TEST(user_key_presence_can_be_checked)
{
	assert_false(vle_keys_user_exists(L"k", NORMAL_MODE));
	vle_keys_user_add(L"k", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_true(vle_keys_user_exists(L"k", NORMAL_MODE));
}

TEST(removing_user_mapping_from_a_mapping_is_fine)
{
	keys_add_info_t keys = {WK_x, {{&key_selfremove}}};
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	vle_keys_user_add(L"a", L"x", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"a")));
}

static void
key_selfremove(key_info_t key_info, keys_info_t *keys_info)
{
	vle_keys_user_clear();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
