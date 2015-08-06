#include <stic.h>

#include <string.h>

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"

#include "test.h"

TEST(left_alignment_ok)
{
	int result = do_parse("-{name}");
	assert_true(result == 0);
	assert_true(info.align == AT_LEFT);
}

TEST(right_alignment_ok)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.align == AT_RIGHT);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
