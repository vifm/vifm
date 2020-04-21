#include <stic.h>

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/path.h"

#ifndef _WIN32
#define ABS_PREFIX
#else
#define ABS_PREFIX "c:"
#endif

TEST(basic)
{
	char buf[PATH_MAX + 1];

	canonicalize_path(ABS_PREFIX "/", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	to_canonic_path(ABS_PREFIX "/", "fake-base", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);
}

TEST(trailing_dots_in_filename)
{
	char buf[PATH_MAX + 1];

	canonicalize_path("/tmp/test../", buf, sizeof(buf));
	assert_string_equal("/tmp/test../", buf);

	canonicalize_path("/tmp/test../..", buf, sizeof(buf));
	assert_string_equal("/tmp/", buf);

	canonicalize_path("/tmp/test.../", buf, sizeof(buf));
	assert_string_equal("/tmp/test.../", buf);

	canonicalize_path("/tmp/test.../..", buf, sizeof(buf));
	assert_string_equal("/tmp/", buf);
}

TEST(root_updir)
{
	char buf[PATH_MAX + 1];

	canonicalize_path(ABS_PREFIX "/..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../../", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);
}

TEST(not_root_updir)
{
	char buf[PATH_MAX + 1];

	canonicalize_path("../", buf, sizeof(buf));
	assert_string_equal("../", buf);

	canonicalize_path("a/../b", buf, sizeof(buf));
	assert_string_equal("b/", buf);

	canonicalize_path("../b", buf, sizeof(buf));
	assert_string_equal("../b/", buf);

	canonicalize_path("../../", buf, sizeof(buf));
	assert_string_equal("../../", buf);

	canonicalize_path("../../../", buf, sizeof(buf));
	assert_string_equal("../../../", buf);

	canonicalize_path("b/..", buf, sizeof(buf));
	assert_string_equal("./", buf);
}

TEST(remove_dots)
{
	char buf[PATH_MAX + 1];

	canonicalize_path("./", buf, sizeof(buf));
	assert_string_equal("./", buf);

	canonicalize_path(ABS_PREFIX "/a/./", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/a/", buf);

	canonicalize_path("././././././", buf, sizeof(buf));
	assert_string_equal("./", buf);
}

TEST(excess_slashes)
{
	char buf[PATH_MAX + 1];

	canonicalize_path(ABS_PREFIX "//", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/////////////", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);
}

TEST(complex_tests)
{
	char buf[PATH_MAX + 1];

	canonicalize_path(ABS_PREFIX "/a/b/../c/../..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/a/./b/./.././c/../../.", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "//a//./b/./../////./c///.././/", buf,
			sizeof(buf));
	assert_string_equal(ABS_PREFIX "/a/", buf);

	canonicalize_path("./../", buf, sizeof(buf));
	assert_string_equal("../", buf);

	canonicalize_path("./../dir", buf, sizeof(buf));
	assert_string_equal("../dir/", buf);
}

TEST(treat_many_dots_right)
{
	char buf[PATH_MAX + 1];

	canonicalize_path("...", buf, sizeof(buf));
#ifndef _WIN32
	assert_string_equal(".../", buf);
#else
	assert_string_equal("./", buf);
#endif

	canonicalize_path(".../", buf, sizeof(buf));
#ifndef _WIN32
	assert_string_equal(".../", buf);
#else
	assert_string_equal("./", buf);
#endif

	canonicalize_path("...abc", buf, sizeof(buf));
	assert_string_equal("...abc/", buf);

	canonicalize_path(".abc", buf, sizeof(buf));
	assert_string_equal(".abc/", buf);

	canonicalize_path("abc...", buf, sizeof(buf));
	assert_string_equal("abc.../", buf);

	canonicalize_path("abc.", buf, sizeof(buf));
	assert_string_equal("abc./", buf);

	canonicalize_path(ABS_PREFIX "/a/.../.", buf, sizeof(buf));
#ifndef _WIN32
	assert_string_equal(ABS_PREFIX "/a/.../", buf);
#else
	assert_string_equal(ABS_PREFIX "/a/", buf);
#endif

	canonicalize_path(ABS_PREFIX "/windows/.../", buf, sizeof(buf));
#ifndef _WIN32
	assert_string_equal(ABS_PREFIX "/windows/.../", buf);
#else
	assert_string_equal(ABS_PREFIX "/windows/", buf);
#endif

	canonicalize_path(ABS_PREFIX "/windows/...abc", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/windows/...abc/", buf);

	canonicalize_path(ABS_PREFIX "/windows/..................", buf, sizeof(buf));
#ifndef _WIN32
	assert_string_equal(ABS_PREFIX "/windows/................../", buf);
#else
	assert_string_equal(ABS_PREFIX "/windows/", buf);
#endif
}

TEST(allow_unc, IF(windows))
{
	char buf[PATH_MAX + 1];

	canonicalize_path("//server", buf, sizeof(buf));
	assert_string_equal("//server/", buf);

	canonicalize_path("//server//resource", buf, sizeof(buf));
	assert_string_equal("//server/resource/", buf);

	canonicalize_path("//server//..", buf, sizeof(buf));
	assert_string_equal("//server/", buf);

	canonicalize_path("//server/../", buf, sizeof(buf));
	assert_string_equal("//server/", buf);

	canonicalize_path("//server/resource/../", buf, sizeof(buf));
	assert_string_equal("//server/", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
