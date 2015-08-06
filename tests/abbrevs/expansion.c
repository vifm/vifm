#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(correct_expansion)
{
	int no_remap;
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));
}

TEST(correct_expansion_pick)
{
	int no_remap;
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_success(vle_abbr_add(L"prefixlhs", L"prefixrhs"));
	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));
}

TEST(no_remap_flag_false)
{
	int no_remap = -1;
	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_true(vle_abbr_expand(L"lhs", &no_remap) != NULL);
	assert_false(no_remap);
}

TEST(no_remap_flag_true)
{
	int no_remap = -1;
	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_true(vle_abbr_expand(L"lhs", &no_remap) != NULL);
	assert_true(no_remap);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
