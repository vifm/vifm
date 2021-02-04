#include <stic.h>

#include <stdio.h> /* remove() snprintf() */

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/fswatch.h"
#include "../../src/utils/path.h"

static int using_inotify(void);

static char sandbox[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	if(is_path_absolute(SANDBOX_PATH))
	{
		snprintf(sandbox, sizeof(sandbox), "%s", SANDBOX_PATH);
	}
	else
	{
		snprintf(sandbox, sizeof(sandbox), "%s/%s/", cwd, SANDBOX_PATH);
	}
}

TEST(watch_is_created_for_existing_directory)
{
	fswatch_t *watch;

	assert_non_null(watch = fswatch_create(sandbox));
	fswatch_free(watch);
}

TEST(two_watches_for_the_same_directory)
{
	fswatch_t *watch1, *watch2;

	assert_non_null(watch1 = fswatch_create(sandbox));
	assert_non_null(watch2 = fswatch_create(sandbox));
	fswatch_free(watch2);
	fswatch_free(watch1);
}

TEST(events_are_accumulated, IF(using_inotify))
{
	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(sandbox));

	os_mkdir(SANDBOX_PATH "/testdir", 0700);
	remove(SANDBOX_PATH "/testdir");
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));
	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	fswatch_free(watch);
}

TEST(started_as_not_changed)
{
	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(sandbox));

	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	fswatch_free(watch);
}

TEST(handles_several_events_in_a_row, IF(using_inotify))
{
	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(sandbox));

	os_mkdir(SANDBOX_PATH "/testdir", 0700);
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	remove(SANDBOX_PATH "/testdir");
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	fswatch_free(watch);
}

TEST(target_replacement_is_detected, IF(using_inotify))
{
	assert_success(os_mkdir(SANDBOX_PATH "/testdir", 0700));

	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(SANDBOX_PATH "/testdir"));

	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	assert_success(remove(SANDBOX_PATH "/testdir"));
	assert_success(os_mkdir(SANDBOX_PATH "/eatinode", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/testdir", 0700));

	assert_int_equal(FSWS_REPLACED, fswatch_poll(watch));
	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	fswatch_free(watch);

	assert_success(remove(SANDBOX_PATH "/testdir"));
	assert_success(remove(SANDBOX_PATH "/eatinode"));
}

TEST(target_mount_is_detected, IF(using_inotify))
{
	/* Mounting is simulated by renaming original directory, which gives similar
	 * effect as mounting that path. */

	assert_success(os_mkdir(SANDBOX_PATH "/testdir", 0700));

	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(SANDBOX_PATH "/testdir"));

	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	assert_success(rename(SANDBOX_PATH "/testdir", SANDBOX_PATH "/eatinode"));
	assert_success(os_mkdir(SANDBOX_PATH "/testdir", 0700));

	assert_int_equal(FSWS_REPLACED, fswatch_poll(watch));
	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	fswatch_free(watch);

	assert_success(remove(SANDBOX_PATH "/testdir"));
	assert_success(remove(SANDBOX_PATH "/eatinode"));
}

TEST(to_many_events_causes_banning_of_same_events, IF(using_inotify))
{
	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(sandbox));

	os_mkdir(SANDBOX_PATH "/testdir", 0700);

	int i;
	for(i = 0; i < 100; ++i)
	{
		os_chmod(SANDBOX_PATH "/testdir", 0777);
		os_chmod(SANDBOX_PATH "/testdir", 0000);
		(void)fswatch_poll(watch);
	}

	os_chmod(SANDBOX_PATH "/testdir", 0777);
	assert_int_equal(FSWS_UNCHANGED, fswatch_poll(watch));

	assert_success(remove(SANDBOX_PATH "/testdir"));
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	fswatch_free(watch);
}

TEST(file_recreation_removes_ban, IF(using_inotify))
{
	fswatch_t *watch;
	assert_non_null(watch = fswatch_create(sandbox));

	os_mkdir(SANDBOX_PATH "/testdir", 0700);

	int i;
	for(i = 0; i < 100; ++i)
	{
		os_chmod(SANDBOX_PATH "/testdir", 0777);
		os_chmod(SANDBOX_PATH "/testdir", 0000);
		(void)fswatch_poll(watch);
	}

	assert_success(remove(SANDBOX_PATH "/testdir"));
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	os_mkdir(SANDBOX_PATH "/testdir", 0700);
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	os_chmod(SANDBOX_PATH "/testdir", 0777);
	assert_int_equal(FSWS_UPDATED, fswatch_poll(watch));

	fswatch_free(watch);

	assert_success(remove(SANDBOX_PATH "/testdir"));
}

static int
using_inotify(void)
{
#ifdef HAVE_INOTIFY
	return 1;
#else
	return 0;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
