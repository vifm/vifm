#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t dummy(const call_info_t *call_info);

static int called;

SETUP_ONCE()
{
	static const function_t function_a = { "a", 0, &dummy };

	assert_int_equal(0, function_register(&function_a));
}

TEARDOWN_ONCE()
{
	function_reset_all();
}

TEARDOWN()
{
	called = 0;
}

static var_t
dummy(const call_info_t *call_info)
{
	static const var_val_t var_val = { .string = "" };
	called = 1;
	return var_new(VTYPE_STRING, var_val);
}

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

TEST(strings_are_converted_to_integers)
{
	ASSERT_OK("'a' && 'b' && 'c'", "0");
}

TEST(or_is_lazy)
{
	ASSERT_OK("1 || a()", "1");
	assert_false(called);
}

TEST(and_is_lazy)
{
	ASSERT_OK("0 && a()", "0");
	assert_false(called);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
