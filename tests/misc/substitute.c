#include <stic.h>

#include "../../src/fileops.h"

TEST(substitute_segfault_bug)
{
	/* see #SF3515922 */
	assert_string_equal("barfoobar", substitute_in_name("foobar", "^", "bar", 1));
}

TEST(substitute_begin_global)
{
	assert_string_equal("01", substitute_in_name("001", "^0", "", 1));
}

TEST(substitute_end_global)
{
	assert_string_equal("10", substitute_in_name("100", "0$", "", 1));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
