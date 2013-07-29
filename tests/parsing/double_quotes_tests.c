#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_empty_ok(void)
{
	ASSERT_OK("\"\"", "");
}

static void
test_simple_ok(void)
{
	ASSERT_OK("\"test\"", "test");
}

static void
test_not_closed_error(void)
{
	ASSERT_FAIL("\"test", PE_MISSING_QUOTE);
}

static void
test_concatenation(void)
{
	ASSERT_OK("\"NV\".\"AR\"", "NVAR");
	ASSERT_OK("\"NV\" .\"AR\"", "NVAR");
	ASSERT_OK("\"NV\". \"AR\"", "NVAR");
	ASSERT_OK("\"NV\" . \"AR\"", "NVAR");
}

static void
test_double_quote_escaping_ok(void)
{
	ASSERT_OK("\"\\\"\"", "\"");
}

static void
test_special_chars_ok(void)
{
	ASSERT_OK("\"\\t\"", "\t");
}

static void
test_spaces_ok(void)
{
	ASSERT_OK("\" s y \"", " s y ");
}

static void
test_dot_ok(void)
{
	ASSERT_OK("\"a . c\"", "a . c");
}

void
double_quoted_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_empty_ok);
	run_test(test_simple_ok);
	run_test(test_not_closed_error);
	run_test(test_concatenation);
	run_test(test_double_quote_escaping_ok);
	run_test(test_special_chars_ok);
	run_test(test_spaces_ok);
	run_test(test_dot_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
