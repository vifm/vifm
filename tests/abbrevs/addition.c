#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(adds_fine_first_time)
{
	int no_remap;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));
}

TEST(overwriten_on_second_addition)
{
	int no_remap;

	assert_success(vle_abbr_add(L"lhs1", L"rhs1_1"));
	assert_wstring_equal(L"rhs1_1", vle_abbr_expand(L"lhs1", &no_remap));
	assert_success(vle_abbr_add(L"lhs1", L"rhs1_2"));
	assert_wstring_equal(L"rhs1_2", vle_abbr_expand(L"lhs1", &no_remap));
	assert_success(vle_abbr_add_no_remap(L"lhs1", L"rhs1_3"));
	assert_wstring_equal(L"rhs1_3", vle_abbr_expand(L"lhs1", &no_remap));

	assert_success(vle_abbr_add_no_remap(L"lhs2", L"rhs2_1"));
	assert_wstring_equal(L"rhs2_1", vle_abbr_expand(L"lhs2", &no_remap));
	assert_success(vle_abbr_add(L"lhs2", L"rhs2_2"));
	assert_wstring_equal(L"rhs2_2", vle_abbr_expand(L"lhs2", &no_remap));
	assert_success(vle_abbr_add_no_remap(L"lhs2", L"rhs2_3"));
	assert_wstring_equal(L"rhs2_3", vle_abbr_expand(L"lhs2", &no_remap));
}

TEST(overwrite_changes_mapping_type)
{
	int no_remap;

	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);

	assert_success(vle_abbr_add_no_remap(L"lhs", L"rhs"));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_true(no_remap);

	assert_success(vle_abbr_add(L"lhs", L"rhs"));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
