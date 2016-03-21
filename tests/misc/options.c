#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* memcmp() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/ui/fileview.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/opt_handlers.h"

#include "utils.h"

static void print_func(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[]);
static void format_none(int id, const void *data, size_t buf_len, char buf[]);

static int ncols;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	lwin.dir_entry = NULL;
	lwin.list_rows = 0;
	lwin.window_rows = 1;
	lwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(&lwin, lwin.sort);
	lwin.columns = columns_create();
	lwin.view_columns = strdup("");
	lwin.num_width_g = 4;
	lwin.num_width = 4;
	lwin.ls_view = 0;
	update_string(&lwin.sort_groups, "");
	update_string(&lwin.sort_groups_g, "");

	rwin.dir_entry = NULL;
	rwin.list_rows = 0;
	rwin.window_rows = 1;
	rwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(&rwin, rwin.sort);
	rwin.columns = columns_create();
	rwin.view_columns = strdup("");
	rwin.num_width_g = 4;
	rwin.num_width = 4;
	rwin.ls_view = 0;

	/* Name+size matches default column view setting ("-{name},{}"). */
	columns_add_column_desc(SK_BY_NAME, &format_none);
	columns_add_column_desc(SK_BY_SIZE, &format_none);
	columns_set_line_print_func(&print_func);

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();

	reset_cmds();

	columns_free(lwin.columns);
	lwin.columns = NULL;
	update_string(&lwin.view_columns, NULL);
	update_string(&lwin.sort_groups, NULL);
	update_string(&lwin.sort_groups_g, NULL);

	columns_free(rwin.columns);
	rwin.columns = NULL;
	update_string(&rwin.view_columns, NULL);
}

static void
print_func(const void *data, int column_id, const char buf[], size_t offset,
		AlignType align, const char full_column[])
{
	ncols += (column_id != FILL_COLUMN_ID);
}

static void
format_none(int id, const void *data, size_t buf_len, char buf[])
{
	buf[0] = '\0';
}

TEST(lsview_block_columns_update_on_sort_change)
{
	assert_success(exec_commands("set viewcolumns=", curr_view, CIT_COMMAND));
	assert_success(exec_commands("set lsview", curr_view, CIT_COMMAND));
	assert_success(exec_commands("set sort=name", curr_view, CIT_COMMAND));
	/* The check is implicit, an assert will fail if view columns are updated. */
}

TEST(recovering_from_wrong_viewcolumns_value_works)
{
	/* Prepare state required for the test and ensure that it's correct (default
	 * columns). */
	assert_success(exec_commands("set viewcolumns={name}", curr_view, CIT_COMMAND));
	assert_success(exec_commands("set viewcolumns=", curr_view, CIT_COMMAND));
	ncols = 0;
	columns_format_line(curr_view->columns, NULL, 100);
	assert_int_equal(2, ncols);

	/* Recovery after wrong string to default state should be done correctly. */
	assert_failure(exec_commands("set viewcolumns=#4$^", curr_view, CIT_COMMAND));

	ncols = 0;
	columns_format_line(curr_view->columns, NULL, 100);
	assert_int_equal(2, ncols);

	assert_string_equal("", get_option_value("viewcolumns", OPT_LOCAL));
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

TEST(fails_to_set_sort_group_with_wrong_regexp)
{
	assert_failure(exec_commands("set sortgroups=*", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sortgroups=.*,*", &lwin, CIT_COMMAND));
}

TEST(classify_parsing_of_types)
{
	const char decorations[FT_COUNT][2][9] = {
		[FT_DIR][DECORATION_PREFIX][0] = '-',
		[FT_DIR][DECORATION_SUFFIX][0] = '1',
		[FT_REG][DECORATION_PREFIX][0] = '*',
		[FT_REG][DECORATION_SUFFIX][0] = '/',
	};

	assert_success(exec_commands("set classify=*:reg:/,-:dir:1", &lwin,
				CIT_COMMAND));

	assert_int_equal(0,
			memcmp(&cfg.decorations, &decorations, sizeof(cfg.decorations)));
	assert_int_equal(0, cfg.name_dec_count);

	assert_string_equal("-:dir:1,*:reg:/",
			get_option_value("classify", OPT_GLOBAL));
}

TEST(classify_parsing_of_exprs)
{
	const char decorations[FT_COUNT][2][9] = {};

	assert_success(
			exec_commands("set classify=*::{*.c}::/,b::/.*-.*/i::q,-::*::1", &lwin,
				CIT_COMMAND));

	assert_int_equal(0,
			memcmp(&cfg.decorations, &decorations, sizeof(cfg.decorations)));
	assert_int_equal(3, cfg.name_dec_count);
	assert_string_equal("*", cfg.name_decs[0].prefix);
	assert_string_equal("/", cfg.name_decs[0].suffix);
	assert_string_equal("b", cfg.name_decs[1].prefix);
	assert_string_equal("q", cfg.name_decs[1].suffix);
	assert_string_equal("-", cfg.name_decs[2].prefix);
	assert_string_equal("1", cfg.name_decs[2].suffix);

	assert_string_equal("*::{*.c}::/,b::/.*-.*/i::q,-::*::1",
			get_option_value("classify", OPT_GLOBAL));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
