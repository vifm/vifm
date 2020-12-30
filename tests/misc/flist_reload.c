#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

static view_t *const view = &lwin;

SETUP()
{
	char cwd[PATH_MAX + 1];

	assert_success(chdir(SANDBOX_PATH));

	update_string(&cfg.slow_fs_list, "");

	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);

	view_setup(view);
	copy_str(view->curr_dir, sizeof(view->curr_dir), cwd);

	assert_success(os_mkdir("0", 0000));
	assert_success(os_mkdir("1", 0000));
	assert_success(os_mkdir("2", 0000));
	assert_success(os_mkdir("3", 0000));

	populate_dir_list(view, 0);
}

TEARDOWN()
{
	view_teardown(view);

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
