#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern const char *value;

TEST(get_option_value_empty_for_bool)
{
	assert_true(vle_opts_set("fastrun", OPT_GLOBAL) == 0);
	assert_string_equal("", vle_opts_get("fastrun", OPT_GLOBAL));

	assert_true(vle_opts_set("nofastrun", OPT_GLOBAL) == 0);
	assert_string_equal("", vle_opts_get("fastrun", OPT_GLOBAL));
}

TEST(get_option_value_works_for_int_no_leading_zeroes)
{
	assert_true(vle_opts_set("tabstop=8", OPT_GLOBAL) == 0);
	assert_string_equal("8", vle_opts_get("tabstop", OPT_GLOBAL));

	assert_true(vle_opts_set("tabstop=08", OPT_GLOBAL) == 0);
	assert_string_equal("8", vle_opts_get("tabstop", OPT_GLOBAL));
}

TEST(get_option_value_works_for_str)
{
	assert_true(vle_opts_set("fusehome=/", OPT_GLOBAL) == 0);
	assert_string_equal("/", vle_opts_get("fusehome", OPT_GLOBAL));

	assert_true(vle_opts_set("fusehome=", OPT_GLOBAL) == 0);
	assert_string_equal("", vle_opts_get("fusehome", OPT_GLOBAL));
}

TEST(get_option_value_works_for_strlist)
{
	assert_true(vle_opts_set("cdpath=a,b,c", OPT_GLOBAL) == 0);
	assert_string_equal("a,b,c", vle_opts_get("cdpath", OPT_GLOBAL));

	assert_true(vle_opts_set("cdpath-=b", OPT_GLOBAL) == 0);
	assert_string_equal("a,c", vle_opts_get("cdpath", OPT_GLOBAL));
}

TEST(get_option_value_works_for_enum)
{
	assert_true(vle_opts_set("sort=name", OPT_GLOBAL) == 0);
	assert_string_equal("name", vle_opts_get("sort", OPT_GLOBAL));
}

TEST(get_option_value_works_for_set)
{
	assert_true(vle_opts_set("vifminfo=tui", OPT_GLOBAL) == 0);
	assert_string_equal("tui", vle_opts_get("vifminfo", OPT_GLOBAL));
}

TEST(get_option_value_works_for_charset)
{
	assert_true(vle_opts_set("cpoptions=a", OPT_GLOBAL) == 0);
	assert_string_equal("a", vle_opts_get("cpoptions", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
