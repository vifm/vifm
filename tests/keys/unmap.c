#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(unmap_users)
{
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L","));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"q"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"s"));

	assert_int_equal(0, add_user_keys(L",q", L"k", NORMAL_MODE, 0));
	assert_int_equal(0, add_user_keys(L",s", L"j", NORMAL_MODE, 0));

	assert_int_equal(KEYS_WAIT, execute_keys(L","));

	assert_int_equal(0, execute_keys(L",q"));
	assert_int_equal(0, remove_user_keys(L",q", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L",q"));

	assert_int_equal(0, execute_keys(L",s"));
	assert_int_equal(0, remove_user_keys(L",s", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L",s"));

	assert_int_equal(KEYS_UNKNOWN, execute_keys(L","));
}

TEST(unmap_remapped)
{
	assert_int_equal(0, execute_keys(L"j"));

	assert_int_equal(0, add_user_keys(L"j", L"k", NORMAL_MODE, 0));

	assert_int_equal(0, execute_keys(L"j"));

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(0, remove_user_keys(L"j", NORMAL_MODE));
	assert_int_equal(0, execute_keys(L"j"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
