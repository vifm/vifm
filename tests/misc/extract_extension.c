#include "seatest.h"

#include "../../src/utils/path.h"

static void
test_no_extension(void)
{
	char buf[] = "file";
	assert_string_equal("", extract_extension(buf));
	assert_string_equal("file", buf);
}

static void
test_unary_extensions(void)
{
	char buf[][20] =
	{
		"file.jpg",
		"program-1.0.zip",
		"tar.zip",
	};
	assert_string_equal("jpg", extract_extension(buf[0]));
	assert_string_equal("file", buf[0]);
	assert_string_equal("zip", extract_extension(buf[1]));
	assert_string_equal("program-1.0", buf[1]);
	assert_string_equal("zip", extract_extension(buf[2]));
	assert_string_equal("tar", buf[2]);
}

static void
test_binary_extensions(void)
{
	char buf[][20] =
	{
		"archive.tar.gz",
		"photos.tar.bz2",
	};
	assert_string_equal("tar.gz", extract_extension(buf[0]));
	assert_string_equal("archive", buf[0]);
	assert_string_equal("tar.bz2", extract_extension(buf[1]));
	assert_string_equal("photos", buf[1]);
}

void
extract_extension_tests(void)
{
	test_fixture_start();

	run_test(test_no_extension);
	run_test(test_unary_extensions);
	run_test(test_binary_extensions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
