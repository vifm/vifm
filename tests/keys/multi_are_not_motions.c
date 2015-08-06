#include <stic.h>

#include "../../src/engine/keys.h"

TEST(dont_treat_multikeys_as_motions)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"dm"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
