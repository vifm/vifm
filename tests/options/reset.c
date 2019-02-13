#include <stic.h>

#include "../../src/engine/options.h"

extern int fusehome_handler_calls;
extern const char *value;

TEST(reset_of_str_option_not_differ_no_callback)
{
	int res;

	fusehome_handler_calls = 0;
	res = vle_opts_set("fusehome&", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_int_equal(0, fusehome_handler_calls);
}

TEST(reset_of_str_option_differ_callback)
{
	int res;

	res = vle_opts_set("fusehome='abc'", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_string_equal("abc", value);

	fusehome_handler_calls = 0;
	res = vle_opts_set("fusehome&", OPT_GLOBAL);
	assert_int_equal(0, res);
	assert_int_equal(1, fusehome_handler_calls);
	assert_string_equal("fusehome-default", value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
