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
	assert_success(add_user_keys(L"w", L"k", NORMAL_MODE, 0));

	assert_false(IS_KEYS_RET_CODE(execute_keys(L"w")));
	assert_true(IS_KEYS_RET_CODE(execute_keys_no_remap(L"w")));

	assert_false(IS_KEYS_RET_CODE(execute_keys_timed_out(L"w")));
	assert_true(IS_KEYS_RET_CODE(execute_keys_timed_out_no_remap(L"w")));
}

TEST(remap_selector_waitpoint)
{
	assert_success(add_user_keys(L"y", L"d", NORMAL_MODE, 1));

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"yj")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"y2j")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"2yk")));
	assert_int_equal(4, last);

	assert_success(add_user_keys(L"z", L"d", NORMAL_MODE, 1));

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"zj")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"z2j")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"2zk")));
	assert_int_equal(4, last);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
