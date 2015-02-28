#include <stic.h>

#include "../../src/engine/keys.h"

TEST(treat_full_commands_right)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"\""));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\""));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\"r"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\"r1"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"\"a1d\"r1k")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
