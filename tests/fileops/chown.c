#include <stic.h>

#ifndef _WIN32

#include <sys/types.h>
#include <unistd.h> /* chdir() unlink() */

#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"

static int get_gids(gid_t *gid1, gid_t *gid2);
static int has_more_than_one_group(void);

SETUP()
{
	replace_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);
	curr_view = &lwin;
}

TEST(file_group_is_changed, IF(has_more_than_one_group))
{
	FILE *f;

	int i;
	struct stat s;

	gid_t gid1, gid2;
	if(get_gids(&gid1, &gid2) != 0)
	{
		return;
	}

	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	assert_success(chdir(lwin.curr_dir));

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	f = fopen(SANDBOX_PATH "/dir/chown-me", "w");
	if(f != NULL)
	{
		fclose(f);
	}

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, SANDBOX_PATH "/dir/chown-me");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	mark_selection_or_current(curr_view);
	fops_chown(0, 1, 0, gid1);
	assert_success(os_stat(SANDBOX_PATH "/dir/chown-me", &s));
	assert_true(s.st_gid == gid1);

	mark_selection_or_current(curr_view);
	fops_chown(0, 1, 0, gid2);
	assert_success(os_stat(SANDBOX_PATH "/dir/chown-me", &s));
	assert_true(s.st_gid == gid2);

	for(i = 0; i < lwin.list_rows; ++i)
	{
		fentry_free(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);

	filter_dispose(&lwin.local_filter.filter);

	assert_success(unlink(SANDBOX_PATH "/dir/chown-me"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

static int
get_gids(gid_t *gid1, gid_t *gid2)
{
	const int size = getgroups(0, NULL);
	if(size > 0)
	{
		gid_t list[size];
		if(getgroups(size, &list[0]) != size)
		{
			return 1;
		}
		*gid1 = list[0];
		*gid2 = list[1];
		return 0;
	}
	else
	{
		return 1;
	}
}

static int
has_more_than_one_group(void)
{
	return (getgroups(0, NULL) >= 2);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
