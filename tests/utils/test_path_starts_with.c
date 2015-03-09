#include <stic.h>

#include "../../src/utils/path.h"

TEST(yes)
{
	assert_true(path_starts_with("/home/trash", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash/"));
	assert_true(path_starts_with("/home/trash", "/home/trash/"));
}

TEST(no)
{
	assert_false(path_starts_with("/home/tras", "/home/trash"));
	assert_false(path_starts_with("/home/trash_", "/home/trash"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
