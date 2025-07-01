#include <stic.h>

#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/cmd_core.h"

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 0;
	lwin.selected_files = 1;

	cmds_init();
	modes_init();
	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	curr_view = NULL;
	other_view = NULL;

	vle_cmds_reset();
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(normal_command_exits_cmdline_mode)
{
	assert_true(vle_mode_is(NORMAL_MODE));

	assert_success(cmds_dispatch("normal :", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));

	assert_success(cmds_dispatch("normal /", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));

	assert_success(cmds_dispatch("normal =", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(normal_command_does_not_reset_selection)
{
	assert_success(cmds_dispatch(":normal! t", &lwin, CIT_COMMAND));
	assert_int_equal(0, lwin.selected_files);
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	assert_success(cmds_dispatch(":normal! vG\r", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	assert_success(cmds_dispatch(":normal! t", &lwin, CIT_COMMAND));
	assert_int_equal(1, lwin.selected_files);
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
