#include <stic.h>

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() */

#include "../../src/ui/column_view.h"
#include "test.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		int offset, AlignType align);

static const size_t MAX_WIDTH = 80;

static size_t print_offset;
static char print_buffer[800 + 1];

SETUP()
{
	print_next = &column_line_print;
}

TEARDOWN()
{
	print_next = NULL;
	col1_next = NULL;
}

static void
column_line_print(const void *data, int column_id, const char buf[], int offset,
		AlignType align)
{
	print_offset = offset;
	memcpy(print_buffer + offset, buf, strlen(buf));
}

TEST(literal_is_used)
{
	static column_info_t column_info = {
		.column_id = FILL_COLUMN_ID, .full_width = 2UL,     .text_width = 2UL,
		.align = AT_RIGHT,           .sizing = ST_ABSOLUTE, .cropping = CT_TRUNCATE,
		.literal = "abc"
	};

	columns_t *const cols = columns_create();
	columns_add_column(cols, column_info);

	memset(print_buffer, '\0', MAX_WIDTH);
	columns_format_line(cols, NULL, 10);
	assert_string_equal("bc        ", print_buffer);

	columns_free(cols);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
