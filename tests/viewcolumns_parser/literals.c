#include <stic.h>

#include <string.h>

#include <test-utils.h>

#include "../../src/ui/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(defaults_are_ok)
{
	assert_success(do_parse("{#literal}"));
	assert_int_equal(FILL_COLUMN_ID, info.column_id);
	assert_int_equal(AT_RIGHT, info.align);
	assert_int_equal(ST_ABSOLUTE, info.sizing);
	assert_int_equal(CT_TRUNCATE, info.cropping);
}

TEST(value_and_widths_are_ok)
{
	assert_success(do_parse("{#}"));
	assert_string_equal("", info.literal);
	assert_int_equal(0, info.full_width);
	assert_int_equal(0, info.text_width);

	assert_success(do_parse("{#val}"));
	assert_string_equal("val", info.literal);
	assert_int_equal(3, info.full_width);
	assert_int_equal(3, info.text_width);
}

TEST(fields_are_applied)
{
	assert_success(do_parse("-3.2{#}..."));
	assert_int_equal(FILL_COLUMN_ID, info.column_id);
	assert_int_equal(AT_LEFT, info.align);
	assert_int_equal(ST_ABSOLUTE, info.sizing);
	assert_int_equal(3, info.full_width);
	assert_int_equal(2, info.text_width);
	assert_int_equal(CT_NONE, info.cropping);

	assert_success(do_parse("*10%{#}..."));
	assert_int_equal(FILL_COLUMN_ID, info.column_id);
	assert_int_equal(AT_DYN, info.align);
	assert_int_equal(ST_PERCENT, info.sizing);
	assert_int_equal(10, info.full_width);
	assert_int_equal(10, info.text_width);
	assert_int_equal(CT_NONE, info.cropping);
}

TEST(unicode, IF(utf8_locale))
{
	assert_success(do_parse("{#абвгд}"));
	assert_string_equal("абвгд", info.literal);
	assert_int_equal(5, info.full_width);
	assert_int_equal(5, info.text_width);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
