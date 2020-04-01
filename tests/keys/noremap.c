#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

TEST(without_noremap)
{
	assert_int_equal(0, vle_keys_exec(L"k"));
	assert_int_equal(1, last);

	assert_int_equal(0, vle_keys_exec(L"j"));
	assert_int_equal(2, last);

	assert_int_equal(0, vle_keys_user_add(L"j", L"k", NORMAL_MODE,
				KEYS_FLAG_NONE));
	assert_int_equal(0, vle_keys_user_add(L"q", L"j", NORMAL_MODE,
				KEYS_FLAG_NONE));

	assert_int_equal(0, vle_keys_exec(L"j"));
	assert_int_equal(1, last);

	assert_int_equal(0, vle_keys_exec(L"q"));
	assert_int_equal(1, last);
}

TEST(with_noremap)
{
	assert_int_equal(0, vle_keys_exec(L"k"));
	assert_int_equal(1, last);

	assert_int_equal(0, vle_keys_exec(L"j"));
	assert_int_equal(2, last);

	assert_int_equal(0, vle_keys_user_add(L"j", L"k", NORMAL_MODE,
				KEYS_FLAG_NONE));
	assert_int_equal(0, vle_keys_user_add(L"q", L"j", NORMAL_MODE,
				KEYS_FLAG_NOREMAP));

	assert_int_equal(0, vle_keys_exec(L"j"));
	assert_int_equal(1, last);

	assert_int_equal(0, vle_keys_exec(L"q"));
	assert_int_equal(2, last);
}

TEST(noremap_functions)
{
	assert_success(vle_keys_user_add(L"w", L"k", NORMAL_MODE, KEYS_FLAG_NONE));

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"w")));
	assert_true(IS_KEYS_RET_CODE(vle_keys_exec_no_remap(L"w")));

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec_timed_out(L"w")));
	assert_true(IS_KEYS_RET_CODE(vle_keys_exec_timed_out_no_remap(L"w")));
}

TEST(remap_selector_waitpoint)
{
	assert_success(vle_keys_user_add(L"y", L"d", NORMAL_MODE, KEYS_FLAG_NOREMAP));

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"yj")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"y2j")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"2yk")));
	assert_int_equal(4, last);

	assert_success(vle_keys_user_add(L"z", L"d", NORMAL_MODE, KEYS_FLAG_NOREMAP));

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"zj")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"z2j")));
	assert_int_equal(4, last);

	last = -1;
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"2zk")));
	assert_int_equal(4, last);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
