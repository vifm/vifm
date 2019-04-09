#include <stic.h>

#include "../../src/engine/options.h"

extern const char *value;

SETUP()
{
	assert_success(vle_opts_set("cdpath=/path", OPT_GLOBAL));
	assert_string_equal("/path", value);
}

TEST(string_list_add)
{
	assert_success(vle_opts_set("cdpath+=/path/1", OPT_GLOBAL));
	assert_string_equal("/path,/path/1", value);
}

TEST(string_list_add_to_empty)
{
	assert_success(vle_opts_set("cdpath=", OPT_GLOBAL));
	assert_string_equal("", value);
	assert_success(vle_opts_set("cdpath+=/path/1", OPT_GLOBAL));
	assert_string_equal("/path/1", value);
}

TEST(string_list_remove)
{
	assert_success(vle_opts_set("cdpath-=/path", OPT_GLOBAL));
	assert_string_equal("", value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
