#include "seatest.h"

#include "../../src/engine/keys.h"

static void
test_dj(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dj")));
}

static void
test_dk(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dk")));
}

static void
test_2dj(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"2"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"2d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"2dj")));
}

static void
test_d3k(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"d3"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"d3k")));
}

static void
test_4dj(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"4"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"4d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"4dj")));
}

void
motions(void)
{
	test_fixture_start();

	run_test(test_dj);
	run_test(test_dk);
	run_test(test_2dj);
	run_test(test_d3k);
	run_test(test_4dj);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
