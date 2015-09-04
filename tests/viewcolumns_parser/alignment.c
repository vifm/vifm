#include <stic.h>

#include <string.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"

#include "test.h"

TEST(left_alignment_ok)
{
	assert_success(do_parse("-{name}"));
	assert_int_equal(AT_LEFT, info.align);
}

TEST(right_alignment_ok)
{
	assert_success(do_parse("{name}"));
	assert_int_equal(AT_RIGHT, info.align);
}

TEST(dynamic_alignment_ok)
{
	assert_success(do_parse("*{name}"));
	assert_int_equal(AT_DYN, info.align);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
