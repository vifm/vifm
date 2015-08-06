#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */

#include "../../src/utils/file_streams.h"

TEST(reads_line_chunk_by_chunk)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");
	char *line = NULL;
	char line_buf[5];

	if(fp == NULL)
	{
		assert_fail("Failed to open file.");
		return;
	}

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st ", line);

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("line", line);

	fclose(fp);
}

TEST(eol_is_preserved)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");
	char *line = NULL;
	char line_buf[100];

	if(fp == NULL)
	{
		assert_fail("Failed to open file.");
		return;
	}

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st line\n", line);

	fclose(fp);
}

TEST(reads_more_than_one_line)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");
	char *line = NULL;
	char line_buf[100];

	if(fp == NULL)
	{
		assert_fail("Failed to open file.");
		return;
	}

	line = get_line(fp, line_buf, sizeof(line_buf));
	assert_string_equal("1st line\n", line);

	line = get_line(fp, line_buf, 5);
	assert_string_equal("2nd ", line);

	fclose(fp);
}

TEST(writes_nothing_and_returns_null_on_zero_size_buffer)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");
	char *line = NULL;
	char line_buf[] = "01";

	if(fp == NULL)
	{
		assert_fail("Failed to open file.");
		return;
	}

	line = get_line(fp, line_buf, 0);
	assert_string_equal(NULL, line);
	assert_string_equal("01", line_buf);

	fclose(fp);
}

TEST(writes_nothing_and_returns_null_on_one_byte_buffer)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/two-lines", "r");
	char *line = NULL;
	char line_buf[] = "01";

	if(fp == NULL)
	{
		assert_fail("Failed to open file.");
		return;
	}

	line = get_line(fp, line_buf, 1);
	assert_string_equal(NULL, line);
	assert_string_equal("01", line_buf);

	fclose(fp);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
