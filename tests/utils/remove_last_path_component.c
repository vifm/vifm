#include <stic.h>

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/path.h"

TEST(empty_path)
{
	char path[PATH_MAX + 1] = "";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

TEST(root_path)
{
	/* XXX: is this behaviour we want for root? */
	char path[PATH_MAX + 1] = "/";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

TEST(dir_in_root)
{
	char path[PATH_MAX + 1] = "/bin";
	remove_last_path_component(path);
	assert_string_equal("/", path);
}

TEST(path_does_not_end_with_slash)
{
	char path[PATH_MAX + 1] = "/a/b/c";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

TEST(path_ends_with_slash)
{
	char path[PATH_MAX + 1] = "/a/b/c/";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

TEST(path_ends_with_multiple_slashes)
{
	char path[PATH_MAX + 1] = "/a/b/c///";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

TEST(can_remove_path_completely)
{
	char path[PATH_MAX + 1] = "name";

	remove_last_path_component(path);
	assert_true(path[0] == '\0');
}

TEST(can_remove_path_completely_on_windows, IF(windows))
{
	char path[PATH_MAX + 1] = "c:/a";

	remove_last_path_component(path);
	assert_false(path[0] == '\0');

	remove_last_path_component(path);
	assert_true(path[0] == '\0');
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
