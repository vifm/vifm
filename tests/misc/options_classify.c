#include <stic.h>

#include <limits.h> /* INT_MIN */
#include <string.h> /* memcmp() strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

SETUP()
{
	init_commands();

	view_setup(&lwin);
	view_setup(&rwin);
	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();

	vle_cmds_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(classify_parsing_of_types)
{
	const char type_decs[FT_COUNT][2][9] = {
		[FT_DIR][DECORATION_PREFIX][0] = '-',
		[FT_DIR][DECORATION_SUFFIX][0] = '1',
		[FT_REG][DECORATION_PREFIX][0] = '*',
		[FT_REG][DECORATION_SUFFIX][0] = '/',
	};

	assert_success(exec_commands("set classify=*:reg:/,-:dir:1", &lwin,
				CIT_COMMAND));

	assert_int_equal(0,
			memcmp(&cfg.type_decs, &type_decs, sizeof(cfg.type_decs)));
	assert_int_equal(0, cfg.name_dec_count);

	assert_string_equal("-:dir:1,*:reg:/", vle_opts_get("classify", OPT_GLOBAL));
}

TEST(classify_parsing_of_exprs)
{
	const char type_decs[FT_COUNT][2][9] = {};

	assert_success(
			exec_commands(
				"set classify=*::!{*.c}::/,123::*.c,,*.b::753,b::/.*-.*/i::q,-::*::1",
				&lwin, CIT_COMMAND));

	assert_int_equal(0,
			memcmp(&cfg.type_decs, &type_decs, sizeof(cfg.type_decs)));
	assert_int_equal(4, cfg.name_dec_count);
	assert_string_equal("*", cfg.name_decs[0].prefix);
	assert_string_equal("/", cfg.name_decs[0].suffix);
	assert_string_equal("123", cfg.name_decs[1].prefix);
	assert_string_equal("753", cfg.name_decs[1].suffix);
	assert_string_equal("b", cfg.name_decs[2].prefix);
	assert_string_equal("q", cfg.name_decs[2].suffix);
	assert_string_equal("-", cfg.name_decs[3].prefix);
	assert_string_equal("1", cfg.name_decs[3].suffix);

	assert_string_equal("*::!{*.c}::/,123::*.c,,*.b::753,b::/.*-.*/i::q,-::*::1",
			vle_opts_get("classify", OPT_GLOBAL));
}

TEST(classify_suffix_prefix_lengths)
{
	char type_decs[FT_COUNT][2][9];
	memcpy(&type_decs, &cfg.type_decs, sizeof(cfg.type_decs));

	assert_failure(exec_commands("set classify=123456789::{*.c}::/", &lwin,
				CIT_COMMAND));
	assert_int_equal(0,
			memcmp(&cfg.type_decs, &type_decs, sizeof(cfg.type_decs)));

	assert_failure(exec_commands("set classify=::{*.c}::123456789", &lwin,
				CIT_COMMAND));
	assert_int_equal(0,
			memcmp(&cfg.type_decs, &type_decs, sizeof(cfg.type_decs)));

	assert_success(exec_commands("set classify=12345678::{*.c}::12345678", &lwin,
				CIT_COMMAND));
}

TEST(classify_pattern_list)
{
	dir_entry_t entry = {
		.name = "binary-data",
		.origin = TEST_DATA_PATH "/test-data/read",
		.name_dec_num = -1,
	};

	const char *prefix, *suffix;

	assert_success(exec_commands("set classify=<::{*-data}{*-data}::>", &lwin,
				CIT_COMMAND));

	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("<", prefix);
	assert_string_equal(">", suffix);
}

TEST(classify_can_be_set_to_empty_value)
{
	dir_entry_t entry = {
		.name = "binary-data",
		.origin = TEST_DATA_PATH "/test-data/read",
		.name_dec_num = -1,
	};

	const char *prefix, *suffix;

	assert_success(exec_commands("set classify=", &lwin, CIT_COMMAND));

	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("", prefix);
	assert_string_equal("", suffix);
}

TEST(classify_account_assumes_trailing_slashes_for_dirs)
{
	dir_entry_t entry = {
		.name = "read",
		.type = FT_DIR,
		.origin = TEST_DATA_PATH,
		.name_dec_num = -1,
	};

	const char *prefix, *suffix;

	assert_success(exec_commands("set classify=[:dir:],*::*ad::@", &lwin,
				CIT_COMMAND));

	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("[", prefix);
	assert_string_equal("]", suffix);
}

TEST(classify_state_is_not_changed_if_format_is_wong)
{
	assert_success(exec_commands("set classify=*::*ad::@", &lwin, CIT_COMMAND));
	assert_int_equal(1, cfg.name_dec_count);
	assert_failure(exec_commands("set classify=*:*ad:@", &lwin, CIT_COMMAND));
	assert_int_equal(1, cfg.name_dec_count);
}

TEST(classify_does_not_stop_on_empty_prefix)
{
	dir_entry_t entry = {
		.name = "read",
		.origin = TEST_DATA_PATH,
		.name_dec_num = -1,
	};

	const char *prefix, *suffix;

	assert_success(exec_commands("set classify=:dir:/,:link:@,:fifo:\\|", &lwin,
				CIT_COMMAND));

	entry.type = FT_DIR;
	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("", prefix);
	assert_string_equal("/", suffix);

	entry.type = FT_LINK;
	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("", prefix);
	assert_string_equal("@", suffix);

	entry.type = FT_FIFO;
	ui_get_decors(&entry, &prefix, &suffix);
	assert_string_equal("", prefix);
	assert_string_equal("|", suffix);
}

TEST(changing_classify_invalidates_decors_cache)
{
	assert_success(stats_init(&cfg));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	strcpy(rwin.curr_dir, lwin.curr_dir);
	load_dir_list(&lwin, 1);
	load_dir_list(&rwin, 1);

	assert_success(exec_commands("set millerview milleroptions=lsize:1,rsize:1",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("set classify=::*.vifm::*\\|,::read::<>", &lwin,
				CIT_COMMAND));

	cfg.columns = 10;
	fview_setup();
	lwin.window_cols = 100;
	lwin.window_rows = 10;
	lwin.columns = columns_create();

	/* Fill dir_entry_t::name_dec_num by querying decorations. */
	int i;
	for(i = 0; i < lwin.list_rows; ++i)
	{
		const char *prefix, *suffix;
		ui_get_decors(&lwin.dir_entry[i], &prefix, &suffix);
	}

	tabs_new(NULL, NULL);

	(void)stats_update_fetch();
	curr_stats.load_stage = 2;
	redraw_view(&lwin);
	assert_success(exec_commands("set classify=", &lwin, CIT_COMMAND));
	curr_stats.load_stage = 0;

	/* Check that dir_entry_t::name_dec_num of inactive tab are reset. */
	tabs_goto(0);
	for(i = 0; i < lwin.list_rows; ++i)
	{
		assert_int_equal(-1, lwin.dir_entry[i].name_dec_num);
	}

	tabs_only(&lwin);
	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_teardown();
	cfg.columns = INT_MIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
