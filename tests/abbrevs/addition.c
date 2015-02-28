#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(adds_fine_first_time)
{
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
}

TEST(errors_on_second_addition)
{
	assert_int_equal(0, vle_abbr_add("lhs1", "rhs1"));
	assert_false(vle_abbr_add("lhs1", "rhs1") == 0);
	assert_false(vle_abbr_add_no_remap("lhs1", "rhs1") == 0);

	assert_int_equal(0, vle_abbr_add_no_remap("lhs2", "rhs2"));
	assert_false(vle_abbr_add("lhs2", "rhs2") == 0);
	assert_false(vle_abbr_add_no_remap("lhs2", "rhs2") == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
