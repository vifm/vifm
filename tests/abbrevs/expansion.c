#include <stic.h>

#include "../../src/engine/abbrevs.h"

TEST(correct_expansion)
{
	int no_remap;
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
	assert_string_equal("rhs", vle_abbr_expand("lhs", &no_remap));
}

TEST(correct_expansion_pick)
{
	int no_remap;
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
	assert_int_equal(0, vle_abbr_add("prefixlhs", "prefixrhs"));
	assert_string_equal("rhs", vle_abbr_expand("lhs", &no_remap));
}

TEST(no_remap_flag_false)
{
	int no_remap = -1;
	assert_int_equal(0, vle_abbr_add("lhs", "rhs"));
	assert_true(vle_abbr_expand("lhs", &no_remap) != NULL);
	assert_int_equal(0, no_remap);
}

TEST(no_remap_flag_true)
{
	int no_remap = -1;
	assert_int_equal(0, vle_abbr_add_no_remap("lhs", "rhs"));
	assert_true(vle_abbr_expand("lhs", &no_remap) != NULL);
	assert_int_equal(1, no_remap);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
