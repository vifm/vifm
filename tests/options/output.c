#include <stic.h>

#include "../../src/engine/options.h"

extern int fastrun;
extern const char *value;

TEST(get_option_value_empty_for_bool)
{
	assert_true(set_options("fastrun") == 0);
	assert_string_equal("", get_option_value("fastrun"));

	assert_true(set_options("nofastrun") == 0);
	assert_string_equal("", get_option_value("fastrun"));
}

TEST(get_option_value_works_for_int_no_leading_zeroes)
{
	assert_true(set_options("tabstop=8") == 0);
	assert_string_equal("8", get_option_value("tabstop"));

	assert_true(set_options("tabstop=08") == 0);
	assert_string_equal("8", get_option_value("tabstop"));
}

TEST(get_option_value_works_for_str)
{
	assert_true(set_options("fusehome=/") == 0);
	assert_string_equal("/", get_option_value("fusehome"));

	assert_true(set_options("fusehome=") == 0);
	assert_string_equal("", get_option_value("fusehome"));
}

TEST(get_option_value_works_for_strlist)
{
	assert_true(set_options("cdpath=a,b,c") == 0);
	assert_string_equal("a,b,c", get_option_value("cdpath"));

	assert_true(set_options("cdpath-=b") == 0);
	assert_string_equal("a,c", get_option_value("cdpath"));
}

TEST(get_option_value_works_for_enum)
{
	assert_true(set_options("sort=name") == 0);
	assert_string_equal("name", get_option_value("sort"));
}

TEST(get_option_value_works_for_set)
{
	assert_true(set_options("vifminfo=tui") == 0);
	assert_string_equal("tui", get_option_value("vifminfo"));
}

TEST(get_option_value_works_for_charset)
{
	assert_true(set_options("cpoptions=a") == 0);
	assert_string_equal("a", get_option_value("cpoptions"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
