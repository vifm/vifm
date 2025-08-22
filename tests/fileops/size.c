#include <stic.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* rmdir() unlink() */

#include <string.h> /* strcpy() */
#include <time.h> /* time() time_t */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"
#include "../../src/status.h"

static uint64_t wait_for_size(const char path[]);

SETUP()
{
	stats_init(&cfg);
	view_setup(&lwin);
	view_setup(&rwin);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	stats_reset(&cfg);
}

TEST(directory_size_is_calculated_in_bg)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	append_view_entry(&lwin, "various-sizes")->type = FT_DIR;
	lwin.dir_entry[0].selected = 1;

	fops_size_bg(&lwin, 0);
	assert_int_equal(73728, wait_for_size(TEST_DATA_PATH "/various-sizes"));
}

TEST(parent_dir_entry_triggers_calculation_of_current_dir)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/various-sizes");
	append_view_entry(&lwin, "..")->type = FT_DIR;

	fops_size_bg(&lwin, 0);
	assert_int_equal(73728, wait_for_size(TEST_DATA_PATH "/various-sizes"));
}

TEST(changed_directory_detected_on_size_calculation, IF(not_windows))
{
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0777));
	assert_success(os_mkdir(SANDBOX_PATH "/dir/subdir", 0777));

	FILE *f = fopen(SANDBOX_PATH "/dir/subdir/file", "w");
	if(f != NULL)
	{
		fputs("text", f);
		fclose(f);
	}

	strcpy(lwin.curr_dir, SANDBOX_PATH "/dir");
	append_view_entry(&lwin, "subdir")->type = FT_DIR;

	fops_size_bg(&lwin, 0);
	assert_int_equal(4, wait_for_size(SANDBOX_PATH "/dir/subdir"));

	assert_success(unlink(SANDBOX_PATH "/dir/subdir/file"));
	view_teardown(&lwin);
	view_setup(&lwin);
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	append_view_entry(&lwin, "dir")->type = FT_DIR;

	fops_size_bg(&lwin, 0);
	assert_int_equal(0, wait_for_size(SANDBOX_PATH "/dir"));

	assert_success(rmdir(SANDBOX_PATH "/dir/subdir"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(directory_size_is_not_recalculated)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	append_view_entry(&lwin, ".")->type = FT_DIR;
	lwin.dir_entry[0].selected = 1;

	fops_size_bg(&lwin, 0);
	assert_int_equal(73728, wait_for_size(TEST_DATA_PATH "/various-sizes"));

	const time_t ts = time(NULL) + 10;
	dcache_set_size_timestamp(TEST_DATA_PATH "/various-sizes", ts);

	fops_size_bg(&lwin, 0);
	wait_for_bg();

	assert_ulong_equal(ts,
			dcache_get_size_timestamp(TEST_DATA_PATH "/various-sizes"));
}

TEST(symlinks_to_dirs, IF(not_windows))
{
	create_dir(SANDBOX_PATH "/dir");

	char cwd[PATH_MAX + 1];
	get_cwd(cwd, sizeof(cwd));
	char dir[PATH_MAX + 1];
	make_abs_path(dir, sizeof(dir), TEST_DATA_PATH, "various-sizes", cwd);
	assert_success(make_symlink(dir, SANDBOX_PATH "/link"));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	populate_dir_list(&lwin, 0);
	assert_int_equal(2, lwin.list_rows);
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	fops_size_bg(&lwin, 0);
	assert_int_equal(0, wait_for_size(SANDBOX_PATH "/dir"));
	assert_int_equal(73728, wait_for_size(SANDBOX_PATH "/link"));

	assert_success(unlink(SANDBOX_PATH "/link"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

static uint64_t
wait_for_size(const char path[])
{
	wait_for_bg();

	time_t mtime = 10;
	uint64_t inode = DCACHE_UNKNOWN;
#ifndef _WIN32
	struct stat s;
	assert_success(os_stat(path, &s));
	mtime = s.st_mtime;
	inode = s.st_ino;
#endif

	uint64_t size;
	/* Tests are executed fast, so decrement mtime. */
	dcache_get_at(path, mtime - 10, inode, &size, NULL);
	return size;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
