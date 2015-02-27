#include <stic.h>

#include "../../src/engine/keys.h"

TEST(multi_as_a_motion)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"'"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"d'"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"d'm")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
