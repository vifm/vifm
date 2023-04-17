#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdio.h> /* remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"

static void init_view(view_t *view);
static void free_view(view_t *view);

SETUP()
{
	assert_success(chdir(SANDBOX_PATH));

	curr_view = &lwin;
	other_view = &rwin;

	cmds_init();

	cfg.slow_fs_list = strdup("");
	cfg.chase_links = 1;

	init_view(&lwin);
	init_view(&rwin);
}

TEARDOWN()
{
	vle_cmds_reset();

	update_string(&cfg.slow_fs_list, NULL);

	cfg.chase_links = 0;

	free_view(&lwin);
	free_view(&rwin);
}

static void
init_view(view_t *view)
{
	char *error;

	filter_init(&view->local_filter.filter, 1);
	assert_non_null(view->manual_filter = matcher_alloc("", 0, 0, "", &error));
	filter_init(&view->auto_filter, 1);

	view->dir_entry = NULL;
	view->list_rows = 0;

	view->window_rows = 1;
	view->sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(view, view->sort);
}

static void
free_view(view_t *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free(view->dir_entry[i].name);
	}
	dynarray_free(view->dir_entry);

	filter_dispose(&view->local_filter.filter);
	matcher_free(view->manual_filter);
	view->manual_filter = NULL;
	filter_dispose(&view->auto_filter);
}

TEST(link_is_not_resolved_by_default, IF(not_windows))
{
	cfg.chase_links = 0;

	assert_success(os_mkdir("dir", 0700));
	assert_success(make_symlink("dir", "dir-link"));

	assert_non_null(get_cwd(curr_view->curr_dir, sizeof(curr_view->curr_dir)));
	assert_true(change_directory(curr_view, "dir-link") >= 0);
	assert_string_equal("dir-link", get_last_path_component(curr_view->curr_dir));

	/* Go out of the directory so that we can remove it. */
	assert_true(change_directory(curr_view, "..") >= 0);

	assert_success(rmdir("dir"));
	assert_success(remove("dir-link"));
}

TEST(chase_links_causes_link_to_be_resolved, IF(not_windows))
{
	assert_success(os_mkdir("dir", 0700));
	assert_success(make_symlink("dir", "dir-link"));

	assert_non_null(get_cwd(curr_view->curr_dir, sizeof(curr_view->curr_dir)));
	assert_true(change_directory(curr_view, "dir-link") >= 0);
	assert_string_equal("dir", get_last_path_component(curr_view->curr_dir));

	/* Go out of the directory so that we can remove it. */
	assert_true(change_directory(curr_view, "..") >= 0);

	assert_success(rmdir("dir"));
	assert_success(remove("dir-link"));
}

TEST(chase_links_is_not_affected_by_chdir, IF(not_windows))
{
	char pwd[PATH_MAX + 1];

	assert_success(os_mkdir("dir", 0700));
	assert_success(make_symlink("dir", "dir-link"));

	assert_non_null(get_cwd(pwd, sizeof(pwd)));
	strcpy(curr_view->curr_dir, pwd);

	assert_true(change_directory(curr_view, "dir-link") >= 0);
	assert_success(chdir(".."));
	assert_true(change_directory(curr_view, "..") >= 0);
	assert_true(paths_are_equal(curr_view->curr_dir, pwd));

	assert_success(rmdir("dir"));
	assert_success(remove("dir-link"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
