#include <stdio.h> /* FILE fopen() fclose() */

#include "seatest.h"

#include "../../src/utils/file_streams.h"

static void
test_reads_line_chunk_by_chunk(void)
{
	FILE *const fp = fopen("test-data/read/two-lines", "r");
	char *line = NULL;
	char line_buf[5];

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st ", line);

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("line", line);

	fclose(fp);
}

static void
test_eol_is_preserved(void)
{
	FILE *const fp = fopen("test-data/read/two-lines", "r");
	char *line = NULL;
	char line_buf[100];

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st line\n", line);

	fclose(fp);
}

static void
test_reads_more_than_one_line(void)
{
	FILE *const fp = fopen("test-data/read/two-lines", "r");
	char *line = NULL;
	char line_buf[100];

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st line\n", line);

	line = get_line(fp, line_buf, 5);
	assert_string_equal("2nd ", line);

	fclose(fp);
}

static void
test_writes_nothing_and_returns_null_on_zero_size_buffer(void)
{
	FILE *const fp = fopen("test-data/read/two-lines", "r");
	char *line = NULL;
	char line_buf[] = "01";

	line = get_line(fp, line_buf, 0);
	assert_string_equal(NULL, line);
	assert_string_equal("01", line_buf);

	fclose(fp);
}

static void
test_writes_nothing_and_returns_null_on_one_byte_buffer(void)
{
	FILE *const fp = fopen("test-data/read/two-lines", "r");
	char *line = NULL;
	char line_buf[] = "01";

	line = get_line(fp, line_buf, 1);
	assert_string_equal(NULL, line);
	assert_string_equal("01", line_buf);

	fclose(fp);
}

void
get_line_tests(void)
{
	test_fixture_start();

	run_test(test_reads_line_chunk_by_chunk);
	run_test(test_eol_is_preserved);
	run_test(test_reads_more_than_one_line);
	run_test(test_writes_nothing_and_returns_null_on_zero_size_buffer);
	run_test(test_writes_nothing_and_returns_null_on_one_byte_buffer);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
