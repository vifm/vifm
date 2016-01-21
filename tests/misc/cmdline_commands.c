#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/marks.h"
#include "../../src/registers.h"

static void init_view(FileView *view);
static void free_view(FileView *view);

SETUP()
{
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;

	update_string(&cfg.slow_fs_list, "no");
	update_string(&cfg.fuse_home, "");

	init_view(&lwin);
	init_view(&rwin);
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	reset_cmds();

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

	filter_dispose(&view->local_filter.filter);
	filter_dispose(&view->manual_filter);
	filter_dispose(&view->auto_filter);

	for(i = 0; i < view->list_rows; ++i)
	{
		free_dir_entry(view, &view->dir_entry[i]);
	}
	dynarray_free(view->dir_entry);

	for(i = 0; i < view->custom.entry_count; ++i)
	{
		free_dir_entry(view, &view->custom.entries[i]);
	}
	dynarray_free(view->custom.entries);
	view->custom.entries = NULL;
	view->custom.entry_count = 0;
}

TEST(yank_works_with_ranges)
{
	reg_t *reg;

	init_registers();

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	reg = find_register(DEFAULT_REG_NAME);
	assert_non_null(reg);

	assert_int_equal(0, reg->nfiles);
	(void)exec_commands("%yank", &lwin, CIT_COMMAND);
	assert_int_equal(1, reg->nfiles);

	clear_registers();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
