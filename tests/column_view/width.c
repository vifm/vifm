#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() */

#include "../../src/ui/column_view.h"
#include "test.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		int offset, AlignType align);
static void column1_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

static const size_t MAX_WIDTH = 20;
static char print_buffer[20 + 1];

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
column_line_print(const void *data, int column_id, const char buf[], int offset,
		AlignType align)
{
	memcpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "aaaaayyyyyzzzzz");
}

static void
column2_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "bbbbbcccccdddddeeeee");
}

static void
perform_test(column_info_t column_infos[2])
{
	columns_t *const cols = columns_create();
	columns_add_column(cols, column_infos[0]);
	columns_add_column(cols, column_infos[1]);

	memset(print_buffer, ' ', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);
}

TEST(absolute_same_width)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaayyyyydddddeeeee";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

TEST(absolute_smaller_width)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 8UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 5UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaayyy       eeeee";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

TEST(last_percent_column_gets_unused_space)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 25UL,   .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_PERCENT, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 50UL,   .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_PERCENT, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaabbbbbcccccddddd";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

TEST(auto_sizing)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyybbbbbccccc";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

TEST(no_space_for_auto_left_ok)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 20UL,    .text_width = 20UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL,     .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO,     .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyyzzzzz     ";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

TEST(even_width)
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "bbbbaaaaayyyyyzzzzz ";

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_infos[0]);
	columns_add_column(cols, column_infos[1]);

	memset(print_buffer, ' ', MAX_WIDTH - 1);
	columns_format_line(cols, NULL, MAX_WIDTH - 1);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

TEST(filling)
{
	static column_info_t column_infos[1] = {
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyyzzzzz     ";

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_infos[0]);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
