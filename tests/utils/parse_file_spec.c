#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/utils.h"

#define DEFAULT_LINENUM 1

static char *saved_cwd;
static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(test_data, sizeof(test_data), "%s", TEST_DATA_PATH);
	}
	else
	{
		snprintf(test_data, sizeof(test_data), "%s/%s", cwd, TEST_DATA_PATH);
	}
}

SETUP()
{
	saved_cwd = save_cwd();
	assert_success(chdir(TEST_DATA_PATH));
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
}

TEST(empty_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("", &line_num, ".");

	assert_string_equal("./", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(empty_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec(":78:", &line_num, ".");

	assert_string_equal("./", path);
	assert_int_equal(78, line_num);

	free(path);
}

TEST(absolute_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec(test_data, &line_num, ".");

	assert_string_equal(test_data, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(trailing_slash_of_path_is_preserved)
{
	int line_num;
	char *path;

	strcat(test_data, "/");

	path = parse_file_spec(test_data, &line_num, ".");

	assert_string_equal(test_data, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);

	chosp(test_data);
}

TEST(trailing_forward_slash_of_path_is_preserved, IF(windows))
{
	int line_num;
	char *path;

	strcat(test_data, "\\");

	path = parse_file_spec(test_data, &line_num, ".");

	chosp(test_data);
	strcat(test_data, "/");

	assert_string_equal(test_data, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);

	chosp(test_data);
}

TEST(tilde_path_is_expanded)
{
	int line_num;
	char *const path = parse_file_spec("~", &line_num, ".");
	assert_false(strcmp(path, "~") == 0);
	free(path);
}

TEST(absolute_path_with_linenum)
{
	char spec[PATH_MAX + 1];

	int line_num;
	char *path;

	snprintf(spec, sizeof(spec), "%s:1234:", test_data);
	path = parse_file_spec(spec, &line_num, ".");

	assert_string_equal(test_data, path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(relative_path_without_linenum)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_without_linenum_ends_with_colon)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_without_linenum_with_text)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a: text", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_with_linenum)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:9876:", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(9876, line_num);

	free(path);
}

TEST(empty_path_linenum_no_trailing_colon)
{
	int line_num;
	char *const path = parse_file_spec(":78", &line_num, ".");

	assert_string_equal("./", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(absolute_path_linenum_no_trailing_colon)
{
	char spec[PATH_MAX + 1];

	int line_num;
	char *path;

	snprintf(spec, sizeof(spec), "%s:1234", test_data);
	path = parse_file_spec(spec, &line_num, ".");

	assert_string_equal(test_data, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(relative_path_linenum_no_trailing_colon)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:9876", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(linenum_non_digit)
{
	int line_num;
	char *const path = parse_file_spec("existing-files/a:a:", &line_num, ".");

	assert_string_equal("./existing-files/a", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(linenum_non_number)
{
	int line_num;
	char *const path = parse_file_spec("repos/repo:7a:", &line_num, ".");

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(colon_in_name_with_linenum)
{
	int line_num;
	char *path = parse_file_spec("repos/repo:7a:::a4:17:text", &line_num, ".");

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_absolute_path_without_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec(test_data, &line_num, ".");

	assert_string_equal(test_data, path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_absolute_path_with_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("c:/home/user:1234:", &line_num, ".");

	assert_string_equal("c:/home/user", path);
	assert_int_equal(1234, line_num);

	free(path);
}

TEST(win_relative_path_without_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo", &line_num, ".");

	assert_string_equal("./repos/repo", path);
	assert_int_equal(DEFAULT_LINENUM, line_num);

	free(path);
}

TEST(win_relative_path_with_linenum, IF(windows))
{
	int line_num;
	char *const path = parse_file_spec("repos\\repo:9876:", &line_num, ".");

	assert_string_equal("./repos/repo", path);
	assert_int_equal(9876, line_num);

	free(path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
