#include <stic.h>

#include <unistd.h> /* unlink() */

#include <stdio.h> /* fopen() fclose() */
#include <string.h> /* strcmp() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/int/file_magic.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"

static void check_empty_file(const char fname[]);
static int has_mime_type_detection_and_symlinks(void);
static int has_mime_type_detection_and_can_test_cache(void);
static int has_mime_type_detection(void);
static int has_no_mime_type_detection(void);

TEST(escaping_for_determining_mime_type, IF(has_mime_type_detection))
{
	check_empty_file(SANDBOX_PATH "/start'end");
	check_empty_file(SANDBOX_PATH "/start\"end");
	check_empty_file(SANDBOX_PATH "/start`end");
}

TEST(mimetype_cache_can_be_invalidated,
		IF(has_mime_type_detection_and_can_test_cache))
{
	copy_file(TEST_DATA_PATH "/read/very-long-line", SANDBOX_PATH "/file");
	assert_string_equal("text/plain", get_mimetype(SANDBOX_PATH "/file", 0));

	copy_file(TEST_DATA_PATH "/read/binary-data", SANDBOX_PATH "/file");
	reset_timestamp(SANDBOX_PATH "/file");
	assert_false(strcmp("text/plain", get_mimetype(SANDBOX_PATH "/file", 0)) ==
			0);

	remove_file(SANDBOX_PATH "/file");
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
	assert_success(make_symlink(long_name, SANDBOX_PATH "/B/123.mp3"));
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

TEST(empty_magic_handlers_on_no_mime_type_detection,
		IF(has_no_mime_type_detection))
{
	copy_file(TEST_DATA_PATH "/read/very-long-line", SANDBOX_PATH "/file");

	assert_null(get_mimetype(SANDBOX_PATH "/file", /*resolve_symlinks=*/1));
	assoc_records_t handlers = get_magic_handlers(SANDBOX_PATH "/file");
	assert_int_equal(0, handlers.count);

	remove_file(SANDBOX_PATH "/file");
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
has_mime_type_detection_and_can_test_cache(void)
{
	/* There is something wrong with cache test specifically on AppVeyor. */
	return (env_get("APPVEYOR") == NULL && has_mime_type_detection());
}

static int
has_mime_type_detection(void)
{
	const char *text = get_mimetype(TEST_DATA_PATH "/read/two-lines", 0);
	if(text == NULL || strcmp(text, "text/plain") != 0)
	{
		return 0;
	}

	const char *binary = get_mimetype(TEST_DATA_PATH "/read/binary_data", 0);
	if(binary == NULL || strcmp(binary, "text/plain") == 0)
	{
		return 0;
	}

	return 1;
}

static int
has_no_mime_type_detection(void)
{
	return has_mime_type_detection() == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
