#include "seatest.h"

#include "../../src/utils/path.h"

static void
test_in_one_dir(void)
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

static void
test_slashes(void)
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

static void
test_under_dir(void)
{
	const char *buf;

	buf = make_rel_path("/home/a/b", "/home/");
	assert_string_equal("a/b", buf);

	buf = make_rel_path("/home/a/b///c", "/home/");
	assert_string_equal("a/b/c", buf);
}

static void
test_parent_dir(void)
{
	const char *buf;

	buf = make_rel_path("/", "/home/");
	assert_string_equal("..", buf);

	buf = make_rel_path("/", "/home/user/");
	assert_string_equal("../..", buf);

	buf = make_rel_path("/", "/home/////user/dir/");
	assert_string_equal("../../..", buf);
}

static void
test_different_subtree(void)
{
	const char *buf;

	buf = make_rel_path("/home/user1///u1dir1", "/home/user2/././/u2dir1/.");
	assert_string_equal("../../user1/u1dir1", buf);

	buf = make_rel_path("/home/file", "/tmp");
	assert_string_equal("../home/file", buf);
}

#ifdef _WIN32
static void
test_windows_specific(void)
{
	const char *buf;

	buf = make_rel_path("c:/home/user1/dir1", "c:/home/user1/");
	assert_string_equal("dir1", buf);

	buf = make_rel_path("c:/home/user1/", "d:/home/user1/dir1");
	assert_string_equal("c:/home/user1/", buf);
}
#endif

void
rel_symlinks_tests(void)
{
	test_fixture_start();

	run_test(test_in_one_dir);
	run_test(test_slashes);
	run_test(test_under_dir);
	run_test(test_parent_dir);
	run_test(test_different_subtree);
#ifdef _WIN32
	run_test(test_windows_specific);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
