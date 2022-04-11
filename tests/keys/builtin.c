#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void key_X(key_info_t key_info, keys_info_t *keys_info);

static void *user_data;

SETUP()
{
    user_data = NULL;
}

TEST(handler_gets_user_data)
{
    int var;
	keys_add_info_t key = { L"X", {{ &key_X }, .user_data = &var } };
	assert_success(vle_keys_add(&key, 1, NORMAL_MODE));

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"X")));
	assert_true(user_data == &var);
}

TEST(builtin_selectors_can_not_be_cleared)
{
	vle_keys_user_clear();

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"dj")));
}

static void
key_X(key_info_t key_info, keys_info_t *keys_info)
{
	user_data = key_info.user_data;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
