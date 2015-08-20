#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/commands.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

static void init_view(FileView *view);
static void free_view(FileView *view);

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	cfg.slow_fs_list = strdup("");

	init_view(&lwin);
	init_view(&rwin);
}

TEARDOWN()
{
	reset_cmds();

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	free_view(&lwin);
	free_view(&rwin);
}

static void
init_view(FileView *view)
{
	filter_init(&view->local_filter.filter, 1);
	filter_init(&view->manual_filter, 1);
	filter_init(&view->auto_filter, 1);

	view->dir_entry = NULL;
	view->list_rows = 0;

	view->window_rows = 1;
	view->sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(view, view->sort);
}

static void
free_view(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free(view->dir_entry[i].name);
	}
	dynarray_free(view->dir_entry);
}

TEST(sync_syncs_local_filter)
{
	other_view->curr_dir[0] = '\0';
	assert_true(change_directory(curr_view, ".") >= 0);
	populate_dir_list(curr_view, 0);
	local_filter_apply(curr_view, "a");

	assert_success(exec_commands("sync! location filters", curr_view,
				CIT_COMMAND));
	assert_string_equal("a", other_view->local_filter.filter.raw);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
