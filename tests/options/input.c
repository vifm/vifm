#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

TEST(colon)
{
	optval_t val = { .str_val = "/home/tmp" };
	set_option("fusehome", val);

	assert_true(set_options("fusehome=/tmp") == 0);
	assert_string_equal("/tmp", value);

	assert_true(set_options("fusehome:/var/fuse") == 0);
	assert_string_equal("/var/fuse", value);
}

TEST(print_all_pseudo_option)
{
	assert_success(set_options("all"));
}

TEST(reset_all_pseudo_option)
{
	extern int fastrun;

	optval_t val = { .bool_val = 1 };

	fastrun = val.bool_val;
	set_option("fastrun", val);
	assert_success(set_options("all&"));
	assert_false(fastrun);

	fastrun = val.bool_val;
	set_option("fastrun", val);
	assert_success(set_options("all &"));
	assert_false(fastrun);

	fastrun = val.bool_val;
	set_option("fastrun", val);
	assert_success(set_options("all   &"));
	assert_false(fastrun);
}

TEST(wrong_all_pseudo_option_suffixes)
{
	assert_failure(set_options("all!"));
	assert_failure(set_options("all !"));
	assert_failure(set_options("all #"));
	assert_failure(set_options("all#"));
}

TEST(wrong_all_pseudo_option_prefixes)
{
	assert_failure(set_options("noall"));
	assert_failure(set_options("invall"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
