#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(delete_inexistent)
{
	assert_false(vle_abbr_remove("lhs") == 0);
}

TEST(delete_existent_by_lhs)
{
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
	assert_int_equal(0, vle_abbr_remove("lhs"));

	assert_int_equal(0, vle_abbr_add_no_remap("lhs", "rhs"));
	assert_int_equal(0, vle_abbr_remove("lhs"));
}

TEST(delete_existent_by_rhs)
{
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
	assert_int_equal(0, vle_abbr_remove("rhs"));

	assert_int_equal(0, vle_abbr_add_no_remap("lhs", "rhs"));
	assert_int_equal(0, vle_abbr_remove("rhs"));
}

TEST(delete_existent_by_rhs_check_all_lhs_first)
{
	assert_int_equal(0, vle_abbr_add("a", "b"));
	assert_int_equal(0, vle_abbr_add_no_remap("b", "c"));

	assert_int_equal(0, vle_abbr_remove("b"));
	assert_int_equal(0, vle_abbr_remove("a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
