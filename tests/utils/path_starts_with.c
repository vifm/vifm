#include <stic.h>

#include "../../src/utils/path.h"

TEST(yes)
{
	assert_true(path_starts_with("/home/trash", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash/"));
	assert_true(path_starts_with("/home/trash", "/home/trash/"));
}

TEST(no)
{
	assert_false(path_starts_with("/home/tras", "/home/trash"));
	assert_false(path_starts_with("/home/trash_", "/home/trash"));
}

TEST(root_prefix)
{
	assert_true(path_starts_with("/", "/"));
	assert_true(path_starts_with("/bin", "/"));

	assert_false(path_starts_with("", "/"));
	assert_false(path_starts_with("bin", "/"));
}

TEST(single_character_prefix)
{
	assert_true(path_starts_with("a", "a"));
	assert_true(path_starts_with("a/b", "a"));

	assert_false(path_starts_with("/a", "a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
