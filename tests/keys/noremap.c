#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

extern int last;

static void
test_without_noremap(void)
{
	assert_int_equal(0, execute_keys(L"k"));
	assert_int_equal(1, last);

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(2, last);

	assert_int_equal(0, add_user_keys(L"j", L"k", NORMAL_MODE, 0));
	assert_int_equal(0, add_user_keys(L"q", L"j", NORMAL_MODE, 0));

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(1, last);

	assert_int_equal(0, execute_keys(L"q"));
	assert_int_equal(1, last);
}

static void
test_with_noremap(void)
{
	assert_int_equal(0, execute_keys(L"k"));
	assert_int_equal(1, last);

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(2, last);

	assert_int_equal(0, add_user_keys(L"j", L"k", NORMAL_MODE, 0));
	assert_int_equal(0, add_user_keys(L"q", L"j", NORMAL_MODE, 1));

	assert_int_equal(0, execute_keys(L"j"));
	assert_int_equal(1, last);

	assert_int_equal(0, execute_keys(L"q"));
	assert_int_equal(2, last);
}

void
noremap_tests(void)
{
	test_fixture_start();

	run_test(test_without_noremap);
	run_test(test_with_noremap);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
