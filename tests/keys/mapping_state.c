#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

static int handler(wchar_t key);

extern int key_is_mapped;
extern int is_in_maping_state;

static int mapping_states;

SETUP()
{
	vle_keys_set_def_handler(CMDLINE_MODE, &handler);
	vle_keys_user_add(L"s", L":shell", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEARDOWN()
{
	vle_keys_set_def_handler(CMDLINE_MODE, NULL);
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
}

static int
handler(wchar_t key)
{
	mapping_states += (vle_keys_inside_mapping() != 0);
	return 0;
}

TEST(none_mapping_as_initial_state)
{
	assert_false(vle_keys_inside_mapping());
}

TEST(none_mapping_after_calling_a_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_false(vle_keys_inside_mapping());
}

TEST(none_mapping_in_defhandler)
{
	mapping_states = 0;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_int_equal(0, mapping_states);
}

TEST(mapping_inside_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_true(key_is_mapped);
	assert_true(is_in_maping_state);
}

TEST(mapping_outside_mapping)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ma")));
	assert_false(key_is_mapped);
	assert_false(is_in_maping_state);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
