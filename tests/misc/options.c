#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* memcmp() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
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
	lwin.num_width_g = lwin.num_width = 4;
	lwin.ls_view_g = lwin.ls_view = 0;
	lwin.hide_dot_g = lwin.hide_dot = 1;
	update_string(&lwin.sort_groups, "");
	update_string(&lwin.sort_groups_g, "");

	rwin.dir_entry = NULL;
	rwin.list_rows = 0;
	rwin.window_rows = 1;
	rwin.sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(&rwin, rwin.sort);
	rwin.columns = columns_create();
	rwin.view_columns = strdup("");
	rwin.num_width_g = rwin.num_width = 4;
	rwin.ls_view_g = rwin.ls_view = 0;
	rwin.hide_dot_g = rwin.hide_dot = 1;

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

	columns_clear_column_descs();
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
	assert_success(exec_commands("set viewcolumns={name}", curr_view,
				CIT_COMMAND));
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

	assert_string_equal("-:dir:1,*:reg:/",
			get_option_value("classify", OPT_GLOBAL));
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
			get_option_value("classify", OPT_GLOBAL));
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

	assert_success(exec_commands("set classify=:dir:/,:link:@,:fifo:|", &lwin,
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

TEST(suggestoptions_all_values)
{
	cfg.sug.flags = 0;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_success(exec_commands("set suggestoptions=normal,visual,view,otherpane"
				",delay,keys,marks,registers,foldsubkeys", &lwin, CIT_COMMAND));

	assert_int_equal(SF_NORMAL | SF_VISUAL | SF_VIEW | SF_OTHERPANE | SF_DELAY |
			SF_KEYS | SF_MARKS | SF_REGISTERS | SF_FOLDSUBKEYS, cfg.sug.flags);
	assert_int_equal(5, cfg.sug.maxregfiles);
	assert_int_equal(500, cfg.sug.delay);
}

TEST(suggestoptions_wrong_value)
{
	cfg.sug.flags = 0;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_failure(exec_commands("set suggestoptions=asdf", &lwin, CIT_COMMAND));

	assert_int_equal(0, cfg.sug.flags);
	assert_int_equal(0, cfg.sug.maxregfiles);
	assert_int_equal(0, cfg.sug.delay);
}

TEST(suggestoptions_empty_value)
{
	assert_success(exec_commands("set suggestoptions=normal", &lwin,
				CIT_COMMAND));

	cfg.sug.flags = SF_NORMAL;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_success(exec_commands("set suggestoptions=", &lwin, CIT_COMMAND));

	assert_int_equal(0, cfg.sug.flags);
	assert_int_equal(5, cfg.sug.maxregfiles);
	assert_int_equal(500, cfg.sug.delay);
}

TEST(suggestoptions_registers_number)
{
	cfg.sug.flags = SF_NORMAL | SF_VISUAL | SF_VIEW | SF_OTHERPANE | SF_DELAY |
					SF_KEYS | SF_MARKS | SF_REGISTERS | SF_FOLDSUBKEYS;
	cfg.sug.maxregfiles = 4;

	assert_failure(exec_commands("set suggestoptions=registers:-4", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.maxregfiles);

	assert_failure(exec_commands("set suggestoptions=registers:0", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.maxregfiles);

	assert_success(exec_commands("set suggestoptions=registers:1", &lwin,
				CIT_COMMAND));

	assert_int_equal(1, cfg.sug.maxregfiles);
}

TEST(suggestoptions_delay_number)
{
	cfg.sug.delay = 4;

	assert_failure(exec_commands("set suggestoptions=delay:-4", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.delay);

	assert_success(exec_commands("set suggestoptions=delay:0", &lwin,
				CIT_COMMAND));
	assert_int_equal(0, cfg.sug.delay);

	assert_success(exec_commands("set suggestoptions=delay:100", &lwin,
				CIT_COMMAND));

	assert_int_equal(100, cfg.sug.delay);
}

TEST(dotfiles)
{
	assert_success(exec_commands("setlocal dotfiles", &lwin, CIT_COMMAND));
	assert_true(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);

	assert_success(exec_commands("setglobal nodotfiles", &lwin, CIT_COMMAND));
	assert_true(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);

	assert_success(exec_commands("set dotfiles", &lwin, CIT_COMMAND));
	assert_false(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);

	curr_stats.global_local_settings = 1;
	assert_success(exec_commands("set nodotfiles", &lwin, CIT_COMMAND));
	assert_true(lwin.hide_dot_g);
	assert_true(lwin.hide_dot);
	assert_true(rwin.hide_dot_g);
	assert_true(rwin.hide_dot);
	curr_stats.global_local_settings = 0;

	vle_tb_clear(vle_err);
	load_view_options(&lwin);
	assert_success(set_options("dotfiles?", OPT_GLOBAL));
	assert_success(set_options("dotfiles?", OPT_LOCAL));
	load_view_options(&rwin);
	assert_success(set_options("dotfiles?", OPT_GLOBAL));
	assert_success(set_options("dotfiles?", OPT_LOCAL));
	assert_string_equal("nodotfiles\nnodotfiles\nnodotfiles\nnodotfiles",
			vle_tb_get_data(vle_err));

	rwin.sort_g[0] = SK_BY_NAME;

	assert_success(exec_commands("setlocal dotfiles", &rwin, CIT_COMMAND));
	reset_local_options(&rwin);
	vle_tb_clear(vle_err);
	assert_success(set_options("dotfiles?", OPT_LOCAL));
	assert_string_equal("nodotfiles", vle_tb_get_data(vle_err));
}

TEST(global_local_always_updates_two_views)
{
	lwin.ls_view_g = lwin.ls_view = 1;
	rwin.ls_view_g = rwin.ls_view = 0;
	lwin.hide_dot_g = lwin.hide_dot = 0;
	rwin.hide_dot_g = rwin.hide_dot = 1;

	load_view_options(curr_view);

	curr_stats.global_local_settings = 1;
	assert_success(exec_commands("set nodotfiles lsview", &lwin, CIT_COMMAND));
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
	assert_success(exec_commands("set tabstop+=10", &lwin, CIT_COMMAND));
	assert_int_equal(10, cfg.tab_stop);
	curr_stats.global_local_settings = 0;
}

TEST(caseoptions_are_normalized)
{
	assert_success(exec_commands("set caseoptions=pPGg", &lwin, CIT_COMMAND));
	assert_string_equal("Pg", get_option_value("caseoptions", OPT_GLOBAL));
	assert_int_equal(CO_GOTO_FILE | CO_PATH_COMPL, cfg.case_override);
	assert_int_equal(CO_GOTO_FILE, cfg.case_ignore);

	cfg.case_ignore = 0;
	cfg.case_override = 0;
}

TEST(range_in_wordchars_are_inclusive)
{
	int i;

	assert_success(exec_commands("set wordchars=a-c,d", &lwin, CIT_COMMAND));

	for(i = 0; i < 255; ++i)
	{
		if(i != 'a' && i != 'b' && i != 'c' && i != 'd')
		{
			assert_false(cfg.word_chars[i]);
		}
	}

	assert_true(cfg.word_chars['a']);
	assert_true(cfg.word_chars['b']);
	assert_true(cfg.word_chars['c']);
	assert_true(cfg.word_chars['d']);
}

TEST(wrong_ranges_are_handled_properly)
{
	unsigned int i;
	char word_chars[sizeof(cfg.word_chars)];
	for(i = 0; i < sizeof(word_chars); ++i)
	{
		cfg.word_chars[i] = i;
		word_chars[i] = i;
	}

	/* Inversed range. */
	assert_failure(exec_commands("set wordchars=c-a", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));

	/* Inversed range with negative beginning. */
	assert_failure(exec_commands("set wordchars=\xff-10", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));

	/* Non single character range. */
	assert_failure(exec_commands("set wordchars=a-bc", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));

	/* Half-open ranges. */
	assert_failure(exec_commands("set wordchars=a-", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));
	assert_failure(exec_commands("set wordchars=-a", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));

	/* Bad numerical values. */
	assert_failure(exec_commands("set wordchars=-1", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));
	assert_failure(exec_commands("set wordchars=666", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));
	assert_failure(exec_commands("set wordchars=40-1000", &lwin, CIT_COMMAND));
	assert_success(memcmp(cfg.word_chars, word_chars, 256));
}

TEST(sorting_is_set_correctly_on_restart)
{
	lwin.sort[0] = SK_BY_NAME;
	ui_view_sort_list_ensure_well_formed(&lwin, lwin.sort);
	lwin.sort_g[0] = SK_BY_NAME;
	ui_view_sort_list_ensure_well_formed(&lwin, lwin.sort_g);

	curr_stats.restart_in_progress = 1;
	assert_success(exec_commands("set sort=+iname", &lwin, CIT_COMMAND));
	curr_stats.restart_in_progress = 0;

	assert_int_equal(SK_BY_INAME, lwin.sort[0]);
	assert_int_equal(SK_BY_INAME, lwin.sort_g[0]);
}

TEST(fillchars_is_set_on_correct_input)
{
	(void)replace_string(&cfg.border_filler, "x");
	assert_success(exec_commands("set fillchars=vborder:a", &lwin, CIT_COMMAND));
	assert_string_equal("a", cfg.border_filler);
	update_string(&cfg.border_filler, NULL);
}

TEST(fillchars_not_changed_on_wrong_input)
{
	(void)replace_string(&cfg.border_filler, "x");
	assert_failure(exec_commands("set fillchars=vorder:a", &lwin, CIT_COMMAND));
	assert_string_equal("x", cfg.border_filler);
	update_string(&cfg.border_filler, NULL);
}

TEST(values_in_fillchars_are_deduplicated)
{
	(void)replace_string(&cfg.border_filler, "x");

	assert_success(exec_commands("set fillchars=vborder:a", &lwin, CIT_COMMAND));
	assert_success(exec_commands("set fillchars+=vborder:b", &lwin, CIT_COMMAND));
	assert_string_equal("b", cfg.border_filler);
	update_string(&cfg.border_filler, NULL);

	vle_tb_clear(vle_err);
	assert_success(set_options("fillchars?", OPT_GLOBAL));
	assert_string_equal("  fillchars=vborder:b", vle_tb_get_data(vle_err));

	update_string(&cfg.border_filler, NULL);
}

TEST(sizefmt_is_set_on_correct_input)
{
	cfg.sizefmt.base = -1;
	cfg.sizefmt.precision = -1;

	assert_success(exec_commands("set sizefmt=units:iec", &lwin, CIT_COMMAND));

	assert_int_equal(1024, cfg.sizefmt.base);
	assert_int_equal(0, cfg.sizefmt.precision);

	assert_success(exec_commands("set sizefmt=units:si,precision:1", &lwin,
				CIT_COMMAND));

	assert_int_equal(1000, cfg.sizefmt.base);
	assert_int_equal(1, cfg.sizefmt.precision);
}

TEST(sizefmt_not_changed_on_wrong_input)
{
	cfg.sizefmt.base = -1;
	cfg.sizefmt.precision = -1;

	assert_failure(exec_commands("set sizefmt=wrong", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=units:wrong", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=precision:0", &lwin, CIT_COMMAND));

	assert_int_equal(-1, cfg.sizefmt.base);
	assert_int_equal(-1, cfg.sizefmt.precision);
}

TEST(values_in_sizefmt_are_deduplicated)
{
	(void)replace_string(&cfg.border_filler, "x");

	assert_success(exec_commands("set sizefmt=units:si", &lwin, CIT_COMMAND));
	assert_success(exec_commands("set sizefmt+=units:iec,precision:10", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("sizefmt?", OPT_GLOBAL));
	assert_string_equal("  sizefmt=units:iec,precision:10",
			vle_tb_get_data(vle_err));
}

TEST(millerview)
{
	assert_success(exec_commands("se millerview", &lwin, CIT_COMMAND));
	assert_success(exec_commands("se invmillerview", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setl millerview", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setl invmillerview", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setg millerview", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setg invmillerview", &lwin, CIT_COMMAND));
}

TEST(milleroptions_handles_wrong_input)
{
	assert_failure(exec_commands("se milleroptions=msi:1", &lwin, CIT_COMMAND));

	assert_failure(exec_commands("se milleroptions=lsize:a", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("se milleroptions=csize:a", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("se milleroptions=rsize:a", &lwin, CIT_COMMAND));

	assert_failure(exec_commands("se milleroptions=csize:0", &lwin, CIT_COMMAND));
}

TEST(milleroptions_accepts_correct_input)
{
	assert_success(exec_commands("set milleroptions=csize:33,rsize:12", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:33,rsize:12",
			vle_tb_get_data(vle_err));
}

TEST(milleroptions_normalizes_input)
{
	assert_success(exec_commands("set milleroptions=lsize:-10,csize:133", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:100,rsize:0",
			vle_tb_get_data(vle_err));
}

TEST(lsoptions_empty_input)
{
	assert_success(exec_commands("set lsoptions=", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("lsoptions?", OPT_GLOBAL));
	assert_string_equal("  lsoptions=", vle_tb_get_data(vle_err));
}

TEST(lsoptions_handles_wrong_input)
{
	assert_failure(exec_commands("se lsoptions=transposed:yes", &lwin,
				CIT_COMMAND));
	assert_failure(exec_commands("se lsoptions=transpose", &lwin,
				CIT_COMMAND));
}

TEST(lsoptions_accepts_correct_input)
{
	assert_success(exec_commands("set lsview lsoptions=transposed", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("lsoptions?", OPT_GLOBAL));
	assert_string_equal("  lsoptions=transposed", vle_tb_get_data(vle_err));
}

TEST(lsoptions_normalizes_input)
{
	assert_success(exec_commands("set lsoptions=transposed,transposed", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("lsoptions?", OPT_GLOBAL));
	assert_string_equal("  lsoptions=transposed", vle_tb_get_data(vle_err));
}

TEST(previewprg_updates_state_of_view)
{
	assert_success(exec_commands("setg previewprg=gcmd", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setl previewprg=lcmd", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(set_options("previewprg?", OPT_GLOBAL));
	assert_success(set_options("previewprg?", OPT_LOCAL));
	assert_string_equal("  previewprg=gcmd\n  previewprg=lcmd",
			vle_tb_get_data(vle_err));
}

TEST(tuioptions)
{
	assert_success(set_options("tuioptions=", OPT_GLOBAL));
	assert_false(cfg.extra_padding);
	assert_false(cfg.side_borders_visible);
	assert_false(cfg.use_unicode_characters);

	assert_success(set_options("tuioptions=pu", OPT_GLOBAL));
	assert_true(cfg.extra_padding);
	assert_false(cfg.side_borders_visible);
	assert_true(cfg.use_unicode_characters);

	assert_success(set_options("tuioptions+=s", OPT_GLOBAL));
	assert_true(cfg.extra_padding);
	assert_true(cfg.side_borders_visible);
	assert_true(cfg.use_unicode_characters);
}

TEST(setting_tabscope_works)
{
	assert_success(exec_commands("set tabscope=pane", &lwin, CIT_COMMAND));
	assert_true(cfg.pane_tabs);

	assert_success(exec_commands("set tabscope=global", &lwin, CIT_COMMAND));
	assert_false(cfg.pane_tabs);
}

TEST(setting_showtabline_works)
{
	assert_success(exec_commands("set showtabline=0", &lwin, CIT_COMMAND));
	assert_int_equal(STL_NEVER, cfg.show_tab_line);
	assert_success(exec_commands("set showtabline=never", &lwin, CIT_COMMAND));
	assert_int_equal(STL_NEVER, cfg.show_tab_line);

	assert_success(exec_commands("set showtabline=1", &lwin, CIT_COMMAND));
	assert_int_equal(STL_MULTIPLE, cfg.show_tab_line);
	assert_success(exec_commands("set showtabline=multiple", &lwin, CIT_COMMAND));
	assert_int_equal(STL_MULTIPLE, cfg.show_tab_line);

	assert_success(exec_commands("set showtabline=2", &lwin, CIT_COMMAND));
	assert_int_equal(STL_ALWAYS, cfg.show_tab_line);
	assert_success(exec_commands("set showtabline=always", &lwin, CIT_COMMAND));
	assert_int_equal(STL_ALWAYS, cfg.show_tab_line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
