#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(delete_inexistent)
{
	assert_false(vle_abbr_remove(L"lhs") == 0);
}

TEST(delete_existent_by_lhs)
{
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"lhs"));

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"lhs"));
}

TEST(delete_existent_by_rhs)
{
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"rhs"));

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_success(vle_abbr_remove(L"rhs"));
}

TEST(delete_existent_by_rhs_check_all_lhs_first)
{
	assert_success(vle_abbr_add(L"a", L"b"));
	assert_success(vle_abbr_add_no_remap(L"b", L"c"));

	assert_success(vle_abbr_remove(L"b"));
	assert_success(vle_abbr_remove(L"a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
