#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

#include "../../src/utils/utils.h"

#define DEFAULT_LINENUM 1

static int windows(void);

SETUP()
{
	assert_success(chdir(TEST_DATA_PATH));
}

TEST(empty_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(empty_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec(":78:", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(78, line_num);

	free(path);
}

TEST(absolute_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec(TEST_DATA_PATH, &line_num);

	assert_string_equal(TEST_DATA_PATH, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(tilde_path_is_expanded)
{
	int line_num;
	char *const path = parse_file_spec("~", &line_num);
	assert_false(strcmp(path, "~") == 0);
	free(path);
}

TEST(absolute_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec(TEST_DATA_PATH ":1234:", &line_num);

	assert_string_equal(TEST_DATA_PATH, path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(relative_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_without_linenum_ends_with_colon)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_without_linenum_with_text)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a: text", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:9876:", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(9876, line_num);

	free(path);
}

TEST(empty_path_linenum_no_trailing_colon)
{
	int line_num;
	char *const path = parse_file_spec(":78", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(absolute_path_linenum_no_trailing_colon)
{
	int line_num;
	char *const path = parse_file_spec(TEST_DATA_PATH ":1234", &line_num);

	assert_string_equal(TEST_DATA_PATH, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_linenum_no_trailing_colon)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:9876", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(linenum_non_digit)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:a:", &line_num);

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(linenum_non_number)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo:7a:", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(colon_in_name_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo:7a:::a4:17:text", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_absolute_path_without_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec(TEST_DATA_PATH, &line_num);

	assert_string_equal(TEST_DATA_PATH, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_absolute_path_with_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user:1234:", &line_num);

	assert_string_equal("c:/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(win_relative_path_without_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_relative_path_with_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo:9876:", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

static int
windows(void)
{
#ifdef _WIN32
	return 1;
#else
	return 0;
#endif
}
/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
