#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(multichar_selector)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"di"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dif")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
