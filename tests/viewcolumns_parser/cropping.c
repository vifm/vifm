#include <stic.h>

#include <string.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

TEST(cropping_no_ok)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.cropping == CT_NONE);
}

TEST(cropping_none_ok)
{
	int result = do_parse("{name}...");
	assert_true(result == 0);
	assert_true(info.cropping == CT_NONE);
}

TEST(cropping_truncate_ok)
{
	int result = do_parse("{name}.");
	assert_true(result == 0);
	assert_true(info.cropping == CT_TRUNCATE);
}

TEST(cropping_ellipsis_ok)
{
	int result = do_parse("{name}..");
	assert_true(result == 0);
	assert_true(info.cropping == CT_ELLIPSIS);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
