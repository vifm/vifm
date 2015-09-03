#include <stic.h>

#include <string.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"

#include "test.h"

TEST(auto_ok)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.sizing == ST_AUTO);
}

TEST(absolute_without_limitation_ok)
{
	int result = do_parse("10{name}");
	assert_true(result == 0);
	assert_int_equal(10, info.full_width);
	assert_int_equal(10, info.text_width);
	assert_true(info.sizing == ST_ABSOLUTE);
}

TEST(absolute_with_limitation_ok)
{
	int result = do_parse("10.8{name}");
	assert_true(result == 0);
	assert_int_equal(10, info.full_width);
	assert_int_equal(8, info.text_width);
	assert_true(info.sizing == ST_ABSOLUTE);
}

TEST(absolute_with_limitation_overflow_fail)
{
	int result = do_parse("10.11{name}");
	assert_false(result == 0);
}

TEST(percent_ok)
{
	int result = do_parse("50%{name}");
	assert_true(result == 0);
	assert_int_equal(50, info.full_width);
	assert_int_equal(50, info.text_width);
	assert_true(info.sizing == ST_PERCENT);
}

TEST(percent_zero_fail)
{
	int result = do_parse("0%{name}");
	assert_false(result == 0);
}

TEST(percent_greater_than_hundred_fail)
{
	int result = do_parse("110%{name}");
	assert_false(result == 0);
}

TEST(percent_summ_greater_than_hundred_fail)
{
	int result = do_parse("50%{name},50%{name},50%{name}");
	assert_false(result == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
