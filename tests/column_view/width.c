#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 20;
static char print_buffer[20 + 1];

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	strncpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "aaaaayyyyyzzzzz");
}

static void
column2_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "bbbbbcccccdddddeeeee");
}

static void
setup(void)
{
	print_next = column_line_print;
	col1_next = column1_func;
	col2_next = column2_func;
}

static void
teardown(void)
{
	print_next = NULL;
	col1_next = NULL;
	col2_next = NULL;
}

static void
perform_test(column_info_t column_infos[2])
{
	columns_t cols = columns_create();
	columns_add_column(cols, column_infos[0]);
	columns_add_column(cols, column_infos[1]);

	memset(print_buffer, ' ', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);
}

static void
test_absolute_same_width(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaayyyyydddddeeeee";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_absolute_smaller_width(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 8UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 5UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaayyy       eeeee";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_percent(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 25UL,   .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_PERCENT, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 50UL,   .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_PERCENT, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaabbbbbccccc     ";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_auto(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyybbbbbccccc";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_no_space_for_auto_left_ok(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 20UL,    .text_width = 20UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL,     .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO,     .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyyzzzzz     ";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_even_width(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "bbbbaaaaayyyyyzzzzz ";

	columns_t cols = columns_create();
	columns_add_column(cols, column_infos[0]);
	columns_add_column(cols, column_infos[1]);

	memset(print_buffer, ' ', MAX_WIDTH - 1);
	columns_format_line(cols, NULL, MAX_WIDTH - 1);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

static void
test_filling(void)
{
	static column_info_t column_infos[1] =
	{
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaayyyyyzzzzz     ";

	columns_t cols = columns_create();
	columns_add_column(cols, column_infos[0]);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

void
width_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_absolute_same_width);
	run_test(test_absolute_smaller_width);
	run_test(test_percent);
	run_test(test_auto);
	run_test(test_no_space_for_auto_left_ok);
	run_test(test_even_width);
	run_test(test_filling);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
