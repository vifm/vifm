#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 80;

static size_t print_offset;
static char print_buffer[800 + 1];

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	print_offset = offset;
	strncpy(print_buffer + offset, buf, strlen(buf));
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s",
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

static void
column1_func2(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "abcdefg");
}

static void
setup(void)
{
	print_next = column_line_print;
	col1_next = column1_func;
}

static void
teardown(void)
{
	print_next = NULL;
	col1_next = NULL;
}

static void
perform_test(column_info_t column_info)
{
	columns_t cols = columns_create();
	columns_add_column(cols, column_info);

	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);
}

static void
test_right_align(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 30UL,    .text_width = 30UL,
		.align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_NONE,
	};

	perform_test(column_info);

	assert_int_equal(0, print_offset);
}

static void
test_left_align(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 80UL,    .text_width = 80UL,
		.align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_NONE,
	};

	perform_test(column_info);

	assert_int_equal(0, print_offset);
}

static void
test_very_long_line_right_align(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_NONE,
	};
	static const char expected[] =
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaa";

	perform_test(column_info);

	assert_string_equal(expected, print_buffer);
}

static void
test_truncation_on_right_align(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		.align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_TRUNCATE,
	};
	static const char expected[] = "g";

	columns_t cols = columns_create();
	col1_next = column1_func2;
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	print_buffer[1] = '\0';
	columns_format_line(cols, NULL, 1);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

void
align_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_right_align);
	run_test(test_left_align);
	run_test(test_very_long_line_right_align);
	run_test(test_truncation_on_right_align);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
