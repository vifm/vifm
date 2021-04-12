#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() */

#include "../../src/ui/column_view.h"
#include "../../src/utils/macros.h"

#include "test.h"

static const size_t MAX_WIDTH = 40;
static char print_buffer[40 + 1];

static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align);
static void column1_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_short_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

SETUP()
{
	print_next = &column_line_print;
	col1_next = &column1_func;
	col2_next = &column2_func;
}

TEARDOWN()
{
	print_next = NULL;
	col1_next = NULL;
	col2_next = NULL;
}

static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align)
{
	memcpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "aaaaaaaaaaaaaaazzzzzzzzzzzzzzz");
}

static void
column2_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
}

static void
column2_short_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "xxxxx");
}

static void
perform_test(column_info_t column_infos[], int n)
{
	int i;
	columns_t *const cols = columns_create();

	for(i = 0; i < n; ++i)
	{
		columns_add_column(cols, column_infos[i]);
	}

	memset(print_buffer, ' ', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);
}

TEST(none_align_left)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(ellipsis_align_left)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaa...bbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(truncating_align_left)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(none_align_right)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(none_align_right_overlapping)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaazzzzzzzzzzzzzzz     xxxxx";

	col2_next = column2_short_func;
	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(no_overlapping)
{
	static column_info_t column_infos[3] = {
		{ .column_id = COL2_ID, .full_width = 0UL,     .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO,     .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 8UL,     .text_width = 8UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 4UL,     .text_width = 4UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
	};
	static const char expected[] = "xxxxx                          xxxxxxxxx";

	col2_next = column2_short_func;
	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(ellipsis_align_right)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "...zzzzzzzzzzzzbbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(truncating_align_right)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos, ARRAY_LEN(column_infos));

	assert_string_equal(expected, print_buffer);
}

TEST(ellipsis_less_space)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_ELLIPSIS,
	};
	static const char expected[] = "..";

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	print_buffer[2] = '\0';
	columns_format_line(cols, NULL, 2);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
