#include <stic.h>

#include <stddef.h> /* NULL size_t */

#include "../../src/ui/column_view.h"

#include "test.h"

static void print_not_less_than_zero(const void *data, int column_id,
		const char buf[], size_t offset, AlignType align);
static void column12_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

static const size_t MAX_WIDTH = 80;

static columns_t *columns;

SETUP()
{
	static column_info_t column_infos[2] = {
		{ .column_id = COL1_ID, .full_width = 0,   .text_width = 0,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0,   .text_width = 0,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};

	col1_next = &column12_func;
	col2_next = &column12_func;
	print_next = &print_not_less_than_zero;

	columns = columns_create();
	columns_add_column(columns, column_infos[0]);
	columns_add_column(columns, column_infos[1]);
}

TEARDOWN()
{
	print_next = NULL;
	col1_next = NULL;
	col2_next = NULL;

	columns_free(columns);
}

static void
print_not_less_than_zero(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align)
{
	assert_true(offset <= MAX_WIDTH);
}

static void
column12_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	buf[0] = '\0';
}

TEST(cant_add_columns_with_same_id)
{
	assert_false(columns_add_column_desc(COL1_ID, NULL, NULL) == 0);
	assert_false(columns_add_column_desc(COL2_ID, NULL, NULL) == 0);
}

TEST(not_out_of_max_width)
{
	columns_format_line(columns, NULL, MAX_WIDTH);
}

TEST(free_null_columns_ok)
{
	columns_free(NULL);
}

TEST(add_duplicate_columns_ok)
{
	static column_info_t column_info = {
		.column_id = COL1_ID, .full_width = 0,   .text_width = 0,
		.align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE,
	};

	columns_add_column(columns, column_info);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
