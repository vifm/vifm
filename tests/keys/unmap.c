#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(unmap_users)
{
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L","));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"q"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"s"));

	assert_success(vle_keys_user_add(L",q", L"k", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_success(vle_keys_user_add(L",s", L"j", NORMAL_MODE, KEYS_FLAG_NONE));

	assert_int_equal(KEYS_WAIT, vle_keys_exec(L","));

	assert_success(vle_keys_exec(L",q"));
	assert_success(vle_keys_user_remove(L",q", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L",q"));

	assert_success(vle_keys_exec(L",s"));
	assert_success(vle_keys_user_remove(L",s", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L",s"));

	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L","));
}

TEST(unmap_remapped)
{
	assert_success(vle_keys_exec(L"j"));

	assert_success(vle_keys_user_add(L"j", L"k", NORMAL_MODE, KEYS_FLAG_NONE));

	assert_success(vle_keys_exec(L"j"));

	assert_success(vle_keys_exec(L"j"));
	assert_success(vle_keys_user_remove(L"j", NORMAL_MODE));
	assert_success(vle_keys_exec(L"j"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
