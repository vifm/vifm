#include <stic.h>

#include <stddef.h> /* NULL size_t */

#include "../../src/ui/column_view.h"

#include "test.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[]);
static void column1_func(int id, const void *data, size_t buf_len, char *buf);
static void column2_func(int id, const void *data, size_t buf_len, char *buf);

DEFINE_SUITE();

SETUP()
{
	columns_set_ellipsis("...");
	columns_set_line_print_func(column_line_print);
	assert_int_equal(0, columns_add_column_desc(COL1_ID, column1_func));
	assert_int_equal(0, columns_add_column_desc(COL2_ID, column2_func));
}

TEARDOWN()
{
	columns_clear_column_descs();
}

static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[])
{
	assert_non_null(print_next);
	print_next(data, column_id, buf, offset, align);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
