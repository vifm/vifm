#include <stic.h>

#include "../../src/engine/parsing.h"
#include "../../src/engine/variables.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(wrong_paren_syntax)
{
	ASSERT_FAIL("(", PE_MISSING_PAREN);
	ASSERT_FAIL(")", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("(0", PE_MISSING_PAREN);
	ASSERT_FAIL("0)", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("()", PE_INVALID_EXPRESSION);
}

TEST(whitespace_in_parens)
{
	ASSERT_OK("(1)", "1");
	ASSERT_OK("( 1)", "1");
	ASSERT_OK("(1 )", "1");
	ASSERT_OK("( 1 )", "1");
	ASSERT_OK(" (1)", "1");
	ASSERT_OK("(1) ", "1");
	ASSERT_OK(" (1) ", "1");
}

TEST(nesting)
{
	ASSERT_OK("('a')", "a");
	ASSERT_OK("(('a'))", "a");
	ASSERT_OK("((('a')))", "a");
}

TEST(grouping_works)
{
	ASSERT_OK("(1 + 2 + 3)", "6");
	ASSERT_OK("(1 + 2) + 3", "6");
	ASSERT_OK("1 + (2 + 3)", "6");
	ASSERT_OK("1 + (2) + 3", "6");

	ASSERT_OK("(1 - 2 - 3)", "-4");
	ASSERT_OK("(1 - 2) - 3", "-4");
	ASSERT_OK("1 - (2 - 3)", "2");
	ASSERT_OK("1 - (2) - 3", "-4");

	ASSERT_OK("-(2)", "-2");
	ASSERT_OK("-(-2)", "2");

	ASSERT_OK("(1 + 2).'2'", "32");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
