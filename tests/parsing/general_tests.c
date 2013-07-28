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
test_empty_fail(void)
{
	ASSERT_FAIL("", PE_INVALID_EXPRESSION);
}

static void
test_non_quoted_fail(void)
{
	ASSERT_FAIL("b", PE_INVALID_EXPRESSION);
}

static void
test_double_dot_fail(void)
{
	ASSERT_FAIL("'a'..'b'", PE_INVALID_EXPRESSION);
}

static void
test_starts_with_dot_fail(void)
{
	ASSERT_FAIL(".'b'", PE_INVALID_EXPRESSION);
}

static void
test_ends_with_dot_fail(void)
{
	ASSERT_FAIL("'a'.", PE_INVALID_EXPRESSION);
}

static void
test_fail_position_correct(void)
{
	ASSERT_FAIL("'b' c", PE_INVALID_EXPRESSION)
	assert_string_equal("'b' c", get_last_position());

	ASSERT_FAIL("a b", PE_INVALID_EXPRESSION)
	assert_string_equal("a b", get_last_position());
}

static void
test_spaces_and_fail_position_correct(void)
{
	ASSERT_FAIL("  'b' c", PE_INVALID_EXPRESSION)
	assert_string_equal("'b' c", get_last_position());

	ASSERT_FAIL("  a b", PE_INVALID_EXPRESSION)
	assert_string_equal("a b", get_last_position());
}

void
general_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_empty_fail);
	run_test(test_non_quoted_fail);
	run_test(test_double_dot_fail);
	run_test(test_starts_with_dot_fail);
	run_test(test_ends_with_dot_fail);
	run_test(test_fail_position_correct);
	run_test(test_spaces_and_fail_position_correct);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
