#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

static void key_X(key_info_t key_info, keys_info_t *keys_info);

static int called;

SETUP()
{
	called = 0;
}

TEST(add_foreign_key)
{
	key_conf_t key = { { &key_X } };
	assert_success(vle_keys_foreign_add(L"X", &key, NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"X", NORMAL_MODE));

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"X")));
	assert_int_equal(1, called);
}

TEST(replace_user_key_by_foreign_key)
{
	assert_success(vle_keys_user_add(L"X", L"dd", NORMAL_MODE, KEYS_FLAG_NONE));

	key_conf_t key = { { &key_X } };
	assert_success(vle_keys_foreign_add(L"X", &key, NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"X", NORMAL_MODE));

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"X")));
	assert_int_equal(1, called);
}

static void
key_X(key_info_t key_info, keys_info_t *keys_info)
{
	called = 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
