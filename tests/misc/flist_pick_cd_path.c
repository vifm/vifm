#include <stic.h>

#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

#ifdef _WIN32
#define PREFIX "c:"
#else
#define PREFIX ""
#endif

TEST(absolute_path_dominates)
{
	int updir;
	char dir[PATH_MAX + 1];

	flist_pick_cd_path(&lwin, "something", PREFIX "/abs/path", &updir, dir,
			sizeof(dir));

	assert_false(updir);
	assert_string_equal(PREFIX "/abs/path", dir);
}

TEST(dash_chooses_previous_location)
{
	int updir;
	char dir[PATH_MAX + 1];

	update_string(&lwin.last_dir, "--last--");
	strcpy(lwin.curr_dir, "--cur--");

	flist_pick_cd_path(&lwin, ".", "-", &updir, dir, sizeof(dir));

	assert_false(updir);
	assert_string_equal("--last--", dir);
}

TEST(null_or_empty_chooses_home)
{
	int updir;
	char dir[PATH_MAX + 1];

	strcpy(cfg.home_dir, "--home--");
	strcpy(lwin.curr_dir, "--cur--");

	flist_pick_cd_path(&lwin, ".", "", &updir, dir, sizeof(dir));
	assert_false(updir);
	assert_string_equal("--home--", dir);

	flist_pick_cd_path(&lwin, ".", NULL, &updir, dir, sizeof(dir));
	assert_false(updir);
	assert_string_equal("--home--", dir);
}

TEST(double_in_current_view_dot_just_sets_updir)
{
	int updir;
	char dir[PATH_MAX + 1];

	dir[0] = '\0';

	flist_pick_cd_path(&lwin, lwin.curr_dir, "..", &updir, dir, sizeof(dir));
	assert_true(updir);
	assert_string_equal("", dir);
}

TEST(double_in_other_view_changes_dir)
{
	int updir;
	char dir[PATH_MAX + 1];

	dir[0] = '\0';

	flist_pick_cd_path(&lwin, "some path", "..", &updir, dir, sizeof(dir));
	assert_false(updir);
	assert_string_equal("some path/..", dir);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
