#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(adds_fine_first_time)
{
	assert_int_equal(0, vle_abbr_add(L"lhs", L"rhs"));
}

TEST(errors_on_second_addition)
{
	assert_int_equal(0, vle_abbr_add(L"lhs1", L"rhs1"));
	assert_false(vle_abbr_add(L"lhs1", L"rhs1") == 0);
	assert_false(vle_abbr_add_no_remap(L"lhs1", L"rhs1") == 0);

	assert_int_equal(0, vle_abbr_add_no_remap(L"lhs2", L"rhs2"));
	assert_false(vle_abbr_add(L"lhs2", L"rhs2") == 0);
	assert_false(vle_abbr_add_no_remap(L"lhs2", L"rhs2") == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
