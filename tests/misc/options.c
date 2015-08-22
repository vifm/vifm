#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/utils/dynarray.h"
#include "../../src/commands.h"
#include "../../src/filelist.h"
#include "../../src/opt_handlers.h"

static void format_name(int id, const void *data, size_t buf_len, char buf[]);

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	cfg.slow_fs_list = strdup("");
	cfg.apropos_prg = strdup("");
	cfg.cd_path = strdup("");
	cfg.find_prg = strdup("");
	cfg.fuse_home = strdup("");
	cfg.time_format = strdup("+");
	cfg.vi_command = strdup("");
	cfg.vi_x_command = strdup("");
	cfg.ruler_format = strdup("");
	cfg.status_line = strdup("");
	cfg.grep_prg = strdup("");
	cfg.locate_prg = strdup("");
	cfg.border_filler = strdup("");
	cfg.shell = strdup("");

	lwin.dir_entry = NULL;
	lwin.list_rows = 0;
	lwin.window_rows = 1;
	lwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(&lwin, lwin.sort);
	lwin.columns = columns_create();
	lwin.view_columns = strdup("");
	lwin.num_width_g = 4;
	lwin.num_width = 4;

	rwin.dir_entry = NULL;
	rwin.list_rows = 0;
	rwin.window_rows = 1;
	rwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(&rwin, rwin.sort);
	rwin.columns = columns_create();
	rwin.view_columns = strdup("");
	rwin.num_width_g = 4;
	rwin.num_width = 4;

	columns_add_column_desc(SK_BY_NAME, &format_name);

	init_option_handlers();
}

TEARDOWN()
{
	reset_cmds();
	clear_options();

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	free(cfg.apropos_prg);
	free(cfg.cd_path);
	free(cfg.find_prg);
	free(cfg.fuse_home);
	free(cfg.time_format);
	free(cfg.vi_command);
	free(cfg.vi_x_command);
	free(cfg.ruler_format);
	free(cfg.status_line);
	free(cfg.grep_prg);
	free(cfg.locate_prg);
	free(cfg.border_filler);
	free(cfg.shell);

	columns_free(lwin.columns);
	lwin.columns = NULL;
	free(lwin.view_columns);
	lwin.view_columns = NULL;

	columns_free(rwin.columns);
	rwin.columns = NULL;
	free(rwin.view_columns);
	rwin.view_columns = NULL;
}

static void
format_name(int id, const void *data, size_t buf_len, char buf[])
{
}

TEST(lsview_block_columns_update_on_sort_change)
{
	assert_success(exec_commands("set viewcolumns=", curr_view, CIT_COMMAND));
	assert_success(exec_commands("set lsview", curr_view, CIT_COMMAND));
	assert_success(exec_commands("set sort=name", curr_view, CIT_COMMAND));
	/* The check is implicit, an assert will fail if view columns are updated. */
}

TEST(set_local_sets_local_value)
{
	assert_success(exec_commands("setlocal numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(4, lwin.num_width_g);
	assert_int_equal(2, lwin.num_width);
}

TEST(set_global_sets_global_value)
{
	assert_success(exec_commands("setglobal numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.num_width_g);
	assert_int_equal(4, lwin.num_width);
}

TEST(set_sets_local_and_global_values)
{
	assert_success(exec_commands("set numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.num_width_g);
	assert_int_equal(2, lwin.num_width);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
