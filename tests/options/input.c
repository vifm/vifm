#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

TEST(colon)
{
	optval_t val = { .str_val = "/home/tmp" };
	vle_opts_assign("fusehome", val, OPT_GLOBAL);

	assert_true(vle_opts_set("fusehome=/tmp", OPT_GLOBAL) == 0);
	assert_string_equal("/tmp", value);

	assert_true(vle_opts_set("fusehome:/var/fuse", OPT_GLOBAL) == 0);
	assert_string_equal("/var/fuse", value);
}

TEST(comments)
{
	optval_t val = { .str_val = "/home/tmp" };
	vle_opts_assign("fusehome", val, OPT_GLOBAL);

	assert_true(vle_opts_set("fusehome=/tmp \"bla", OPT_GLOBAL) == 0);
	assert_string_equal("/tmp", value);

	assert_true(vle_opts_set("fusehome:/var/fuse \"comment", OPT_GLOBAL) == 0);
	assert_string_equal("/var/fuse", value);
}

TEST(unmatched_quote)
{
	assert_failure(vle_opts_set("fusehome='/tmp", OPT_GLOBAL));
	assert_failure(vle_opts_set("fusehome=\"/var/fuse", OPT_GLOBAL));
}

TEST(print_all_pseudo_option)
{
	assert_success(vle_opts_set("all", OPT_GLOBAL));
}

TEST(reset_all_pseudo_option)
{
	extern int fastrun;

	optval_t val = { .bool_val = 1 };

	fastrun = val.bool_val;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);
	assert_success(vle_opts_set("all&", OPT_GLOBAL));
	assert_false(fastrun);

	fastrun = val.bool_val;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);
	assert_success(vle_opts_set("all &", OPT_GLOBAL));
	assert_false(fastrun);

	fastrun = val.bool_val;
	vle_opts_assign("fastrun", val, OPT_GLOBAL);
	assert_success(vle_opts_set("all   &", OPT_GLOBAL));
	assert_false(fastrun);
}

TEST(wrong_all_pseudo_option_suffixes)
{
	assert_failure(vle_opts_set("all!", OPT_GLOBAL));
	assert_failure(vle_opts_set("all !", OPT_GLOBAL));
	assert_failure(vle_opts_set("all #", OPT_GLOBAL));
	assert_failure(vle_opts_set("all#", OPT_GLOBAL));
}

TEST(wrong_all_pseudo_option_prefixes)
{
	assert_failure(vle_opts_set("noall", OPT_GLOBAL));
	assert_failure(vle_opts_set("invall", OPT_GLOBAL));
}

TEST(huge_input_length)
{
	assert_success(vle_opts_set("fusehome=llongtextlongtextlongtextlongtextlongte"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlongtextlo"
			"ngtextlongtextlongtextlongtextlongtextlongtextlongtextadfadfasdfasdfasfa"
			"sdfsdft", OPT_GLOBAL));
}

TEST(very_long_option_name_does_not_cause_crash)
{
	assert_failure(vle_opts_set("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
			"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
			"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx!", OPT_GLOBAL));
}

TEST(weird_input)
{
	assert_failure(vle_opts_set("opt        !opt", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
