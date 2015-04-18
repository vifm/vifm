#include <stic.h>

#include "../../src/engine/options.h"

extern char cpoptions[10];
extern int cpoptions_handler_calls;

TEST(hat_with_charset_addition)
{
	cpoptions[0] = '\0';

	cpoptions_handler_calls = 0;
	assert_success(set_options("cpoptions^=ac"));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("ac", cpoptions);
}

TEST(hat_with_charset_removal)
{
	cpoptions[0] = '\0';

	assert_success(set_options("cpoptions=abc"));

	cpoptions_handler_calls = 0;
	assert_success(set_options("cpoptions^=bccb"));
	assert_int_equal(1, cpoptions_handler_calls);
	assert_string_equal("a", cpoptions);
}

TEST(hat_with_not_charset_error)
{
	/* TODO: to be implemented for some of option types. */

	cpoptions[0] = '\0';

	assert_failure(set_options("cdpath^=/some/path"));
	assert_failure(set_options("fastrun^=a"));
	assert_failure(set_options("fusehome^=/new/path"));
	assert_failure(set_options("sort^=gid"));
	assert_failure(set_options("tabstop^=5"));
	assert_failure(set_options("vifminfo^=dhistory"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
