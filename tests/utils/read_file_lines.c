#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/background.h"
#include "../../src/utils/string_array.h"

TEST(non_existing_file)
{
	int nlines;
	char **lines = read_file_of_lines(TEST_DATA_PATH "/read/wrong-path", &nlines);
	assert_non_null(lines);
	assert_int_equal(0, nlines);
	free(lines);
}

TEST(dos_line_endings)
{
	int nlines;
	char **lines = read_file_of_lines(TEST_DATA_PATH "/read/dos-line-endings",
			&nlines);

	if(lines == NULL)
	{
		assert_fail("Failed to read file lines");
		return;
	}

	assert_int_equal(3, nlines);
	assert_string_equal("first line", lines[0]);
	assert_string_equal("second line", lines[1]);
	assert_string_equal("third line", lines[2]);

	free_string_array(lines, nlines);
}

TEST(dos_end_of_file)
{
	int nlines;
	char **lines = read_file_of_lines(TEST_DATA_PATH "/read/dos-eof", &nlines);

	if(lines == NULL)
	{
		assert_fail("Failed to read file lines");
		return;
	}

	assert_int_equal(3, nlines);
	assert_string_equal("next line contains EOF", lines[0]);
	assert_string_equal("\x1a", lines[1]);
	assert_string_equal("this is \"invisible\" line", lines[2]);

	free_string_array(lines, nlines);
}

TEST(binary_data_is_fully_read)
{
	int nlines;
	char **lines = read_file_of_lines(TEST_DATA_PATH "/read/binary-data",
			&nlines);

	assert_true(lines != NULL);
	assert_int_equal(12, nlines);

	free_string_array(lines, nlines);
}

TEST(bom_is_skipped)
{
	int nlines;
	char **lines = read_file_of_lines(TEST_DATA_PATH "/read/utf8-bom", &nlines);

	assert_true(lines != NULL);
	assert_int_equal(2, nlines);

	free_string_array(lines, nlines);
}

TEST(bom_is_skipped_for_file)
{
	FILE *fp = fopen(TEST_DATA_PATH "/read/utf8-bom", "rb");
	int nlines;
	char **lines = read_file_lines(fp, &nlines);

	assert_true(lines != NULL);
	assert_int_equal(2, nlines);

	free_string_array(lines, nlines);
	fclose(fp);
}

TEST(bom_is_skipped_for_stream)
{
	FILE *fp = fopen(TEST_DATA_PATH "/read/utf8-bom", "rb");
	int nlines;
	char **lines = read_stream_lines(fp, &nlines, 0, NULL, NULL);

	assert_true(lines != NULL);
	assert_int_equal(2, nlines);

	free_string_array(lines, nlines);
	fclose(fp);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
