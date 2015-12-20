#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(logical_not_of_integers)
{
	ASSERT_INT_OK("!1", 0);
	ASSERT_INT_OK("!0", 1);
	ASSERT_INT_OK("!-10", 0);
}

TEST(logical_not_of_strings)
{
	ASSERT_INT_OK("!'abc'", 1);
	ASSERT_INT_OK("!''", 1);
	ASSERT_INT_OK("!'0'", 1);
	ASSERT_INT_OK("!'1'", 0);
}

TEST(multiple_logical_not)
{
	ASSERT_INT_OK("!!10", 1);
	ASSERT_INT_OK("!!!10", 0);
}

TEST(extra_spaces_in_logical_not)
{
	ASSERT_INT_OK("! 18", 0);
	ASSERT_INT_OK("! \"\"", 1);
	ASSERT_INT_OK("! !! \"\"", 1);
}

TEST(invalid_logical_not_without_argument)
{
	ASSERT_FAIL("!", PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
