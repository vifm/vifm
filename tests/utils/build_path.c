#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/path.h"

TEST(non_empty_parts)
{
	char path[PATH_MAX + 1];

	build_path(path, sizeof(path), "a", "b");
	assert_string_equal("a/b", path);

	build_path(path, sizeof(path), "a/", "b");
	assert_string_equal("a/b", path);

	build_path(path, sizeof(path), "a", "/b");
	assert_string_equal("a/b", path);

	build_path(path, sizeof(path), "a/", "/b");
	assert_string_equal("a/b", path);
}

TEST(empty_head)
{
	char path[PATH_MAX + 1];

	build_path(path, sizeof(path), "", "b");
	assert_string_equal("/b", path);

	build_path(path, sizeof(path), "/", "b");
	assert_string_equal("/b", path);

	build_path(path, sizeof(path), "", "/b");
	assert_string_equal("/b", path);

	build_path(path, sizeof(path), "/", "/b");
	assert_string_equal("/b", path);
}

TEST(empty_tail)
{
	char path[PATH_MAX + 1];

	build_path(path, sizeof(path), "a", "");
	assert_string_equal("a", path);

	build_path(path, sizeof(path), "a/", "");
	assert_string_equal("a/", path);

	build_path(path, sizeof(path), "a", "");
	assert_string_equal("a", path);

	build_path(path, sizeof(path), "a/", "");
	assert_string_equal("a/", path);
}

TEST(both_parts_empty)
{
	char path[PATH_MAX + 1];

	build_path(path, sizeof(path), "", "");
	assert_string_equal("", path);

	build_path(path, sizeof(path), "", "");
	assert_string_equal("", path);

	build_path(path, sizeof(path), "", "");
	assert_string_equal("", path);

	build_path(path, sizeof(path), "", "");
	assert_string_equal("", path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
