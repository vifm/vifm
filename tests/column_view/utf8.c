#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 20;
static char print_buffer[80 + 1];

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	strncpy(print_buffer + offset*2, buf, strlen(buf));
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "абвгдеёжзийклмнопрстуфхцчшщьыъэюя");
}

static void
column2_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "яюэъыьщшчцхфутсрпонмлкйизжёедгвба");
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
perform_test(column_info_t column_infos[], size_t count, size_t max_width)
{
	size_t i;

	columns_t cols = columns_create();
	for(i = 0; i < count; i++)
	{
		columns_add_column(cols, column_infos[i]);
	}

	memset(print_buffer, ' ', 80);
	print_buffer[40] = '\0';
	columns_format_line(cols, NULL, max_width);

	columns_free(cols);
}

static void
test_not_truncating_short_utf8_ok(void)
{
	static column_info_t column_infos[1] =
	{
		{ .column_id = COL1_ID, .full_width = 33UL,    .text_width = 33UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] =
			"абвгдеёжзийклмнопрстуфхцчшщьыъэюя              ";

	perform_test(column_infos, 1, 40);

	assert_string_equal(expected, print_buffer);
}

static void
test_donot_add_ellipsis_short_utf8_ok(void)
{
	static column_info_t column_infos[1] =
	{
		{ .column_id = COL1_ID, .full_width = 33UL,    .text_width = 33UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] =
			"абвгдеёжзийклмнопрстуфхцчшщьыъэюя              ";

	perform_test(column_infos, 1, 40);

	assert_string_equal(expected, print_buffer);
}

static void
test_truncating_ok(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "абвгдеёжзиизжёедгвба";

	perform_test(column_infos, 2, MAX_WIDTH);

	assert_string_equal(expected, print_buffer);
}

static void
test_add_ellipsis_ok(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
	};
	/* The spaces in the middle are because of special print function. */
	static const char expected[] = "абвгдеё...   ...ёедгвба   ";

	perform_test(column_infos, 2, MAX_WIDTH);

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
	static const char expected[] = "абвгдеёжзийклмнопрстуфхцчшщьыъэюя       ";

	columns_t cols = columns_create();
	columns_add_column(cols, column_infos[0]);

	memset(print_buffer, '\0', 80);
	columns_format_line(cols, NULL, 40);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

void
utf8_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_not_truncating_short_utf8_ok);
	run_test(test_donot_add_ellipsis_short_utf8_ok);
	run_test(test_truncating_ok);
	run_test(test_add_ellipsis_ok);
	run_test(test_filling);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
