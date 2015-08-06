#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/utils/utils.h"

#define DEFAULT_LINENUM 1

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
	char *const path = parse_file_spec(":78", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(78, line_num);

	free(path);
}

TEST(absolute_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("/home/user", &line_num);

	assert_string_equal("/home/user", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(absolute_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("/home/user:1234", &line_num);

	assert_string_equal("/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(relative_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo:9876", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

#ifdef _WIN32

TEST(win_absolute_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user", &line_num);

	assert_string_equal("c:/home/user", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_absolute_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user:1234", &line_num);

	assert_string_equal("c:/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(win_relative_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_relative_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo:9876", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
