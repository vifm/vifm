#include "seatest.h"

#include <stddef.h> /* size_t */

#include "../../src/column_view.h"
#include "test.h"

static const size_t MAX_WIDTH = 80;

static columns_t columns;

static int print_counter;
static int column1_counter;
static int column2_counter;

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	print_counter++;
}

static void
columns_func(int id, const void *data, size_t buf_len, char *buf)
{
	if(id == COL1_ID)
	{
		column1_counter++;
	}
	else
	{
		column2_counter++;
	}
}

static void
setup(void)
{
	static column_info_t column_infos[2] =
	{
		{ .column_id = COL1_ID, .full_width = 100, .text_width = 100,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
		{ .column_id = COL2_ID, .full_width = 100, .text_width = 100,
		  .align = AT_LEFT,     .sizing = ST_AUTO, .cropping = CT_NONE, },
	};

	print_next = column_line_print;
	col1_next = columns_func;
	col2_next = columns_func;

	print_counter = 0;
	column1_counter = 0;
	column2_counter = 0;

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
test_no_columns_one_print_callback_after_creation(void)
{
	columns_t cols = columns_create();

	columns_format_line(cols, NULL, MAX_WIDTH);

	assert_int_equal(0, column1_counter);
	assert_int_equal(0, column2_counter);
	/* Gap filling callback. */
	assert_int_equal(1, print_counter);

	columns_free(cols);
}

static void
test_no_columns_one_print_callback_after_clearing(void)
{
	columns_clear(columns);

	columns_format_line(columns, NULL, MAX_WIDTH);

	assert_int_equal(0, column1_counter);
	assert_int_equal(0, column2_counter);
	/* Gap filling callback. */
	assert_int_equal(1, print_counter);
}

static void
test_number_of_calls_to_format_functions(void)
{
	columns_format_line(columns, NULL, MAX_WIDTH);

	assert_int_equal(1, column1_counter);
	assert_int_equal(1, column2_counter);
}

static void
test_number_of_calls_to_print_function(void)
{
	columns_format_line(columns, NULL, MAX_WIDTH);

	/* Two more calls are for filling gaps. */
	assert_int_equal(4, print_counter);
}

void
callbacks_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_no_columns_one_print_callback_after_creation);
	run_test(test_no_columns_one_print_callback_after_clearing);
	run_test(test_number_of_calls_to_format_functions);
	run_test(test_number_of_calls_to_print_function);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
