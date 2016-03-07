#include <stic.h>

#include "../../src/engine/keys.h"

TEST(keys_dj)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"dj")));
}

TEST(keys_dk)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"dk")));
}

TEST(keys_2dj)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"2"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"2d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"2dj")));
}

TEST(keys_d3k)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d3"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"d3k")));
}

TEST(keys_4dj)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"4"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"4d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"4dj")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
