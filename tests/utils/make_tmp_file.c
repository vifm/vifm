#include <stic.h>

#include <errno.h> /* EINVAL errno */
#include <stdio.h> /* FILE fputs() fclose() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"

TEST(make_tmp_file_no_auto_delete)
{
	char path[] = SANDBOX_PATH "/tmp-XXXXXX";
	FILE *fp = make_tmp_file(path, 0600, /*auto_delete=*/0);
	assert_non_null(fp);
	assert_false(ends_with(path, "XXXXXX"));

	fputs("line1\n", fp);
	fputs("line2\n", fp);
	fclose(fp);

	const char *lines[] = { "line1", "line2" };
	file_is(path, lines, 2);
	remove_file(path);
}

TEST(make_tmp_file_auto_delete)
{
	char path[] = SANDBOX_PATH "/tmp-XXXXXX";
	FILE *fp = make_tmp_file(path, 0000, /*auto_delete=*/1);
	assert_non_null(fp);
	fclose(fp);
	no_remove_file(path);
}

TEST(make_tmp_file_bad_pattern)
{
	char path[] = SANDBOX_PATH "/tmp-XXXXX";
	assert_null(make_tmp_file(path, 0000, /*auto_delete=*/0));
	assert_int_equal(EINVAL, errno);
}

TEST(make_file_in_tmp_slash_prefix)
{
	char buf[PATH_MAX + 1];
	assert_null(make_file_in_tmp("b/a/d", 0000, /*auto_delete=*/0, buf,
				sizeof(buf)));
	assert_int_equal(EINVAL, errno);
}

TEST(make_file_in_tmp_tiny_buffer)
{
	char buf[5];
	assert_null(make_file_in_tmp("prefix", 0000, /*auto_delete=*/0, buf,
				sizeof(buf)));
	assert_int_equal(ERANGE, errno);
}

TEST(bad_location, IF(regular_unix_user))
{
	create_dir(SANDBOX_PATH "/ro-dir");
	assert_success(os_chmod(SANDBOX_PATH "/ro-dir", 0000));

	char path[] = SANDBOX_PATH "/ro-dir/tmp-XXXXXX";
	assert_null(make_tmp_file(path, 0000, /*auto_delete=*/0));
	assert_int_equal(EACCES, errno);

	remove_dir(SANDBOX_PATH "/ro-dir");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
