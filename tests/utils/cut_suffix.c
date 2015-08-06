#include <stic.h>

#include "../../src/utils/str.h"

TEST(cut)
{
	char str[] = "abc.vifm";

	assert_true(cut_suffix(str, ".vifm"));
	assert_string_equal("abc", str);
}

TEST(no_cut)
{
	char str[] = "abc.vifm";

	assert_false(cut_suffix(str, ".vim"));
	assert_string_equal("abc.vifm", str);
}

TEST(whole_str)
{
	char str[] = ".vifm";

	assert_true(cut_suffix(str, ".vifm"));
	assert_string_equal("", str);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
