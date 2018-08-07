#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t dummy(const call_info_t *call_info);

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
	ASSERT_FAIL("\"", PE_MISSING_QUOTE);
	ASSERT_FAIL("\" this is a comment", PE_MISSING_QUOTE);
	ASSERT_FAIL("    \"this is a comment", PE_MISSING_QUOTE);
	ASSERT_FAIL("    \"", PE_MISSING_QUOTE);
}

TEST(expression_and_comment)
{
	ASSERT_OK("1\"", "1");
	ASSERT_OK("'str'\" this is a comment", "str");
	ASSERT_OK(" 1 && 0 \"this is a comment", "0");

	ASSERT_FAIL(" +   \"", PE_MISSING_QUOTE);
	ASSERT_FAIL(" 4 || \"", PE_MISSING_QUOTE);
}

TEST(priority_of_operators)
{
	ASSERT_OK("1.'0' + 1 > 10 && 1 + 2 != 0 || +0 == -3 + 2 && 0", "1");
}

TEST(state_is_reset_on_each_parsing)
{
	ASSERT_FAIL("1 1", PE_INVALID_EXPRESSION);
	assert_true(is_prev_token_whitespace());
	ASSERT_FAIL("", PE_INVALID_EXPRESSION);
	assert_false(is_prev_token_whitespace());

	static const function_t function_a = { "a", "adescr", {1,1}, &dummy };
	assert_success(function_register(&function_a));

	ASSERT_FAIL("a('a'", PE_INVALID_EXPRESSION);
	assert_int_equal(VTYPE_ERROR, get_parsing_result().type);

	function_reset_all();
}

static var_t
dummy(const call_info_t *call_info)
{
	return var_from_str("");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
