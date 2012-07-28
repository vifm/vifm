#include "seatest.h"

#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 80;

static columns_t columns;

static void
print_not_less_than_zero(const void *data, int column_id, const char *buf,
		size_t offset)
{
	assert_true(offset <= MAX_WIDTH);
}

static void
column12_func(int id, const void *data, size_t buf_len, char *buf)
{
}

static void
setup(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 0,   .text_width = 0,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 0,   .text_width = 0,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};

	col1_next = column12_func;
	col2_next = column12_func;
	print_next = print_not_less_than_zero;

	columns = columns_create();
	columns_add_column(columns, column_infos[0]);
	columns_add_column(columns, column_infos[1]);
}

static void
teardown(void)
{
	print_next = NULL;
	col1_next = NULL;
	col2_next = NULL;

	columns_free(columns);
}

static void
test_cant_add_columns_with_same_id(void)
{
	assert_false(columns_add_column_desc(COL1_ID, NULL) == 0);
	assert_false(columns_add_column_desc(COL2_ID, NULL) == 0);
}

static void
test_not_out_of_max_width(void)
{
	columns_format_line(columns, NULL, MAX_WIDTH);
}

static void
test_free_null_columns_ok(void)
{
	columns_free(NULL_COLUMNS);
}

static void
test_add_duplicate_columns_ok(void)
{
	static column_info_t column_info =
	{
		.column_id = COL1_ID, .full_width = 0,   .text_width = 0,
		.align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE,
	};

	columns_add_column(columns, column_info);
}

void
general_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_cant_add_columns_with_same_id);
	run_test(test_not_out_of_max_width);
	run_test(test_free_null_columns_ok);
	run_test(test_add_duplicate_columns_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
