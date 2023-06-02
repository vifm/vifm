#include <stic.h>

#include <stddef.h> /* NULL size_t */

#include "../../src/ui/column_view.h"

#include "test.h"

static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);
static void column1_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void column2_func(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

DEFINE_SUITE();

SETUP()
{
	columns_set_ellipsis("...");
	columns_set_line_print_func(&column_line_print);
	assert_int_equal(0, columns_add_column_desc(COL1_ID, &column1_func, NULL));
	assert_int_equal(0, columns_add_column_desc(COL2_ID, &column2_func, NULL));
}

TEARDOWN()
{
	columns_clear_column_descs();
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	assert_non_null(print_next);
	print_next(info->data, info->real_id, buf, offset, align);
}

static void
column1_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	assert_true(col1_next != NULL);
	col1_next(data, buf_len, buf, info);
}

static void
column2_func(void *data, size_t buf_len, char buf[], const format_info_t *info)
{
	assert_true(col2_next != NULL);
	col2_next(data, buf_len, buf, info);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
