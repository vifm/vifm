#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"

#include "asserts.h"

static const char * getenv_value(const char name[]);

SETUP()
{
	init_parser(&getenv_value);
}

static const char *
getenv_value(const char *name)
{
	return name + 1;
}

TEST(simple_ok)
{
	ASSERT_OK("$ENV", "NV");
}

TEST(leading_spaces_ok)
{
	ASSERT_OK(" $ENV", "NV");
}

TEST(trailing_spaces_ok)
{
	ASSERT_OK("$ENV ", "NV");
}

TEST(space_in_the_middle_fail)
{
	ASSERT_FAIL("$ENV $VAR", PE_INVALID_EXPRESSION);
}

TEST(concatenation)
{
	ASSERT_OK("$ENV.$VAR", "NVAR");
	ASSERT_OK("$ENV .$VAR", "NVAR");
	ASSERT_OK("$ENV. $VAR", "NVAR");
	ASSERT_OK("$ENV . $VAR", "NVAR");
}

TEST(non_first_digit_in_name_ok)
{
	ASSERT_OK("$VAR_NUMBER_1", "AR_NUMBER_1");
}

TEST(first_digit_in_name_fail)
{
	ASSERT_FAIL("$1_VAR", PE_INVALID_EXPRESSION);
}

TEST(invalid_first_char_in_name_fail)
{
	ASSERT_FAIL("$#", PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
