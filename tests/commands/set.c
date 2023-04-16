#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/opt_handlers.h"

static void print_func(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	conf_setup();
	view_setup(&lwin);
	view_setup(&rwin);

	lwin.num_width_g = lwin.num_width = 4;
	lwin.columns = columns_create();
	rwin.num_width_g = rwin.num_width = 4;
	rwin.columns = columns_create();

	cmds_init();
	opt_handlers_setup();

	/* Name+size matches default column view setting ("-{name},{}"). */
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&print_func);
}

TEARDOWN()
{
	curr_view = NULL;
	other_view = NULL;

	conf_teardown();
	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_cmds_reset();
	opt_handlers_teardown();
}

TEST(set_local_sets_local_value)
{
	assert_success(cmds_dispatch("setlocal numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(4, lwin.num_width_g);
	assert_int_equal(2, lwin.num_width);
}

TEST(set_global_sets_global_value)
{
	assert_success(cmds_dispatch("setglobal numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.num_width_g);
	assert_int_equal(4, lwin.num_width);
}

TEST(set_sets_local_and_global_values)
{
	assert_success(cmds_dispatch("set numberwidth=2", &lwin, CIT_COMMAND));
	assert_int_equal(2, lwin.num_width_g);
	assert_int_equal(2, lwin.num_width);
}

TEST(fails_to_set_sort_group_with_wrong_regexp)
{
	assert_failure(cmds_dispatch("set sortgroups=*", &lwin, CIT_COMMAND));
	assert_failure(cmds_dispatch("set sortgroups=.*,*", &lwin, CIT_COMMAND));
}

TEST(global_local_always_updates_two_views)
{
	lwin.ls_view_g = lwin.ls_view = 1;
	rwin.ls_view_g = rwin.ls_view = 0;
	lwin.hide_dot_g = lwin.hide_dot = 0;
	rwin.hide_dot_g = rwin.hide_dot = 1;

	load_view_options(curr_view);

	curr_stats.global_local_settings = 1;
	assert_success(cmds_dispatch("set nodotfiles lsview", &lwin, CIT_COMMAND));
	assert_true(lwin.ls_view_g);
	assert_true(lwin.ls_view);
	assert_true(rwin.ls_view_g);
	assert_true(rwin.ls_view);
	assert_true(lwin.hide_dot_g);
	assert_true(lwin.hide_dot);
	assert_true(rwin.hide_dot_g);
	assert_true(rwin.hide_dot);
	curr_stats.global_local_settings = 0;
}

TEST(global_local_updates_regular_options_only_once)
{
	cfg.tab_stop = 0;

	curr_stats.global_local_settings = 1;
	assert_success(cmds_dispatch("set tabstop+=10", &lwin, CIT_COMMAND));
	assert_int_equal(10, cfg.tab_stop);
	curr_stats.global_local_settings = 0;
}

static void
print_func(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
