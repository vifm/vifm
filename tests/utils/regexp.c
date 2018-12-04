#include <stic.h>

#include "../../src/utils/regexp.h"

TEST(bad_regex_leaves_line_unchanged)
{
	assert_string_equal("barfoobar",
			regexp_replace("barfoobar", "*foo", "z", 1, 0));
}

TEST(no_infinite_loop_on_empty_global_match)
{
	assert_string_equal("zbarfoobar", regexp_replace("barfoobar", "", "z", 1, 0));
}

TEST(substitute_segfault_bug)
{
	/* see #SF3515922 */
	assert_string_equal("barfoobar", regexp_replace("foobar", "^", "bar", 1, 0));
	assert_string_equal("01", regexp_replace("001", "^0", "", 1, 0));
}

TEST(substitute_begin_global)
{
	assert_string_equal("01", regexp_replace("001", "^0", "", 1, 0));
}

TEST(substitute_end_global)
{
	assert_string_equal("10", regexp_replace("100", "0$", "", 1, 0));
}

TEST(back_reference_substitution)
{
	assert_string_equal("barfoobaz",
			regexp_replace("bazfoobar", "(.*)foo(.*)", "\\2foo\\1", 1, 0));

	assert_string_equal("barbaz", regexp_replace("foobaz", "foo", "bar\\", 1, 0));

	assert_string_equal("f0t0tbaz", regexp_replace("foobaz", "o", "0\\t", 1, 0));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
