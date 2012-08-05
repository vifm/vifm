#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

static void
test_left_alignment_ok(void)
{
	int result = do_parse("-{name}");
	assert_true(result == 0);
	assert_true(info.align == AT_LEFT);
}

static void
test_right_alignment_ok(void)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.align == AT_RIGHT);
}

void
alignment_tests(void)
{
	test_fixture_start();

	run_test(test_right_alignment_ok);
	run_test(test_right_alignment_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
