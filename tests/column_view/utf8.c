#include <stic.h>

#include <locale.h> /* setlocale() */
#include <stddef.h> /* NULL size_t */
#include <string.h> /* memcpy() */

#include "../../src/utils/utf8.h"
#include "../../src/utils/utils.h"
#include "../../src/ui/column_view.h"

#include "test.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		int offset, AlignType align);
static void column1_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static int locale_works(void);

static const size_t MAX_WIDTH = 20;
static char print_buffer[80 + 1];

static const char *col1_str;

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
}

SETUP()
{
	print_next = &column_line_print;
	col1_next = &column1_func;
	col2_next = &column2_func;

	col1_str = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя";
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
	memcpy(print_buffer + utf8_nstrsnlen(print_buffer, offset), buf, strlen(buf));
}

static void
column1_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", col1_str);
}

static void
column2_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "яюэъыьщшчцхфутсрпонмлкйизжёедгв推");
}

static void
perform_test(column_info_t column_infos[], size_t count, size_t max_width)
{
	size_t i;

	columns_t *const cols = columns_create();
	for(i = 0U; i < count; ++i)
	{
		columns_add_column(cols, column_infos[i]);
	}

	memset(print_buffer, '\0', sizeof(print_buffer));
	columns_format_line(cols, NULL, max_width);

	columns_free(cols);
}

TEST(not_truncating_short_utf8_ok, IF(locale_works))
{
	static column_info_t column_infos[1] = {
		{ .column_id = COL1_ID, .full_width = 33UL,    .text_width = 33UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

	perform_test(column_infos, 1, 40);

	assert_string_equal(expected, print_buffer);
}

TEST(donot_add_ellipsis_short_utf8_ok, IF(locale_works))
{
	static column_info_t column_infos[1] = {
		{ .column_id = COL1_ID, .full_width = 33UL,    .text_width = 33UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

	perform_test(column_infos, 1, 40);

	assert_string_equal(expected, print_buffer);
}

TEST(truncating_ok, IF(locale_works))
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE, },
	};
	static const char expected[] = "师从螺丝刀изжёедгв推";

	perform_test(column_infos, 2, MAX_WIDTH);

	assert_string_equal(expected, print_buffer);
}

TEST(none_cropping_allows_for_correct_gaps, IF(locale_works))
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO,     .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 4UL,     .text_width = 4UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO,     .cropping = CT_NONE, },
	};
	static const char expected[] = "师从 яюэъыьщшчцхфутсрпонмлкйизжёедгв推";

	perform_test(column_infos, 2, 38);

	assert_string_equal(expected, print_buffer);
}

TEST(add_ellipsis_ok, IF(locale_works))
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] = "师从螺... ...ёедгв推";

	perform_test(column_infos, 2, MAX_WIDTH);

	assert_string_equal(expected, print_buffer);
}

TEST(wide_ellipsis_work, IF(locale_works))
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
		{ .column_id = COL2_ID, .full_width = 10UL,    .text_width = 10UL,
		  .align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] = "师从螺丝… …зжёедгв推";

	columns_set_ellipsis("…");
	perform_test(column_infos, 2, MAX_WIDTH);

	assert_string_equal(expected, print_buffer);
}

TEST(filling, IF(locale_works))
{
	static column_info_t column_infos[1] = {
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};
	static const char expected[] = "师从螺丝刀йклмнопрстуфхцчшщьыъэюя       ";

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_infos[0]);

	memset(print_buffer, '\0', 80);
	columns_format_line(cols, NULL, 40);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

TEST(right_filling, IF(locale_works))
{
	static column_info_t column_infos[1] = {
		{ .column_id = COL2_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] = ".";

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_infos[0]);

	memset(print_buffer, '\0', 80);
	columns_format_line(cols, NULL, 1);

	columns_free(cols);

	assert_string_equal(expected, print_buffer);
}

TEST(wide_right_ellipsis_ok, IF(locale_works))
{
	/* A gap might appear after removing several characters to insert ellipsis in
	 * their place, make sure that it's filled with spaces to obtain requested
	 * width. */

	static column_info_t column_infos[1] = {
		{ .column_id = COL1_ID, .full_width = 0UL, .text_width = 0UL,
		  .align = AT_RIGHT,    .sizing = ST_AUTO, .cropping = CT_ELLIPSIS, },
	};
	static const char expected[] = " ...螺丝刀师从螺丝刀";

	col1_str = ",师从螺丝刀师从螺丝刀";
	perform_test(column_infos, ARRAY_LEN(column_infos), MAX_WIDTH);

	assert_string_equal(expected, print_buffer);
}

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
