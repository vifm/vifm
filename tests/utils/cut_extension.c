#include <stic.h>

#include "../../src/utils/path.h"

TEST(no_extension)
{
	char buf[] = "file";
	assert_string_equal("", cut_extension(buf));
	assert_string_equal("file", buf);
}

TEST(unary_extensions)
{
	char buf[][20] =
	{
		"file.jpg",
		"program-1.0.zip",
		"tar.zip",
	};
	assert_string_equal("jpg", cut_extension(buf[0]));
	assert_string_equal("file", buf[0]);
	assert_string_equal("zip", cut_extension(buf[1]));
	assert_string_equal("program-1.0", buf[1]);
	assert_string_equal("zip", cut_extension(buf[2]));
	assert_string_equal("tar", buf[2]);
}

TEST(binary_extensions)
{
	char buf[][20] =
	{
		"archive.tar.gz",
		"photos.tar.bz2",
	};
	assert_string_equal("tar.gz", cut_extension(buf[0]));
	assert_string_equal("archive", buf[0]);
	assert_string_equal("tar.bz2", cut_extension(buf[1]));
	assert_string_equal("photos", buf[1]);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
