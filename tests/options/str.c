#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

TEST(string_append)
{
	optval_t val = { .str_val = "/home/tmp" };
	set_option("fusehome", val, OPT_GLOBAL);

	assert_success(set_options("fusehome+=/tmp", OPT_GLOBAL));
	assert_string_equal("/home/tmp/tmp", value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
