#include <stic.h>

#include <string.h> /* memset() */

#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"

#include "utils.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	curr_view = &lwin;
	other_view = &rwin;
}

SETUP()
{
	view_setup(&lwin);
	init_modes();
	opt_handlers_setup();

	lwin.sort_g[0] = SK_BY_NAME;
	memset(&lwin.sort_g[1], SK_NONE, sizeof(lwin.sort_g) - 1);
}

TEARDOWN()
{
	view_teardown(&lwin);
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(sibl_navigate_correctly)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);

	(void)vle_keys_exec_timed_out(WK_RB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "rename", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"2" WK_RB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "scripts", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"3" WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "quotes-in-names", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"3" WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "color-schemes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_LB WK_R);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "various-sizes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_RB WK_R);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "color-schemes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
