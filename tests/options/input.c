#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

TEST(colon)
{
	optval_t val = { .str_val = "/home/tmp" };
	set_option("fusehome", val, OPT_GLOBAL);

	assert_true(set_options("fusehome=/tmp", OPT_GLOBAL) == 0);
	assert_string_equal("/tmp", value);

	assert_true(set_options("fusehome:/var/fuse", OPT_GLOBAL) == 0);
	assert_string_equal("/var/fuse", value);
}

TEST(print_all_pseudo_option)
{
	assert_success(set_options("all", OPT_GLOBAL));
}

TEST(reset_all_pseudo_option)
{
	extern int fastrun;

	optval_t val = { .bool_val = 1 };

	fastrun = val.bool_val;
	set_option("fastrun", val, OPT_GLOBAL);
	assert_success(set_options("all&", OPT_GLOBAL));
	assert_false(fastrun);

	fastrun = val.bool_val;
	set_option("fastrun", val, OPT_GLOBAL);
	assert_success(set_options("all &", OPT_GLOBAL));
	assert_false(fastrun);

	fastrun = val.bool_val;
	set_option("fastrun", val, OPT_GLOBAL);
	assert_success(set_options("all   &", OPT_GLOBAL));
	assert_false(fastrun);
}

TEST(wrong_all_pseudo_option_suffixes)
{
	assert_failure(set_options("all!", OPT_GLOBAL));
	assert_failure(set_options("all !", OPT_GLOBAL));
	assert_failure(set_options("all #", OPT_GLOBAL));
	assert_failure(set_options("all#", OPT_GLOBAL));
}

TEST(wrong_all_pseudo_option_prefixes)
{
	assert_failure(set_options("noall", OPT_GLOBAL));
	assert_failure(set_options("invall", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
