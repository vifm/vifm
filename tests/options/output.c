#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern const char *value;

TEST(get_option_value_empty_for_bool)
{
	assert_true(set_options("fastrun", OPT_GLOBAL) == 0);
	assert_string_equal("", get_option_value("fastrun", OPT_GLOBAL));

	assert_true(set_options("nofastrun", OPT_GLOBAL) == 0);
	assert_string_equal("", get_option_value("fastrun", OPT_GLOBAL));
}

TEST(get_option_value_works_for_int_no_leading_zeroes)
{
	assert_true(set_options("tabstop=8", OPT_GLOBAL) == 0);
	assert_string_equal("8", get_option_value("tabstop", OPT_GLOBAL));

	assert_true(set_options("tabstop=08", OPT_GLOBAL) == 0);
	assert_string_equal("8", get_option_value("tabstop", OPT_GLOBAL));
}

TEST(get_option_value_works_for_str)
{
	assert_true(set_options("fusehome=/", OPT_GLOBAL) == 0);
	assert_string_equal("/", get_option_value("fusehome", OPT_GLOBAL));

	assert_true(set_options("fusehome=", OPT_GLOBAL) == 0);
	assert_string_equal("", get_option_value("fusehome", OPT_GLOBAL));
}

TEST(get_option_value_works_for_strlist)
{
	assert_true(set_options("cdpath=a,b,c", OPT_GLOBAL) == 0);
	assert_string_equal("a,b,c", get_option_value("cdpath", OPT_GLOBAL));

	assert_true(set_options("cdpath-=b", OPT_GLOBAL) == 0);
	assert_string_equal("a,c", get_option_value("cdpath", OPT_GLOBAL));
}

TEST(get_option_value_works_for_enum)
{
	assert_true(set_options("sort=name", OPT_GLOBAL) == 0);
	assert_string_equal("name", get_option_value("sort", OPT_GLOBAL));
}

TEST(get_option_value_works_for_set)
{
	assert_true(set_options("vifminfo=tui", OPT_GLOBAL) == 0);
	assert_string_equal("tui", get_option_value("vifminfo", OPT_GLOBAL));
}

TEST(get_option_value_works_for_charset)
{
	assert_true(set_options("cpoptions=a", OPT_GLOBAL) == 0);
	assert_string_equal("a", get_option_value("cpoptions", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
