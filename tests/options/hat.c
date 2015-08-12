#include <stic.h>

#include "../../src/engine/options.h"

extern char cpoptions[10];
extern int cpoptions_handler_calls;

TEST(hat_with_charset_addition)
{
	cpoptions[0] = '\0';

	cpoptions_handler_calls = 0;
	assert_success(set_options("cpoptions^=ac", OPT_GLOBAL));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("ac", cpoptions);
}

TEST(hat_with_charset_removal)
{
	cpoptions[0] = '\0';

	assert_success(set_options("cpoptions=abc", OPT_GLOBAL));

	cpoptions_handler_calls = 0;
	assert_success(set_options("cpoptions^=bccb", OPT_GLOBAL));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("a", cpoptions);
}

TEST(hat_with_not_charset_error)
{
	/* TODO: to be implemented for some of option types. */

	cpoptions[0] = '\0';

	assert_failure(set_options("cdpath^=/some/path", OPT_GLOBAL));
	assert_failure(set_options("fastrun^=a", OPT_GLOBAL));
	assert_failure(set_options("fusehome^=/new/path", OPT_GLOBAL));
	assert_failure(set_options("sort^=gid", OPT_GLOBAL));
	assert_failure(set_options("tabstop^=5", OPT_GLOBAL));
	assert_failure(set_options("vifminfo^=dhistory", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
