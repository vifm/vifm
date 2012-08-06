#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

static void
test_simple_ok(void)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
}

static void
test_simple_no_braces_fails(void)
{
	int result = do_parse("name");
	assert_false(result == 0);
}

static void
test_simple_wrong_name_fails(void)
{
	int result = do_parse("{notname}");
	assert_false(result == 0);
}

static void
test_alignment_ok(void)
{
	int result = do_parse("-{name}");
	assert_true(result == 0);
}

static void
test_alignment_fails(void)
{
	int result = do_parse("+{name}");
	assert_false(result == 0);
}

static void
test_sizes_ok(void)
{
	int result = do_parse("20{name}.");
	assert_true(result == 0);
	result = do_parse("-10{name}");
	assert_true(result == 0);
	result = do_parse("10.2{name}");
	assert_true(result == 0);
	result = do_parse("10%{name}");
	assert_true(result == 0);
}

static void
test_sizes_fails(void)
{
	int result = do_parse("+20{name}.");
	assert_false(result == 0);
	result = do_parse("0{name}.");
	assert_false(result == 0);
	result = do_parse("10.{name}.");
	assert_false(result == 0);
	result = do_parse(".1{name}.");
	assert_false(result == 0);
	result = do_parse("%{name}.");
	assert_false(result == 0);
}

static void
test_cropping_ok(void)
{
	int result = do_parse("{name}.");
	assert_true(result == 0);
	result = do_parse("{name}..");
	assert_true(result == 0);
	result = do_parse("{name}...");
	assert_true(result == 0);
}

static void
test_cropping_fails(void)
{
	int result = do_parse("{name}....");
	assert_false(result == 0);
	result = do_parse("{name}:");
	assert_false(result == 0);
}

void
syntax_tests(void)
{
	test_fixture_start();

	run_test(test_simple_ok);
	run_test(test_simple_no_braces_fails);
	run_test(test_simple_wrong_name_fails);
	run_test(test_alignment_ok);
	run_test(test_alignment_fails);
	run_test(test_sizes_ok);
	run_test(test_sizes_fails);
	run_test(test_cropping_ok);
	run_test(test_cropping_fails);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
