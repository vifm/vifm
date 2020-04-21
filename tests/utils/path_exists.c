#include <stic.h>

#include <unistd.h> /* unlink() */

#include <stdio.h> /* FILE fclose() fopen() */

#include <test-utils.h>

#include "../../src/utils/fs.h"

TEST(empty_string_null_returned)
{
	FILE *const f = fopen(SANDBOX_PATH "/file", "w");
	fclose(f);

	assert_true(path_exists(SANDBOX_PATH "/file", DEREF));
	assert_true(path_exists(SANDBOX_PATH "/file", NODEREF));
	if(not_windows())
	{
		assert_false(path_exists(SANDBOX_PATH "/file\\", DEREF));
		assert_false(path_exists(SANDBOX_PATH "/file\\", NODEREF));
	}

	assert_success(unlink(SANDBOX_PATH "/file"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
