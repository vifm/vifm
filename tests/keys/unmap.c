#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(unmap_users)
{
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L","));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"q"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"s"));

	assert_success(vle_keys_user_add(L",q", L"k", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_success(vle_keys_user_add(L",s", L"j", NORMAL_MODE, KEYS_FLAG_NONE));

	assert_int_equal(KEYS_WAIT, vle_keys_exec(L","));

	assert_success(vle_keys_exec(L",q"));
	assert_success(vle_keys_user_remove(L",q", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L",q"));

	assert_success(vle_keys_exec(L",s"));
	assert_success(vle_keys_user_remove(L",s", NORMAL_MODE));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L",s"));

	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L","));
}

TEST(unmap_parent_chunk_before_child)
{
	vle_keys_user_add(L"k", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"kk", L"jj", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_true(vle_keys_user_exists(L"k", NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"kk", NORMAL_MODE));

	assert_success(vle_keys_user_remove(L"k", NORMAL_MODE));
	assert_success(vle_keys_user_remove(L"kk", NORMAL_MODE));

	assert_false(vle_keys_user_exists(L"k", NORMAL_MODE));
	assert_false(vle_keys_user_exists(L"kk", NORMAL_MODE));
}

TEST(unmapping_removes_non_mapped_parents)
{
	assert_success(vle_keys_user_add(L"k1", L"k1", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_success(vle_keys_user_add(L"k123", L"k123", NORMAL_MODE,
				KEYS_FLAG_NONE));

	/* Closest mapped parent remains. */
	assert_success(vle_keys_user_remove(L"k123", NORMAL_MODE));
	assert_false(vle_keys_user_exists(L"k12", NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"k1", NORMAL_MODE));

	/* The whole chain is gone. */
	assert_success(vle_keys_user_remove(L"k1", NORMAL_MODE));
	assert_false(vle_keys_user_exists(L"k", NORMAL_MODE));
}

TEST(unmap_remapped)
{
	assert_success(vle_keys_exec(L"j"));

	assert_success(vle_keys_user_add(L"j", L"k", NORMAL_MODE, KEYS_FLAG_NONE));

	assert_success(vle_keys_exec(L"j"));

	assert_success(vle_keys_exec(L"j"));
	assert_success(vle_keys_user_remove(L"j", NORMAL_MODE));
	assert_success(vle_keys_exec(L"j"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
