#include <stic.h>

#include "../../src/engine/keys.h"

TEST(dont_treat_multikeys_as_motions)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"m"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"dm"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
