#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
test_unmap_users(void)
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

static void
test_unmap_remapped(void)
{
	assert_int_equal(0, execute_keys(L"j"));

	assert_int_equal(0, add_user_keys(L"j", L"k", NORMAL_MODE, 0));

	assert_int_equal(0, execute_keys(L"j"));

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(0, remove_user_keys(L"j", NORMAL_MODE));
	assert_int_equal(0, execute_keys(L"j"));
}

void
unmap_tests(void)
{
	test_fixture_start();

	run_test(test_unmap_users);
	run_test(test_unmap_remapped);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
