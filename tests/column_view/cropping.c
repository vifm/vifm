#include <string.h>

#include "seatest.h"

#include "test.h"
#include "../../src/column_view.h"

static const size_t MAX_WIDTH = 40;
static char print_buffer[40 + 1];

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	strncpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "aaaaaaaaaaaaaaazzzzzzzzzzzzzzz");
}

static void
column2_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
}

static void
column2_short_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "xxxxx");
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
test_none_align_left(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_ellipsis_align_left(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaa...bbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_truncating_align_left(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_none_align_right(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_none_align_right_overlapping(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaazzzzzzzzzzzzzzz     xxxxx";

	col2_next = column2_short_func;
	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_ellipsis_align_right(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "...zzzzzzzzzzzzbbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_truncating_align_right(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 15UL,    .text_width = 15UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 35UL,    .text_width = 35UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "aaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbb";

	perform_test(column_infos);

	assert_string_equal(expected, print_buffer);
}

static void
test_ellipsis_less_space(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_ELLIPSIS,
	};
	static const char expected[] = "..";

	columns_t cols = columns_create();
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	print_buffer[2] = '\0';
	columns_format_line(cols, NULL, 2);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

void
cropping_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_none_align_left);
	run_test(test_ellipsis_align_left);
	run_test(test_truncating_align_left);

	run_test(test_none_align_right);
	run_test(test_none_align_right_overlapping);
	run_test(test_ellipsis_align_right);
	run_test(test_truncating_align_right);

	run_test(test_ellipsis_less_space);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
