#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* memset() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

static FileView *const view = &lwin;

SETUP()
{
	char cwd[PATH_MAX];
	char *error;

	assert_success(chdir(SANDBOX_PATH));

	update_string(&cfg.slow_fs_list, "");

	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);

	copy_str(view->curr_dir, sizeof(view->curr_dir), cwd);

	assert_success(os_mkdir("0", 0000));
	assert_success(os_mkdir("1", 0000));
	assert_success(os_mkdir("2", 0000));
	assert_success(os_mkdir("3", 0000));

	filter_init(&view->local_filter.filter, 1);
	assert_non_null(view->manual_filter = matcher_alloc("", 0, 0, "", &error));
	filter_init(&view->auto_filter, 1);
	view->sort[0] = SK_BY_NAME;
	memset(&view->sort[1], SK_NONE, sizeof(view->sort) - 1);
	view->dir_entry = NULL;
	view->list_rows = 0;
	populate_dir_list(view, 0);
}

TEARDOWN()
{
	int i;

	for(i = 0; i < view->list_rows; i++)
		free(view->dir_entry[i].name);
	dynarray_free(view->dir_entry);

	filter_dispose(&view->auto_filter);
	matcher_free(view->manual_filter);
	view->manual_filter = NULL;
	filter_dispose(&view->local_filter.filter);

	(void)rmdir("0");
	(void)rmdir("1");
	(void)rmdir("2");
	(void)rmdir("3");

	update_string(&cfg.slow_fs_list, NULL);
}

TEST(exact_match_is_preferred)
{
	view->list_pos = 2;
	(void)rmdir("0");
	(void)rmdir("1");

	assert_string_equal("2", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, 1);
	assert_string_equal("2", view->dir_entry[view->list_pos].name);
}

TEST(closest_match_below_is_choosen)
{
	view->list_pos = 1;
	(void)rmdir("1");
	(void)rmdir("2");

	assert_string_equal("1", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, 1);
	assert_string_equal("3", view->dir_entry[view->list_pos].name);
}

TEST(closest_match_above_is_choosen)
{
	view->list_pos = 3;
	(void)rmdir("3");
	(void)rmdir("4");

	assert_string_equal("3", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, 1);
	assert_string_equal("2", view->dir_entry[view->list_pos].name);
}

TEST(selection_is_preserved)
{
	view->dir_entry[0].selected = 1;
	view->dir_entry[1].selected = 1;
	view->dir_entry[3].selected = 1;
	view->selected_files = 3;
	(void)rmdir("3");

	populate_dir_list(view, 1);
	assert_true(view->dir_entry[0].selected);
	assert_true(view->dir_entry[1].selected);
	assert_int_equal(2, view->selected_files);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
