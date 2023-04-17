#include <stic.h>

#include <unistd.h> /* unlink() */

#include <stdio.h> /* fclose() fopen() */

#include <test-utils.h>

#include "../../src/utils/str.h"
#include "../../src/utils/path.h"

TEST(in_one_dir)
{
	const char *buf;

	buf = make_rel_path("/vifm-test/a", "/vifm-test/");
	assert_string_equal("a", buf);

	buf = make_rel_path("/vifm-test/", "/vifm-test/");
	assert_string_equal(".", buf);

	buf = make_rel_path("/vifm-test", "/vifm-test/");
	assert_string_equal(".", buf);

	buf = make_rel_path("/vifm-test/", "/vifm-test");
	assert_string_equal(".", buf);

	buf = make_rel_path("/vifm-test", "/vifm-test");
	assert_string_equal(".", buf);
}

TEST(slashes)
{
	const char *buf;

	buf = make_rel_path("/vifm-test/a", "/vifm-test");
	assert_string_equal("a", buf);

	buf = make_rel_path("/vifm-test/a", "/vifm-test/");
	assert_string_equal("a", buf);

	buf = make_rel_path("/vifm-test/a/", "/vifm-test");
	assert_string_equal("a", buf);

	buf = make_rel_path("/vifm-test/a/", "/vifm-test/");
	assert_string_equal("a", buf);
}

TEST(under_dir)
{
	const char *buf;

	buf = make_rel_path("/vifm-test/a/b", "/vifm-test/");
	assert_string_equal("a/b", buf);

	buf = make_rel_path("/vifm-test/a/b///c", "/vifm-test/");
	assert_string_equal("a/b/c", buf);
}

TEST(parent_dir)
{
	const char *buf;

	buf = make_rel_path("/", "/vifm-test/");
	assert_string_equal("..", buf);

	buf = make_rel_path("/", "/vifm-test/user/");
	assert_string_equal("../..", buf);

	buf = make_rel_path("/", "/vifm-test/////user/dir/");
	assert_string_equal("../../..", buf);
}

TEST(different_subtree)
{
	const char *buf;

	buf = make_rel_path("/vifm-test/user1///u1dir1",
			"/vifm-test/user2/././/u2dir1/.");
	assert_string_equal("../../user1/u1dir1", buf);

	buf = make_rel_path("/vifm-test/file", "/dir-in-root");
	assert_string_equal("../vifm-test/file", buf);
}

TEST(windows_specific, IF(windows))
{
	const char *buf;

	buf = make_rel_path("c:/vifm-test/user1/dir1", "c:/vifm-test/user1/");
	assert_string_equal("dir1", buf);

	buf = make_rel_path("c:/vifm-test/user1/", "d:/vifm-test/user1/dir1");
	assert_string_equal("c:/vifm-test/user1/", buf);
}

TEST(relative_links_are_built_for_real_paths, IF(not_windows))
{
	assert_success(make_symlink(".", SANDBOX_PATH "/a"));
	assert_success(make_symlink(".", SANDBOX_PATH "/b"));

	fclose(fopen(SANDBOX_PATH "/target", "w"));

	const char *rel_path = make_rel_path(SANDBOX_PATH "/a/a/a/target",
			SANDBOX_PATH "/a/a/a/b/b/b");
	assert_string_equal("target", rel_path);

	assert_success(unlink(SANDBOX_PATH "/a"));
	assert_success(unlink(SANDBOX_PATH "/b"));
	assert_success(unlink(SANDBOX_PATH "/target"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
