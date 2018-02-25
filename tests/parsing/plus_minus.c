#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(sum_of_two_elements)
{
	ASSERT_INT_OK("1 + 1", 2);

	ASSERT_INT_OK("-1 + 4", 3);
	ASSERT_INT_OK("- 1 + 4", 3);

	ASSERT_INT_OK("1 + -2", -1);
	ASSERT_INT_OK("1 +- 2", -1);
	ASSERT_INT_OK("1 + - 2", -1);
}

TEST(difference_of_two_elements)
{
	ASSERT_INT_OK("1 - 1", 0);

	ASSERT_INT_OK("-1 - 4", -5);
	ASSERT_INT_OK("- 1 - 4", -5);

	ASSERT_INT_OK("1 - -2", 3);
	ASSERT_INT_OK("1 -- 2", 3);
	ASSERT_INT_OK("1 - - 2", 3);
}

TEST(more_than_two_elements)
{
	ASSERT_INT_OK("1 + 1 + 1", 3);
	ASSERT_INT_OK("1 + 2 + 3 + 4", 10);

	ASSERT_INT_OK("1 - 1 - 1", -1);
	ASSERT_INT_OK("1 - 2 - 3 - 4", -8);
}

TEST(incomplete_expressions)
{
	ASSERT_FAIL("1 +", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("1+", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("0 -", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("0-", PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
