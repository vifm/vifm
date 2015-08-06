#include <stic.h>

#include <string.h> /* strdup() */
#include <stdlib.h> /* free() */

#include "../../src/utils/path.h"

/* XXX: get_ext and split_ext share test data. */

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

TEST(empty_string)
{
	TEST_EXT("", "", 0);
}

TEST(empty_extension)
{
	TEST_EXT("file.", "", 4);
}

TEST(empty_root)
{
	TEST_EXT(".ext", "", 4);
}

TEST(empty_root_double_extension)
{
	TEST_EXT(".ext1.ext2", "ext2", 5);
}

TEST(filename_no_extension)
{
	TEST_EXT("withoutext", "", 10);
}

TEST(filename_unary_extension)
{
	TEST_EXT("with.ext", "ext", 4);
}

TEST(filename_binary_extensions)
{
	TEST_EXT("with.two.ext", "ext", 8);
}

TEST(path_no_extension)
{
	TEST_EXT("/home/user.name/file", "", 20);
}

TEST(path_unary_extension)
{
	TEST_EXT("/home/user.name/file.ext", "ext", 20);
}

TEST(path_binary_extensions)
{
	TEST_EXT("/home/user.name/file.double.ext", "ext", 27);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
