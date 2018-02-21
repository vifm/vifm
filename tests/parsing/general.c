#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(empty_fail)
{
	ASSERT_FAIL("", PE_INVALID_EXPRESSION);
}

TEST(non_quoted_fail)
{
	ASSERT_FAIL("b", PE_INVALID_EXPRESSION);
}

TEST(double_dot_fail)
{
	ASSERT_FAIL("'a'..'b'", PE_INVALID_EXPRESSION);
}

TEST(starts_with_dot_fail)
{
	ASSERT_FAIL(".'b'", PE_INVALID_EXPRESSION);
}

TEST(ends_with_dot_fail)
{
	ASSERT_FAIL("'a'.", PE_INVALID_EXPRESSION);
}

TEST(fail_position_correct)
{
	ASSERT_FAIL("'b' c", PE_INVALID_EXPRESSION);
	assert_string_equal("'b' c", get_last_position());

	ASSERT_FAIL("a b", PE_INVALID_EXPRESSION);
	assert_string_equal("a b", get_last_position());
}

TEST(spaces_and_fail_position_correct)
{
	ASSERT_FAIL("  'b' c", PE_INVALID_EXPRESSION);
	assert_string_equal("'b' c", get_last_position());

	ASSERT_FAIL("  a b", PE_INVALID_EXPRESSION);
	assert_string_equal("a b", get_last_position());
}

TEST(nothing_but_comment)
{
	ASSERT_FAIL("\"", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("\" this is a comment", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("    \"this is a comment", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("    \"", PE_INVALID_EXPRESSION);
}

TEST(expression_and_comment)
{
	ASSERT_OK("1\"", "1");
	ASSERT_OK("'str'\" this is a comment", "str");
	ASSERT_OK(" 1 && 0 \"this is a comment", "0");

	ASSERT_FAIL(" +   \"", PE_INVALID_EXPRESSION);
	ASSERT_FAIL(" 4 || \"", PE_INVALID_EXPRESSION);
}

TEST(priority_of_operators)
{
	ASSERT_OK("1.'0' + 1 > 10 && 1 + 2 != 0 || +0 == -3 + 2 && 0", "1");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
