#include <stic.h>

#include <unistd.h> /* chdir() */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static char *saved_cwd;

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	saved_cwd = save_cwd();

	view_setup(&lwin);
	modes_init();
	opt_handlers_setup();

	assert_success(chdir(TEST_DATA_PATH "/read"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	cfg_resize_histories(10);

	populate_dir_list(&lwin, /*reload=*/0);

	assert_int_equal(6, lwin.list_rows);
	assert_int_equal(0, lwin.list_pos);
}

TEARDOWN()
{
	cfg_resize_histories(0);

	opt_handlers_teardown();
	(void)vle_keys_exec_timed_out(WK_C_c);
	vle_keys_reset();
	view_teardown(&lwin);

	restore_cwd(saved_cwd);
}

TEST(n_works_without_prior_search_in_visual_mode)
{
	hists_search_save("asdfasdfasdf");

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_v WK_n);

	assert_string_equal("Search hit BOTTOM without match for: asdfasdfasdf",
			ui_sb_last());
	assert_int_equal(0, lwin.list_pos);

	hists_search_save(".");

	ui_sb_msg("");

	(void)vle_keys_exec_timed_out(WK_n);

	assert_string_equal("(2 of 6) /.", ui_sb_last());
	assert_int_equal(1, lwin.list_pos);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_false(lwin.dir_entry[2].selected);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
