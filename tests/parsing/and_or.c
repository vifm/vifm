#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(front_back_op_fail)
{
	ASSERT_FAIL("&& 'a' == 'b'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("|| 'a' == 'b'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' == 'a' &&", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' == 'a' ||", PE_INVALID_EXPRESSION);
}

TEST(spaces_before_after_op_ok)
{
	ASSERT_OK("'a' == 'a'&& 'a' == 'b'", "0");
	ASSERT_OK("'a' == 'a'|| 'a' == 'b'", "1");
	ASSERT_OK("'a' == 'a' &&'a' == 'b'", "0");
	ASSERT_OK("'a' == 'a' ||'a' == 'b'", "1");
	ASSERT_OK("'a' == 'a'&&'a' == 'b'", "0");
	ASSERT_OK("'a' == 'a'||'a' == 'b'", "1");
}

TEST(spaces_in_op_fail)
{
	ASSERT_FAIL("'a' == 'a' & & 'a' == 'b'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' == 'a' | | 'a' == 'b'", PE_INVALID_EXPRESSION);
}

TEST(wrong_op_fail)
{
	ASSERT_FAIL("'a' == 'a' &| 'a' == 'b'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' == 'a' |& 'a' == 'b'", PE_INVALID_EXPRESSION);
}

TEST(constant_operands_ok)
{
	ASSERT_OK("'a' == 'a'&& 1", "1");
	ASSERT_OK("0000000000|| 'a' == 'b'", "0");
	ASSERT_OK("0||0||0", "0");
	ASSERT_OK("1&&0&&1", "0");
}

TEST(and_operates_ok)
{
	ASSERT_OK("0 && 0", "0");
	ASSERT_OK("0 && 1", "0");
	ASSERT_OK("1 && 0", "0");
	ASSERT_OK("1 && 1", "1");
}

TEST(or_operates_ok)
{
	ASSERT_OK("0 || 0", "0");
	ASSERT_OK("0 || 1", "1");
	ASSERT_OK("1 || 0", "1");
	ASSERT_OK("1 || 1", "1");
}

TEST(priority_of_and_is_higher)
{
	ASSERT_OK("1 || 1 && 0", "1");
}

TEST(and_or_ignored_inside_strings)
{
	ASSERT_OK("'||&&'", "||&&");
	ASSERT_OK("\"||&&\"", "||&&");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
