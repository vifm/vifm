#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int last;

TEST(without_noremap)
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

TEST(with_noremap)
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

TEST(noremap_functions)
{
	assert_int_equal(0, add_user_keys(L"y", L"k", NORMAL_MODE, 0));

	assert_false(IS_KEYS_RET_CODE(execute_keys(L"y")));
	assert_true(IS_KEYS_RET_CODE(execute_keys_no_remap(L"y")));

	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"y")));
	assert_true(IS_KEYS_RET_CODE(execute_keys_timed_out_no_remap(L"y")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
