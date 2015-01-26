#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/menus/menus.h"

#define DEFAULT_LINENUM 1

static void
test_empty_path_without_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

static void
test_empty_path_with_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec(":78", &line_num);

	assert_string_equal("./", path);
	assert_int_equal(78, line_num);

	free(path);
}

static void
test_absolute_path_without_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("/home/user", &line_num);

	assert_string_equal("/home/user", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

static void
test_absolute_path_with_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("/home/user:1234", &line_num);

	assert_string_equal("/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

static void
test_relative_path_without_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

static void
test_relative_path_with_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo:9876", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

#ifdef _WIN32

static void
test_win_absolute_path_without_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user", &line_num);

	assert_string_equal("c:/home/user", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

static void
test_win_absolute_path_with_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user:1234", &line_num);

	assert_string_equal("c:/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

static void
test_win_relative_path_without_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

static void
test_win_relative_path_with_linenum(void)
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo:9876", &line_num);

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

#endif

void
parse_file_spec_tests(void)
{
	test_fixture_start();

	run_test(test_empty_path_without_linenum);
	run_test(test_empty_path_with_linenum);
	run_test(test_absolute_path_without_linenum);
	run_test(test_absolute_path_with_linenum);
	run_test(test_relative_path_without_linenum);
	run_test(test_relative_path_with_linenum);
#ifdef _WIN32
	run_test(test_win_absolute_path_without_linenum);
	run_test(test_win_absolute_path_with_linenum);
	run_test(test_win_relative_path_without_linenum);
	run_test(test_win_relative_path_with_linenum);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
