#include "test.h"

#include "seatest.h"

#include "../../src/column_view.h"

void align_tests(void);
void callbacks_tests(void);
void cropping_tests(void);
void general_tests(void);
void utf8_tests(void);
void width_tests(void);

static void
all_tests(void)
{
	align_tests();
	callbacks_tests();
	cropping_tests();
	general_tests();
	utf8_tests();
	width_tests();
}

static void
column_line_print(const void *data, int column_id, const char *buf,
		size_t offset)
{
	assert_true(print_next != NULL);
	print_next(data, column_id, buf, offset);
}

static void
column1_func(int id, const void *data, size_t buf_len, char *buf)
{
	assert_true(col1_next != NULL);
	col1_next(id, data, buf_len, buf);
}

static void
column2_func(int id, const void *data, size_t buf_len, char *buf)
{
	assert_true(col2_next != NULL);
	col2_next(id, data, buf_len, buf);
}

static void
setup(void)
{
	columns_set_line_print_func(column_line_print);
	assert_int_equal(0, columns_add_column_desc(COL1_ID, column1_func));
	assert_int_equal(0, columns_add_column_desc(COL2_ID, column2_func));
}

static void
teardown(void)
{
	columns_clear_column_descs();
}

int
main(int argc, char **argv)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
