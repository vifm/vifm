#include "seatest.h"

#include <string.h> /* strdup() */
#include <stdlib.h> /* free() */

#include "../../src/utils/path.h"

/* XXX: get_ext and split_ext share test data. */

static void
test_empty_string(void)
{
	assert_string_equal("", get_ext(""));
}

static void
test_empty_extension(void)
{
	assert_string_equal("", get_ext("file."));
}

static void
test_empty_root(void)
{
	assert_string_equal("", get_ext(".ext"));
}

static void
test_empty_root_double_extension(void)
{
	assert_string_equal("ext2", get_ext(".ext1.ext2"));
}

static void
test_filename_no_extension(void)
{
	assert_string_equal("", get_ext("withoutext"));
}

static void
test_filename_unary_extension(void)
{
	assert_string_equal("ext", get_ext("with.ext"));
}

static void
test_filename_binary_extensions(void)
{
	assert_string_equal("ext", get_ext("with.two.ext"));
}

static void
test_path_no_extension(void)
{
	assert_string_equal("", get_ext("/home/user.name/file"));
}

static void
test_path_unary_extension(void)
{
	assert_string_equal("ext", get_ext("/home/user.name/file.ext"));
}

static void
test_path_binary_extensions(void)
{
	assert_string_equal("ext", get_ext("/home/user.name/file.double.ext"));
}

void
get_ext_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string);
	run_test(test_empty_extension);
	run_test(test_empty_root);
	run_test(test_empty_root_double_extension);
	run_test(test_filename_no_extension);
	run_test(test_filename_unary_extension);
	run_test(test_filename_binary_extensions);
	run_test(test_path_no_extension);
	run_test(test_path_unary_extension);
	run_test(test_path_binary_extensions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
