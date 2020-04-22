#include <stic.h>

#include <unistd.h> /* symlink() unlink() */

#include <stdio.h> /* fclose() fopen() */

#include <test-utils.h>

#include "../../src/utils/str.h"
#include "../../src/utils/path.h"

TEST(in_one_dir)
{
	const char *buf;

	buf = make_rel_path("/home/a", "/home/");
	assert_string_equal("a", buf);

	buf = make_rel_path("/home/", "/home/");
	assert_string_equal(".", buf);

	buf = make_rel_path("/home", "/home/");
	assert_string_equal(".", buf);

	buf = make_rel_path("/home/", "/home");
	assert_string_equal(".", buf);

	buf = make_rel_path("/home", "/home");
	assert_string_equal(".", buf);
}

TEST(slashes)
{
	const char *buf;

	buf = make_rel_path("/home/a", "/home");
	assert_string_equal("a", buf);

	buf = make_rel_path("/home/a", "/home/");
	assert_string_equal("a", buf);

	buf = make_rel_path("/home/a/", "/home");
	assert_string_equal("a", buf);

	buf = make_rel_path("/home/a/", "/home/");
	assert_string_equal("a", buf);
}

TEST(under_dir)
{
	const char *buf;

	buf = make_rel_path("/home/a/b", "/home/");
	assert_string_equal("a/b", buf);

	buf = make_rel_path("/home/a/b///c", "/home/");
	assert_string_equal("a/b/c", buf);
}

TEST(parent_dir)
{
	const char *buf;

	buf = make_rel_path("/", "/home/");
	assert_string_equal("..", buf);

	buf = make_rel_path("/", "/home/user/");
	assert_string_equal("../..", buf);

	buf = make_rel_path("/", "/home/////user/dir/");
	assert_string_equal("../../..", buf);
}

TEST(different_subtree)
{
	const char *buf;

	buf = make_rel_path("/home/user1///u1dir1", "/home/user2/././/u2dir1/.");
	assert_string_equal("../../user1/u1dir1", buf);

	buf = make_rel_path("/home/file", "/dir-in-root");
	assert_string_equal("../home/file", buf);
}

TEST(windows_specific, IF(windows))
{
	const char *buf;

	buf = make_rel_path("c:/home/user1/dir1", "c:/home/user1/");
	assert_string_equal("dir1", buf);

	buf = make_rel_path("c:/home/user1/", "d:/home/user1/dir1");
	assert_string_equal("c:/home/user1/", buf);
}

TEST(relative_links_are_built_for_real_paths, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink(".", SANDBOX_PATH "/a"));
	assert_success(symlink(".", SANDBOX_PATH "/b"));
#endif

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
