#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

static int handler(wchar_t key);

static int mapping_states;

SETUP()
{
	vle_keys_set_def_handler(CMDLINE_MODE, &handler);
	vle_keys_user_add(L"s", L":shell", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"M", L"mx", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEARDOWN()
{
	vle_keys_set_def_handler(CMDLINE_MODE, NULL);
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
}

static int
handler(wchar_t key)
{
	mapping_states += (vle_keys_mapping_state() != 0);
	return 0;
}

TEST(none_mapping_as_initial_state)
{
	assert_int_equal(0, vle_keys_mapping_state());
}

TEST(none_mapping_after_calling_a_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_int_equal(0, vle_keys_mapping_state());
}

TEST(none_mapping_in_defhandler)
{
	mapping_states = 0;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_int_equal(0, mapping_states);
}

TEST(mapping_state_inside_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_true(key_is_mapped);
	assert_true(mapping_state != 0);
}

TEST(mapping_state_outside_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ma")));
	assert_false(key_is_mapped);
	assert_true(mapping_state == 0);
}

TEST(every_mapping_state_is_different)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"M")));
	assert_true(key_is_mapped);
	assert_true(mapping_state != 0);

	int prev_mapping_state = mapping_state;

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"M")));
	assert_true(key_is_mapped);
	assert_true(mapping_state != 0);
	assert_int_equal(prev_mapping_state + 1, mapping_state);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
