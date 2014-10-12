#include "seatest.h"

#include <string.h> /* strdup() */
#include <stdlib.h> /* free() */

#include "../../src/utils/path.h"

#define TEST_EXT(full, expected_ext, expected_root_len) \
	int root_len; \
	const char *ext; \
	char *const name = strdup(full); \
	\
	split_ext(name, &root_len, &ext); \
	\
	assert_string_equal(expected_ext, ext); \
	assert_int_equal((expected_root_len), root_len); \
	\
	free(name);

static void
test_empty_string(void)
{
	TEST_EXT("", "", 0);
}

static void
test_empty_extension(void)
{
	TEST_EXT("file.", "", 4);
}

static void
test_empty_root(void)
{
	TEST_EXT(".ext", "", 4);
}

static void
test_empty_root_double_extension(void)
{
	TEST_EXT(".ext1.ext2", "ext2", 5);
}

static void
test_filename_no_extension(void)
{
	TEST_EXT("withoutext", "", 10);
}

static void
test_filename_unary_extension(void)
{
	TEST_EXT("with.ext", "ext", 4);
}

static void
test_filename_binary_extensions(void)
{
	TEST_EXT("with.two.ext", "ext", 8);
}

static void
test_path_no_extension(void)
{
	TEST_EXT("/home/user.name/file", "", 20);
}

static void
test_path_unary_extension(void)
{
	TEST_EXT("/home/user.name/file.ext", "ext", 20);
}

static void
test_path_binary_extensions(void)
{
	TEST_EXT("/home/user.name/file.double.ext", "ext", 27);
}

void
split_ext_tests(void)
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
