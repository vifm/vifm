#include <stic.h>

#include <string.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"

#include "test.h"

TEST(simple_ok)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
}

TEST(simple_no_braces_fails)
{
	int result = do_parse("name");
	assert_false(result == 0);
}

TEST(simple_wrong_name_fails)
{
	int result = do_parse("{notname}");
	assert_false(result == 0);
}

TEST(alignment_ok)
{
	int result = do_parse("-{name}");
	assert_true(result == 0);
}

TEST(alignment_fails)
{
	int result = do_parse("+{name}");
	assert_false(result == 0);
}

TEST(sizes_ok)
{
	int result = do_parse("20{name}.");
	assert_true(result == 0);
	result = do_parse("-10{name}");
	assert_true(result == 0);
	result = do_parse("10.2{name}");
	assert_true(result == 0);
	result = do_parse("10%{name}");
	assert_true(result == 0);
}

TEST(sizes_fails)
{
	int result = do_parse("+20{name}.");
	assert_false(result == 0);
	result = do_parse("0{name}.");
	assert_false(result == 0);
	result = do_parse("10.{name}.");
	assert_false(result == 0);
	result = do_parse(".1{name}.");
	assert_false(result == 0);
	result = do_parse("%{name}.");
	assert_false(result == 0);
}

TEST(cropping_ok)
{
	int result = do_parse("{name}.");
	assert_true(result == 0);
	result = do_parse("{name}..");
	assert_true(result == 0);
	result = do_parse("{name}...");
	assert_true(result == 0);
}

TEST(cropping_fails)
{
	int result = do_parse("{name}....");
	assert_false(result == 0);
	result = do_parse("{name}:");
	assert_false(result == 0);
}

TEST(literals_ok)
{
	assert_success(do_parse("{#}"));
	assert_success(do_parse("{#bla}"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
