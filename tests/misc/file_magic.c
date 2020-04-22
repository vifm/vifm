#include <stic.h>

#include <unistd.h> /* symlink() unlink() */

#include <stdio.h> /* fopen() fclose() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/int/file_magic.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"

static void check_empty_file(const char fname[]);
static int has_mime_type_detection_and_symlinks(void);
static int has_mime_type_detection(void);

TEST(escaping_for_determining_mime_type, IF(has_mime_type_detection))
{
	check_empty_file(SANDBOX_PATH "/start'end");
	check_empty_file(SANDBOX_PATH "/start\"end");
	check_empty_file(SANDBOX_PATH "/start`end");
}

TEST(relatively_large_file_name_does_not_crash,
		IF(has_mime_type_detection_and_symlinks))
{
	char *const saved_cwd = save_cwd();

	char path[PATH_MAX + 1];

	const char *const long_name = SANDBOX_PATH "/A/111111111111111111111111111111"
		"11111111111222222222222222222222222222222222223333333333333333333333333333"
		"44444444444444444444444444444444444444444444444444444444444444444444444444"
		"4411111111111111111111111111111111111111111.mp3";

	assert_success(os_mkdir(SANDBOX_PATH "/A", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/B", 0700));

	create_file(long_name);

	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink(long_name, SANDBOX_PATH "/B/123.mp3"));
#endif

	assert_success(get_link_target_abs(SANDBOX_PATH "/B/123.mp3",
				SANDBOX_PATH "/B", path, sizeof(path)));

	assert_success(chdir(SANDBOX_PATH "/B"));
	assert_non_null(get_mimetype(get_last_path_component(path), 0));

	restore_cwd(saved_cwd);

	assert_success(unlink(long_name));
	assert_success(unlink(SANDBOX_PATH "/B/123.mp3"));
	assert_success(rmdir(SANDBOX_PATH "/B"));
	assert_success(rmdir(SANDBOX_PATH "/A"));
}

static void
check_empty_file(const char fname[])
{
	FILE *const f = fopen(fname, "w");
	if(f != NULL)
	{
		fclose(f);
		assert_non_null(get_mimetype(fname, 0));
		assert_success(unlink(fname));
	}
}

static int
has_mime_type_detection_and_symlinks(void)
{
	return not_windows() && has_mime_type_detection();
}

static int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
