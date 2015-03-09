#include <stic.h>

#include <limits.h> /* PATH_MAX */

#include "../../src/utils/path.h"

TEST(empty_path)
{
	char path[PATH_MAX] = "";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

TEST(root_path)
{
	char path[PATH_MAX] = "/";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

TEST(path_does_not_end_with_slash)
{
	char path[PATH_MAX] = "/a/b/c";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

TEST(path_ends_with_slash)
{
	char path[PATH_MAX] = "/a/b/c/";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

TEST(path_ends_with_multiple_slashes)
{
	char path[PATH_MAX] = "/a/b/c///";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
