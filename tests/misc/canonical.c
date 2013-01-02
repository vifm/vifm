#include "seatest.h"

#include "../../src/filelist.h"

#ifndef _WIN32
#define ABS_PREFIX
#else
#define ABS_PREFIX "c:"
#endif

static void
root_updir(void)
{
	char buf[PATH_MAX];

	canonicalize_path(ABS_PREFIX "/..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/../../", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);
}

static void
not_root_updir(void)
{
	char buf[PATH_MAX];

	canonicalize_path("../", buf, sizeof(buf));
	assert_string_equal("../", buf);

	canonicalize_path("../../", buf, sizeof(buf));
	assert_string_equal("../../", buf);
}

static void
remove_dots(void)
{
	char buf[PATH_MAX];

	canonicalize_path("./", buf, sizeof(buf));
	assert_string_equal("./", buf);

	canonicalize_path(ABS_PREFIX "/a/./", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/a/", buf);

	canonicalize_path("././././././", buf, sizeof(buf));
	assert_string_equal("./", buf);
}

static void
excess_slashes(void)
{
	char buf[PATH_MAX];

	canonicalize_path(ABS_PREFIX "//", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/////////////", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);
}

static void
complex_tests(void)
{
	char buf[PATH_MAX];

	canonicalize_path(ABS_PREFIX "/a/b/../c/../..", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "/a/./b/./.././c/../../.", buf, sizeof(buf));
	assert_string_equal(ABS_PREFIX "/", buf);

	canonicalize_path(ABS_PREFIX "//a//./b/./../////./c///.././/", buf,
			sizeof(buf));
	assert_string_equal(ABS_PREFIX "/a/", buf);
}

static void
allow_unc(void)
{
	char buf[PATH_MAX];

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

static void
treat_many_dots_right(void)
{
	char buf[PATH_MAX];

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

void
canonical(void)
{
	test_fixture_start();

	run_test(root_updir);
	run_test(not_root_updir);
	run_test(remove_dots);
	run_test(excess_slashes);
	run_test(complex_tests);
#ifdef _WIN32
	run_test(allow_unc);
#endif
	run_test(treat_many_dots_right);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
