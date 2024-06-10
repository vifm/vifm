#include <stic.h>

#include "../../src/utils/path.h"

TEST(dotdirs_in_relative_paths_normalized)
{
	assert_true(paths_are_equal("a", "./a"));
	assert_true(paths_are_equal("./a", "././a"));
	assert_true(paths_are_equal("a", "./././a"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
