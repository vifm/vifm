#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

static void
test_cropping_no_ok(void)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.cropping == CT_NONE);
}

static void
test_cropping_none_ok(void)
{
	int result = do_parse("{name}...");
	assert_true(result == 0);
	assert_true(info.cropping == CT_NONE);
}

static void
test_cropping_truncate_ok(void)
{
	int result = do_parse("{name}.");
	assert_true(result == 0);
	assert_true(info.cropping == CT_TRUNCATE);
}

static void
test_cropping_ellipsis_ok(void)
{
	int result = do_parse("{name}..");
	assert_true(result == 0);
	assert_true(info.cropping == CT_ELLIPSIS);
}

void
cropping_tests(void)
{
	test_fixture_start();

	run_test(test_cropping_no_ok);
	run_test(test_cropping_none_ok);
	run_test(test_cropping_truncate_ok);
	run_test(test_cropping_ellipsis_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
