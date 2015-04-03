#include <stic.h>

#include <string.h> /* free() */

#include "../../src/cfg/config.h"
#include "../../src/commands.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	cfg.slow_fs_list = strdup("");

	lwin.dir_entry = NULL;
	lwin.list_rows = 0;
	lwin.window_rows = 1;
	lwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(lwin.sort);

	rwin.dir_entry = NULL;
	rwin.list_rows = 0;
	rwin.window_rows = 1;
	rwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(rwin.sort);
}

TEARDOWN()
{
	int i;

	reset_cmds();

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	free(rwin.dir_entry);
}

TEST(sync_all_syncs_local_filter)
{
	other_view->curr_dir[0] = '\0';
	assert_true(change_directory(curr_view, ".") >= 0);
	populate_dir_list(curr_view, 0);
	local_filter_apply(curr_view, "a");

	assert_success(exec_commands("sync! all", curr_view, CIT_COMMAND));
	assert_string_equal("a", other_view->local_filter.filter.raw);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
