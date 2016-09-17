#include <stic.h>

#include "../../src/fops_rename.h"

TEST(substitute_segfault_bug)
{
	/* see #SF3515922 */
	assert_string_equal("barfoobar", fops_name_subst("foobar", "^", "bar", 1));
}

TEST(substitute_begin_global)
{
	assert_string_equal("01", fops_name_subst("001", "^0", "", 1));
}

TEST(substitute_end_global)
{
	assert_string_equal("10", fops_name_subst("100", "0$", "", 1));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
