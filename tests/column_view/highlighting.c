#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() memset() */

#include "../../src/ui/column_view.h"
#include "../../src/utils/macros.h"

#include "test.h"

enum { MAX_WIDTH = 10 };

static void column_line_print(const char buf[], int offset, AlignType align,
		const format_info_t *info);
static void column_line_match(const char full_column[],
		const format_info_t *info, int *match_from, int *match_to);
static void column_five_as_zs(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column_many_bs(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void perform_test(const column_info_t *column_info,
		const char printed[], const char highlighted[]);

static char print_buffer[MAX_WIDTH + 1];
static char match_buffer[MAX_WIDTH + 1];
static int match_start, match_end;

SETUP()
{
	print_next = &column_line_print;
	match_next = &column_line_match;
	col1_next = &column_five_as_zs;
	col2_next = &column_many_bs;
}

TEARDOWN()
{
	print_next = NULL;
	match_next = NULL;
	col1_next = NULL;
	col2_next = NULL;
}

TEST(match_fits)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 10UL,    .text_width = 10UL,
		.align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE,
	};

	match_start = 0, match_end = 5;
	perform_test(&column_info, "aaaaazzzzz",
	                           "*****     ");

	match_start = 5, match_end = 10;
	perform_test(&column_info, "aaaaazzzzz",
	                           "     *****");

	match_start = 3, match_end = 8;
	perform_test(&column_info, "aaaaazzzzz",
	                           "   *****  ");
}

TEST(match_on_the_right_cut_out)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 8UL,     .text_width = 8UL,
		.align = AT_LEFT,     .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE,
	};

	match_start = 0, match_end = 5;
	perform_test(&column_info, "aaaaazzz  ",
	                           "*****     ");

	match_start = 5, match_end = 10;
	perform_test(&column_info, "aaaaa>>>  ",
	                           "     ***  ");

	match_start = 3, match_end = 8;
	perform_test(&column_info, "aaaaazzz  ",
	                           "   *****  ");
}

TEST(match_on_the_left_cut_out)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 8UL,     .text_width = 8UL,
		.align = AT_RIGHT,    .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE,
	};

	match_start = 0, match_end = 5;
	perform_test(&column_info, "<<<zzzzz  ",
	                           "***       ");

	match_start = 5, match_end = 10;
	perform_test(&column_info, "aaazzzzz  ",
	                           "   *****  ");

	match_start = 3, match_end = 8;
	perform_test(&column_info, "aaazzzzz  ",
	                           " *****    ");
}

TEST(match_in_the_middle_cut_out)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 8UL,     .text_width = 8UL,
		.align = AT_MIDDLE,   .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE,
	};

	match_start = 0, match_end = 5;
	perform_test(&column_info, "aa>..<zz  ",
	                           "******    ");

	match_start = 5, match_end = 10;
	perform_test(&column_info, "aa>..<zz  ",
	                           "  ******  ");

	match_start = 3, match_end = 8;
	perform_test(&column_info, "aa>..<zz  ",
	                           "  ****    ");
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const format_info_t *info)
{
	memcpy(print_buffer + offset, buf, strlen(buf));
	memset(match_buffer + info->match_from, '*',
			info->match_to - info->match_from);
}

static void
column_line_match(const char full_column[], const format_info_t *info,
		int *match_from, int *match_to)
{
	*match_from = match_start;
	*match_to = match_end;
}

static void
column_five_as_zs(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "aaaaazzzzz");
}

static void
column_many_bs(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	snprintf(buf, buf_len + 1, "%s", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
}

static void
perform_test(const column_info_t *column_info, const char printed[],
		const char highlighted[])
{
	columns_t *const cols = columns_create();
	columns_add_column(cols, *column_info);

	memset(print_buffer, ' ', MAX_WIDTH);
	memset(match_buffer, ' ', MAX_WIDTH);
	columns_format_line(cols, NULL, MAX_WIDTH);

	columns_free(cols);

	assert_string_equal(printed, print_buffer);
	assert_string_equal(highlighted, match_buffer);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
