#include <stic.h>

#ifndef _WIN32

#include <sys/types.h>
#include <unistd.h> /* chdir() unlink() */

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"

static int get_gids(gid_t *gid1, gid_t *gid2);
static int can_change_group(void);

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	replace_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);
	curr_view = &lwin;
	view_setup(&lwin);
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
	view_teardown(&lwin);
}

TEST(file_group_is_changed, IF(can_change_group))
{
	char path[PATH_MAX + 1];
	struct stat s;

	gid_t gid1, gid2;
	if(get_gids(&gid1, &gid2) != 0)
	{
		return;
	}

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	assert_success(chdir(lwin.curr_dir));

	create_dir("dir");
	create_file("dir/chown-me");

	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "dir/chown-me", saved_cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	mark_selection_or_current(curr_view);
	fops_chown(0, 1, 0, gid1);
	assert_success(os_stat("dir/chown-me", &s));
	assert_true(s.st_gid == gid1);

	mark_selection_or_current(curr_view);
	fops_chown(0, 1, 0, gid2);
	assert_success(os_stat("dir/chown-me", &s));
	assert_true(s.st_gid == gid2);

	assert_success(unlink("dir/chown-me"));
	assert_success(rmdir("dir"));
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
can_change_group(void)
{
#ifdef __OpenBSD__
	/* Port system of OpenBSD mocks `chown` and breaks this test which works
	 * outside of ports. */
	return 0;
#else
	/* Only users that have more than one group can change group of a file
	 * twice. */
	return (getgroups(0, NULL) >= 2);
#endif
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
