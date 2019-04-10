#include <stic.h>

#include "../../src/engine/options.h"

extern char cpoptions[10];
extern int cpoptions_handler_calls;
extern const char *value;
extern int vifminfo;

TEST(hat_with_charset_addition)
{
	cpoptions[0] = '\0';

	cpoptions_handler_calls = 0;
	assert_success(vle_opts_set("cpoptions^=ac", OPT_GLOBAL));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("ac", cpoptions);
}

TEST(hat_with_charset_removal)
{
	cpoptions[0] = '\0';

	assert_success(vle_opts_set("cpoptions=abc", OPT_GLOBAL));

	cpoptions_handler_calls = 0;
	assert_success(vle_opts_set("cpoptions^=bccb", OPT_GLOBAL));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("a", cpoptions);
}

TEST(hat_with_strlist_addition)
{
	assert_success(vle_opts_set("cdpath^=/some/path", OPT_GLOBAL));
	assert_string_equal("/some/path", value);
	assert_success(vle_opts_set("cdpath^=/some/new-path", OPT_GLOBAL));
	assert_string_equal("/some/path,/some/new-path", value);
}

TEST(hat_with_strlist_removal)
{
	assert_success(vle_opts_set("cdpath=/some/path", OPT_GLOBAL));
	assert_string_equal("/some/path", value);
	assert_success(vle_opts_set("cdpath^=/some/path", OPT_GLOBAL));
	assert_string_equal("", value);
}

TEST(hat_with_set_addition)
{
	assert_success(vle_opts_set("vifminfo=", OPT_GLOBAL));
	assert_int_equal(0, vifminfo);
	assert_success(vle_opts_set("vifminfo^=options", OPT_GLOBAL));
	assert_int_equal(1, vifminfo);
	assert_success(vle_opts_set("vifminfo^=filetypes", OPT_GLOBAL));
	assert_int_equal(3, vifminfo);
}

TEST(hat_with_set_removal)
{
	assert_success(vle_opts_set("vifminfo=options,filetypes", OPT_GLOBAL));
	assert_int_equal(3, vifminfo);
	assert_success(vle_opts_set("vifminfo^=options", OPT_GLOBAL));
	assert_int_equal(2, vifminfo);
	assert_success(vle_opts_set("vifminfo^=filetypes", OPT_GLOBAL));
	assert_int_equal(0, vifminfo);
}

TEST(hat_with_set_toggle)
{
	assert_success(vle_opts_set("vifminfo=options,filetypes", OPT_GLOBAL));
	assert_int_equal(3, vifminfo);
	assert_success(vle_opts_set("vifminfo^=options,commands", OPT_GLOBAL));
	assert_int_equal(6, vifminfo);
	assert_success(vle_opts_set("vifminfo^=filetypes,options", OPT_GLOBAL));
	assert_int_equal(5, vifminfo);
}

TEST(hat_with_not_charset_error)
{
	assert_failure(vle_opts_set("fastrun^=a", OPT_GLOBAL));
	assert_failure(vle_opts_set("fusehome^=/new/path", OPT_GLOBAL));
	assert_failure(vle_opts_set("sort^=gid", OPT_GLOBAL));
	assert_failure(vle_opts_set("tabstop^=5", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
