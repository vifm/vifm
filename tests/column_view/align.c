#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() */

#include "../../src/ui/column_view.h"
#include "test.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align);
static void column1_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

static const size_t MAX_WIDTH = 80;

static size_t print_offset;
static char print_buffer[800 + 1];
static AlignType last_align;

SETUP()
{
	print_next = &column_line_print;
	col1_next = &column1_func;
}

TEARDOWN()
{
	print_next = NULL;
	col1_next = NULL;
}

static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align)
{
	if(column_id == COL1_ID)
	{
		last_align = align;
	}
	print_offset = offset;
	memcpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s",
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

static void
column2_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "bxx");
}

static void
column1_func2(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "abcdefg");
}

static void
perform_test(column_info_t column_info)
{
	columns_t *const cols = columns_create();
	columns_add_column(cols, column_info);

	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);
}

TEST(right_align)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 30UL,    .text_width = 30UL,
		.align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE,
	};

	perform_test(column_info);

	assert_int_equal(0, print_offset);
}

TEST(left_align)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 80UL,    .text_width = 80UL,
		.align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE,
	};

	perform_test(column_info);

	assert_int_equal(0, print_offset);
}

TEST(very_long_line_right_align)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE,
	};
	static const char expected[] =
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaa";

	perform_test(column_info);

	assert_string_equal(expected, print_buffer);
}

TEST(truncation_on_right_align)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_TRUNCATE,
	};
	static const char expected[] = "g";

	columns_t *const cols = columns_create();
	col1_next = column1_func2;
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 1);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

TEST(dyn_align)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_DYN,      .sizing = ST_AUTO, .cropping = CT_TRUNCATE,
	};

	columns_t *const cols = columns_create();
	col1_next = column1_func2;
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 8);
	assert_string_equal("abcdefg ", print_buffer);
	assert_int_equal(AT_LEFT, last_align);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 7);
	assert_string_equal("abcdefg", print_buffer);
	assert_int_equal(AT_LEFT, last_align);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 6);
	assert_string_equal("bcdefg", print_buffer);
	assert_int_equal(AT_RIGHT, last_align);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 4);
	assert_string_equal("defg", print_buffer);
	assert_int_equal(AT_RIGHT, last_align);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 2);
	assert_string_equal("fg", print_buffer);
	assert_int_equal(AT_RIGHT, last_align);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 1);
	assert_string_equal("g", print_buffer);
	assert_int_equal(AT_RIGHT, last_align);

	columns_free(cols);
}

TEST(dyn_align_on_the_right)
{
	static column_info_t column_info1 = {
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_TRUNCATE,
	};

	static column_info_t column_info2 = {
		.column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_DYN,      .sizing = ST_AUTO, .cropping = CT_TRUNCATE,
	};

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_info1);
	columns_add_column(cols, column_info2);

	col2_next = &column1_func2;

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 8);
	assert_string_equal("aaaadefg", print_buffer);

	col1_next = &column1_func2;
	col2_next = &column2_func;

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 16);
	assert_string_equal(" abcdefgbxx     ", print_buffer);

	col2_next = NULL;

	columns_free(cols);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
