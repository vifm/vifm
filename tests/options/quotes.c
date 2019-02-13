#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

TEST(no_quotes)
{
	value = NULL;
	vle_opts_set("fusehome=a\\ b", OPT_GLOBAL);
	assert_string_equal("a b", value);

	value = NULL;
	vle_opts_set("fh=a\\ b\\ c", OPT_GLOBAL);
	assert_string_equal("a b c", value);

	vle_opts_set("so=name", OPT_GLOBAL);
}

TEST(single_quotes)
{
	vle_opts_set("fusehome='a b'", OPT_GLOBAL);
	assert_string_equal("a b", value);
}

TEST(double_quotes)
{
	vle_opts_set("fusehome=\"a b\"", OPT_GLOBAL);
	assert_string_equal("a b", value);

	vle_opts_set("fusehome=\"a \\\" b\"", OPT_GLOBAL);
	assert_string_equal("a \" b", value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
