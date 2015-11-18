#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(eq_compare_true)
{
	ASSERT_OK("'a'=='a'", "1");
	ASSERT_OK("1==1", "1");
}

TEST(eq_compare_false)
{
	ASSERT_OK("'a'=='b'", "0");
	ASSERT_OK("1==-1", "0");
}

TEST(ne_compare_true)
{
	ASSERT_OK("'a'!='b'", "1");
	ASSERT_OK("-1!=1", "1");
}

TEST(ne_compare_false)
{
	ASSERT_OK("'a'!='a'", "0");
	ASSERT_OK("0!=0", "0");
}

TEST(lt_compare_true)
{
	ASSERT_OK("1<2", "1");
	ASSERT_OK("'a'<'b'", "1");
}

TEST(lt_compare_false)
{
	ASSERT_OK("2<1", "0");
	ASSERT_OK("' '<'a'", "1");
}

TEST(le_compare_true)
{
	ASSERT_OK("1<=1", "1");
	ASSERT_OK("-1<=1", "1");
	ASSERT_OK("'a'<='b'", "1");
	ASSERT_OK("'b'<='b'", "1");
}

TEST(le_compare_false)
{
	ASSERT_OK("3<=2", "0");
	ASSERT_OK("'c'<='b'", "0");
}

TEST(ge_compare_true)
{
	ASSERT_OK("1>=1", "1");
	ASSERT_OK("2>=1", "1");
	ASSERT_OK("'b'>='a'", "1");
	ASSERT_OK("'b'>='b'", "1");
}

TEST(ge_compare_false)
{
	ASSERT_OK("0>=1", "0");
	ASSERT_OK("'b'>='c'", "0");
}

TEST(gt_compare_true)
{
	ASSERT_OK("2>-1", "1");
	ASSERT_OK("'b'>'a'", "1");
}

TEST(gt_compare_false)
{
	ASSERT_OK("1>1", "0");
	ASSERT_OK("'b'>'b'", "0");
}

TEST(compares_prefer_numbers)
{
	ASSERT_OK("'2'>'b'", "0");
	ASSERT_OK("2>'b'", "1");
}

TEST(compares_convert_string_to_numbers)
{
	ASSERT_OK("2>'b'", "1");
	ASSERT_OK("2>'9b'", "0");
}

TEST(leading_spaces_ok)
{
	ASSERT_OK("     'a'!='a'", "0");
}

TEST(trailing_spaces_ok)
{
	ASSERT_OK("'a'!='a'       ", "0");
}

TEST(spaces_before_op_ok)
{
	ASSERT_OK("'a'      !='a'", "0");
}

TEST(spaces_after_op_ok)
{
	ASSERT_OK("'a'!=         'a'", "0");
}

TEST(spaces_in_op_fail)
{
	ASSERT_FAIL("'a' ! = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' = = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' > = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' < = 'a'", PE_INVALID_EXPRESSION);
}

TEST(wrong_op_fail)
{
	ASSERT_FAIL("'a' =! 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' => 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' =< 'a'", PE_INVALID_EXPRESSION);
}

TEST(ops_ignored_inside_strings)
{
	ASSERT_OK("'!= == <= >= !'", "!= == <= >= !");
	ASSERT_OK("\"!= == <= >= !\"", "!= == <= >= !");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
