#include <stic.h>

#include <test-utils.h>

#include "../../src/engine/cmds.h"
#include "../../src/modes/dialogs/msg_dialog.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

static int failed_choosing(const char type[], const char title[],
		const char message[]);

SETUP()
{
	view_setup(&lwin);
	setup_grid(&lwin, /*column_count=*/1, /*list_rows=*/2, /*init=*/1);
	curr_view = &lwin;

	update_string(&lwin.dir_entry[0].name, "1");
	update_string(&lwin.dir_entry[1].name, "2");

	view_setup(&rwin);
	setup_grid(&rwin, /*column_count=*/1, /*list_rows=*/1, /*init=*/1);
	other_view = &rwin;

	cmds_init();
}

TEARDOWN()
{
	curr_view = NULL;
	other_view = NULL;

	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();
}

TEST(open_cmd_can_refuse_to_choose_multiple_files)
{
	curr_stats.choose_one = 1;
	stats_set_chosen_files_out("-");

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	dlg_set_callback(&failed_choosing);
	assert_success(cmds_dispatch1("open", &lwin, CIT_COMMAND));
}

TEST(edit_cmd_can_refuse_to_choose_multiple_files)
{
	curr_stats.choose_one = 1;
	stats_set_chosen_files_out("-");

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	dlg_set_callback(&failed_choosing);
	assert_success(cmds_dispatch1("edit", &lwin, CIT_COMMAND));
}

static int
failed_choosing(const char type[], const char title[], const char message[])
{
	assert_string_equal("error", type);
	assert_string_equal("File Choosing Error", title);
	assert_string_equal("A single-item choice was requested, but 2 items are "
			"selected.", message);

	dlg_set_callback(NULL);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
