#include <stic.h>

#include "../../src/utils/path.h"

#include "utils.h"

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

	buf = make_rel_path("/home/file", "/tmp");
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
