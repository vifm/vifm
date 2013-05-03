/* UTF-8 isn't used on Windows yet. */
#ifndef _WIN32

#include <locale.h> /* setlocale() */
#include <string.h>

#include "seatest.h"

#include "../../src/utils/utf8.h"
#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 20;
static char print_buffer[80 + 1];

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	strncpy(print_buffer + get_normal_utf8_string_widthn(print_buffer, offset),
			buf, strlen(buf));
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "师从螺丝刀йклмнопрстуфхцчшщьыъэюя");
}

static void
column2_func(int id, const void *data, size_t buf_len, char *buf)
{
	snprintf(buf, buf_len + 1, "%s", "яюэъыьщшчцхфутсрпонмлкйизжёедгв推");
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

	memset(print_buffer, '\0', sizeof(print_buffer));
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
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

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
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

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
	static const char expected[] = "师从螺丝刀изжёедгв推";

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
	static const char expected[] = "师从螺... ...ёедгв推";

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
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

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

	(void)setlocale(LC_ALL, "");
	if(wcwidth(L'丝') != 2)
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}

	fixture_setup(setup);
	fixture_teardown(teardown);

	if(wcwidth(L'丝') == 2)
	{
		run_test(test_not_truncating_short_utf8_ok);
		run_test(test_donot_add_ellipsis_short_utf8_ok);
		run_test(test_truncating_ok);
		run_test(test_add_ellipsis_ok);
		run_test(test_filling);
	}

	test_fixture_end();
}

#else

void
utf8_tests(void)
{
	/* UTF-8 isn't used on Windows yet. */
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
