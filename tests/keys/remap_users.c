#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
allow_user_key_remap(void)
{
	assert_int_equal(0, add_user_keys(L"jo", L":do movement", NORMAL_MODE));
	assert_int_equal(0, add_user_keys(L"jo", L":leave insert mode", NORMAL_MODE));
}

void
remap_users(void)
{
	test_fixture_start();

	run_test(allow_user_key_remap);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
