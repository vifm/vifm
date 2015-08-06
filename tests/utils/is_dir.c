#include <stic.h>

#include "../../src/utils/fs.h"

TEST(file_is_not_recognized_as_dir)
{
	assert_false(is_dir(TEST_DATA_PATH "/existing-files/a"));
}

TEST(dir_is_not_recognized_as_dir)
{
	assert_true(is_dir(TEST_DATA_PATH "/existing-files"));
}

TEST(trailing_slash_is_ok)
{
	assert_true(is_dir(TEST_DATA_PATH "/existing-files/"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
