#include <stic.h>

#include <string.h> /* strdup() */
#include <stdlib.h> /* free() */

#include "../../src/utils/path.h"

/* XXX: get_ext and split_ext share test data. */

TEST(empty_string)
{
	assert_string_equal("", get_ext(""));
}

TEST(empty_extension)
{
	assert_string_equal("", get_ext("file."));
}

TEST(empty_root)
{
	assert_string_equal("", get_ext(".ext"));
}

TEST(empty_root_double_extension)
{
	assert_string_equal("ext2", get_ext(".ext1.ext2"));
}

TEST(filename_no_extension)
{
	assert_string_equal("", get_ext("withoutext"));
}

TEST(filename_unary_extension)
{
	assert_string_equal("ext", get_ext("with.ext"));
}

TEST(filename_binary_extensions)
{
	assert_string_equal("ext", get_ext("with.two.ext"));
}

TEST(path_no_extension)
{
	assert_string_equal("", get_ext("/home/user.name/file"));
}

TEST(path_unary_extension)
{
	assert_string_equal("ext", get_ext("/home/user.name/file.ext"));
}

TEST(path_binary_extensions)
{
	assert_string_equal("ext", get_ext("/home/user.name/file.double.ext"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
