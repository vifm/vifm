#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"

#include "asserts.h"

static const char *
ge(const char *name)
{
	return name + 1;
}

static void
setup(void)
{
	init_parser(ge);
}

static void
test_simple_ok(void)
{
	ASSERT_OK("$ENV", "NV");
}

static void
test_leading_spaces_ok(void)
{
	ASSERT_OK(" $ENV", "NV");
}

static void
test_trailing_spaces_ok(void)
{
	ASSERT_OK("$ENV ", "NV");
}

static void
test_space_in_the_middle_fail(void)
{
	ASSERT_FAIL("$ENV $VAR", PE_INVALID_EXPRESSION);
}

static void
test_concatenation(void)
{
	ASSERT_OK("$ENV.$VAR", "NVAR");
	ASSERT_OK("$ENV .$VAR", "NVAR");
	ASSERT_OK("$ENV. $VAR", "NVAR");
	ASSERT_OK("$ENV . $VAR", "NVAR");
}

void
envvar_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_simple_ok);
	run_test(test_leading_spaces_ok);
	run_test(test_trailing_spaces_ok);
	run_test(test_space_in_the_middle_fail);
	run_test(test_concatenation);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
