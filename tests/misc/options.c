#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* memcmp() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/gmux.h"
#include "../../src/utils/shmem.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/opt_handlers.h"
#include "../../src/registers.h"

static void print_func(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info);

static int ncols;

SETUP()
{
	init_commands();

	view_setup(&lwin);

	lwin.columns = columns_create();
	lwin.num_width_g = lwin.num_width = 4;
	lwin.hide_dot_g = lwin.hide_dot = 1;
	curr_view = &lwin;

	view_setup(&rwin);
	rwin.columns = columns_create();
	rwin.num_width_g = rwin.num_width = 4;
	rwin.hide_dot_g = rwin.hide_dot = 1;
	other_view = &rwin;

	/* Name+size matches default column view setting ("-{name},{}"). */
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&print_func);

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();

	vle_cmds_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_free(rwin.columns);
	rwin.columns = NULL;

	columns_teardown();
}

static void
print_func(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	ncols += (info->id != FILL_COLUMN_ID);
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

	assert_string_equal("", vle_opts_get("viewcolumns", OPT_LOCAL));
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
	assert_success(vle_opts_set("dotfiles?", OPT_GLOBAL));
	assert_success(vle_opts_set("dotfiles?", OPT_LOCAL));
	load_view_options(&rwin);
	assert_success(vle_opts_set("dotfiles?", OPT_GLOBAL));
	assert_success(vle_opts_set("dotfiles?", OPT_LOCAL));
	assert_string_equal("nodotfiles\nnodotfiles\nnodotfiles\nnodotfiles",
			vle_tb_get_data(vle_err));

	rwin.sort_g[0] = SK_BY_NAME;

	assert_success(exec_commands("setlocal dotfiles", &rwin, CIT_COMMAND));
	reset_local_options(&rwin);
	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("dotfiles?", OPT_LOCAL));
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
	assert_string_equal("Pg", vle_opts_get("caseoptions", OPT_GLOBAL));
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
	(void)replace_string(&cfg.vborder_filler, "x");
	(void)replace_string(&cfg.hborder_filler, "y");
	assert_success(exec_commands("set fillchars=vborder:a,hborder:b", &lwin,
				CIT_COMMAND));
	assert_string_equal("a", cfg.vborder_filler);
	assert_string_equal("b", cfg.hborder_filler);
	update_string(&cfg.vborder_filler, NULL);
	update_string(&cfg.hborder_filler, NULL);
}

TEST(fillchars_not_changed_on_wrong_input)
{
	(void)replace_string(&cfg.vborder_filler, "x");
	assert_failure(exec_commands("set fillchars=vorder:a", &lwin, CIT_COMMAND));
	assert_string_equal("x", cfg.vborder_filler);
	update_string(&cfg.vborder_filler, NULL);
}

TEST(values_in_fillchars_are_deduplicated)
{
	(void)replace_string(&cfg.vborder_filler, "x");

	assert_success(exec_commands("set fillchars=vborder:a", &lwin, CIT_COMMAND));
	assert_success(exec_commands("set fillchars+=vborder:b", &lwin, CIT_COMMAND));
	assert_string_equal("b", cfg.vborder_filler);
	update_string(&cfg.vborder_filler, NULL);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("fillchars?", OPT_GLOBAL));
	assert_string_equal("  fillchars=vborder:b,hborder:",
			vle_tb_get_data(vle_err));

	update_string(&cfg.vborder_filler, NULL);
}

TEST(fillchars_can_be_reset)
{
	assert_success(exec_commands("set fillchars=vborder:v,hborder:h", &lwin,
				CIT_COMMAND));

	assert_success(exec_commands("set fillchars=vborder:v", &lwin, CIT_COMMAND));
	assert_string_equal("v", cfg.vborder_filler);
	assert_string_equal("", cfg.hborder_filler);

	assert_success(exec_commands("set fillchars=hborder:h", &lwin, CIT_COMMAND));
	assert_string_equal(" ", cfg.vborder_filler);
	assert_string_equal("h", cfg.hborder_filler);

	assert_success(exec_commands("set fillchars=", &lwin, CIT_COMMAND));
	assert_string_equal(" ", cfg.vborder_filler);
	assert_string_equal("", cfg.hborder_filler);

	update_string(&cfg.vborder_filler, NULL);
	update_string(&cfg.hborder_filler, NULL);
}

TEST(sizefmt_is_set_on_correct_input)
{
	assert_success(exec_commands("set sizefmt=units:si,precision:1", &lwin,
				CIT_COMMAND));

	cfg.sizefmt.base = -1;
	cfg.sizefmt.precision = -1;

	assert_success(exec_commands("set sizefmt=units:iec", &lwin, CIT_COMMAND));

	assert_int_equal(1024, cfg.sizefmt.base);
	assert_int_equal(0, cfg.sizefmt.precision);
	assert_int_equal(1, cfg.sizefmt.space);

	assert_success(exec_commands("set sizefmt=units:si,precision:1,space", &lwin,
				CIT_COMMAND));

	assert_int_equal(1000, cfg.sizefmt.base);
	assert_int_equal(1, cfg.sizefmt.precision);
	assert_int_equal(1, cfg.sizefmt.space);

	assert_success(exec_commands("set sizefmt=units:iec,precision:2,nospace",
				&lwin, CIT_COMMAND));

	assert_int_equal(1024, cfg.sizefmt.base);
	assert_int_equal(2, cfg.sizefmt.precision);
	assert_int_equal(0, cfg.sizefmt.space);
}

TEST(sizefmt_not_changed_on_wrong_input)
{
	cfg.sizefmt.base = -1;
	cfg.sizefmt.precision = -1;
	cfg.sizefmt.space = -1;

	assert_failure(exec_commands("set sizefmt=wrong", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=units:wrong", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=precision:0", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=precision:,units:si", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=space", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("set sizefmt=nospace", &lwin, CIT_COMMAND));

	assert_int_equal(-1, cfg.sizefmt.base);
	assert_int_equal(-1, cfg.sizefmt.precision);
	assert_int_equal(-1, cfg.sizefmt.space);
}

TEST(values_in_sizefmt_are_deduplicated)
{
	(void)replace_string(&cfg.vborder_filler, "x");

	assert_success(exec_commands("set sizefmt=units:si,space", &lwin, CIT_COMMAND));
	assert_success(exec_commands("set sizefmt+=nospace,units:iec,precision:10", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("sizefmt?", OPT_GLOBAL));
	assert_string_equal("  sizefmt=units:iec,precision:10,nospace",
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

	assert_failure(exec_commands("se milleroptions=rpreview:files", &lwin,
				CIT_COMMAND));
}

TEST(milleroptions_accepts_correct_input)
{
	assert_success(exec_commands("set milleroptions=csize:33,rsize:12,"
				"rpreview:all", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:33,rsize:12,rpreview:all",
			vle_tb_get_data(vle_err));
}

TEST(milleroptions_normalizes_input)
{
	assert_success(exec_commands("set milleroptions=lsize:-10,csize:133,"
				"rpreview:dirs", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:100,rsize:0,rpreview:dirs",
			vle_tb_get_data(vle_err));
}

TEST(lsoptions_empty_input)
{
	assert_success(exec_commands("set lsoptions=", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("lsoptions?", OPT_GLOBAL));
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
	assert_success(vle_opts_set("lsoptions?", OPT_GLOBAL));
	assert_string_equal("  lsoptions=transposed", vle_tb_get_data(vle_err));
}

TEST(lsoptions_normalizes_input)
{
	assert_success(exec_commands("set lsoptions=transposed,transposed", &lwin,
				CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("lsoptions?", OPT_GLOBAL));
	assert_string_equal("  lsoptions=transposed", vle_tb_get_data(vle_err));
}

TEST(previewprg_updates_state_of_view)
{
	assert_success(exec_commands("setg previewprg=gcmd", &lwin, CIT_COMMAND));
	assert_success(exec_commands("setl previewprg=lcmd", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("previewprg?", OPT_GLOBAL));
	assert_success(vle_opts_set("previewprg?", OPT_LOCAL));
	assert_string_equal("  previewprg=gcmd\n  previewprg=lcmd",
			vle_tb_get_data(vle_err));
}

TEST(tuioptions)
{
	assert_success(vle_opts_set("tuioptions=", OPT_GLOBAL));
	assert_int_equal(0, cfg.ellipsis_position);
	assert_false(cfg.extra_padding);
	assert_false(cfg.side_borders_visible);
	assert_false(cfg.use_unicode_characters);
	assert_false(cfg.flexible_splitter);

	assert_success(vle_opts_set("tuioptions=pul", OPT_GLOBAL));
	assert_int_equal(-1, cfg.ellipsis_position);
	assert_true(cfg.extra_padding);
	assert_false(cfg.side_borders_visible);
	assert_true(cfg.use_unicode_characters);
	assert_false(cfg.flexible_splitter);

	assert_success(vle_opts_set("tuioptions+=sr", OPT_GLOBAL));
	assert_int_equal(1, cfg.ellipsis_position);
	assert_true(cfg.extra_padding);
	assert_true(cfg.side_borders_visible);
	assert_true(cfg.use_unicode_characters);
	assert_false(cfg.flexible_splitter);
	/* "l" shouldn't be in the value along with "r". */
	assert_string_equal("prsu", vle_opts_get("tuioptions", OPT_GLOBAL));

	assert_success(vle_opts_set("tuioptions+=vl", OPT_GLOBAL));
	assert_int_equal(-1, cfg.ellipsis_position);
	assert_true(cfg.extra_padding);
	assert_true(cfg.side_borders_visible);
	assert_true(cfg.use_unicode_characters);
	assert_true(cfg.flexible_splitter);
	/* "r" shouldn't be in the value along with "l". */
	assert_string_equal("lpsuv", vle_opts_get("tuioptions", OPT_GLOBAL));
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

TEST(shortmess)
{
	cfg.tail_tab_line_paths = 0;
	cfg.trunc_normal_sb_msgs = 0;
	cfg.shorten_title_paths = 0;
	cfg.short_term_mux_titles = 0;

	assert_success(exec_commands("set shortmess=Mp", &lwin, CIT_COMMAND));
	assert_false(cfg.tail_tab_line_paths);
	assert_true(cfg.short_term_mux_titles);
	assert_false(cfg.trunc_normal_sb_msgs);
	assert_true(cfg.shorten_title_paths);

	assert_success(exec_commands("set shortmess=TL", &lwin, CIT_COMMAND));
	assert_true(cfg.tail_tab_line_paths);
	assert_false(cfg.short_term_mux_titles);
	assert_true(cfg.trunc_normal_sb_msgs);
	assert_false(cfg.shorten_title_paths);
}

TEST(histcursor)
{
	cfg.ch_pos_on = 0;

	assert_success(exec_commands("set histcursor=startup", &lwin, CIT_COMMAND));
	assert_int_equal(CHPOS_STARTUP, cfg.ch_pos_on);

	assert_success(exec_commands("set histcursor=direnter,dirmark", &lwin,
				CIT_COMMAND));
	assert_int_equal(CHPOS_ENTER | CHPOS_DIRMARK, cfg.ch_pos_on);
}

TEST(quickview)
{
	assert_success(exec_commands("set quickview", &lwin, CIT_COMMAND));
	assert_true(curr_stats.preview.on);
	assert_success(exec_commands("set invquickview", &lwin, CIT_COMMAND));
	assert_false(curr_stats.preview.on);
}

TEST(syncregs)
{
	/* Put all the temporary files into sandbox to avoid issues with running tests
	 * from multiple accounts. */
	char sandbox[PATH_MAX + 1];
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", NULL);
	char *tmpdir_value = mock_env("TMPDIR", sandbox);

	assert_false(regs_sync_enabled());
	assert_success(exec_commands("set syncregs=test1", &lwin, CIT_COMMAND));
	assert_true(regs_sync_enabled());
	assert_success(exec_commands("set syncregs=test2", &lwin, CIT_COMMAND));
	assert_true(regs_sync_enabled());
	assert_success(exec_commands("set syncregs=test1", &lwin, CIT_COMMAND));
	assert_true(regs_sync_enabled());
	assert_success(exec_commands("set syncregs=", &lwin, CIT_COMMAND));
	assert_false(regs_sync_enabled());

	/* Make sure nothing is retained from the tests. */
	gmux_destroy(gmux_create("regs-test1"));
	shmem_destroy(shmem_create("regs-test1", 10, 10));
	gmux_destroy(gmux_create("regs-test2"));
	shmem_destroy(shmem_create("regs-test2", 10, 10));

	unmock_env("TMPDIR", tmpdir_value);
}

TEST(mediaprg, IF(not_windows))
{
	assert_success(exec_commands("set mediaprg=prg", &lwin, CIT_COMMAND));
	assert_string_equal("prg", cfg.media_prg);
	assert_success(exec_commands("set mediaprg=", &lwin, CIT_COMMAND));
	assert_string_equal("", cfg.media_prg);
}

TEST(shell)
{
	assert_success(exec_commands("set shell=/bin/bash", &lwin, CIT_COMMAND));
	assert_string_equal("/bin/bash", cfg.shell);
	assert_success(exec_commands("set sh=/bin/sh", &lwin, CIT_COMMAND));
	assert_string_equal("/bin/sh", cfg.shell);
}

TEST(shellcmdflag)
{
	assert_success(exec_commands("set shellcmdflag=-ic", &lwin, CIT_COMMAND));
	assert_string_equal("-ic", cfg.shell_cmd_flag);
	assert_success(exec_commands("set shcf=-c", &lwin, CIT_COMMAND));
	assert_string_equal("-c", cfg.shell_cmd_flag);
}

TEST(tablabel)
{
	assert_success(exec_commands("set tabprefix={", &lwin, CIT_COMMAND));
	assert_success(exec_commands("set tablabel=%[(%n)%]%[%[%T{tree}%]{%c}@%]%p:t",
				&lwin, CIT_COMMAND));
	assert_success(exec_commands("set tabsuffix=}", &lwin, CIT_COMMAND));

	assert_string_equal("{", cfg.tab_prefix);
	assert_string_equal("%[(%n)%]%[%[%T{tree}%]{%c}@%]%p:t", cfg.tab_label);
	assert_string_equal("}", cfg.tab_suffix);
}

TEST(sessionoptions)
{
	assert_success(exec_commands("set sessionoptions=dhistory", &lwin,
				CIT_COMMAND));
	assert_int_equal(VINFO_DHISTORY, cfg.session_options);

	assert_success(exec_commands("set ssop=savedirs,tui", &lwin, CIT_COMMAND));
	assert_int_equal(VINFO_SAVEDIRS | VINFO_TUI, cfg.session_options);

	assert_success(exec_commands("set ssop=", &lwin, CIT_COMMAND));
	assert_int_equal(0, cfg.session_options);
}

TEST(previewoptions)
{
	assert_success(exec_commands("set previewoptions=graphicsdelay:12345", &lwin,
				CIT_COMMAND));
	assert_int_equal(12345, cfg.graphics_delay);
	assert_false(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);

	assert_failure(exec_commands("set previewoptions=graphicsdelay:inf", &lwin,
				CIT_COMMAND));
	assert_string_equal("Failed to parse \"graphicsdelay\" value: inf",
			vle_tb_get_data(vle_err));
	assert_failure(exec_commands("set previewoptions=maxtreedepth:inf", &lwin,
				CIT_COMMAND));
	assert_string_equal("Failed to parse \"maxtreedepth\" value: inf",
			vle_tb_get_data(vle_err));
	assert_int_equal(12345, cfg.graphics_delay);
	assert_false(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);

	assert_failure(exec_commands("set previewoptions=graphicsdelay:-12345", &lwin,
				CIT_COMMAND));
	assert_string_equal("\"graphicsdelay\" can't be negative, got: -12345",
			vle_tb_get_data(vle_err));
	assert_failure(exec_commands("set previewoptions=maxtreedepth:-1", &lwin,
				CIT_COMMAND));
	assert_string_equal("\"maxtreedepth\" can't be negative, got: -1",
			vle_tb_get_data(vle_err));
	assert_int_equal(12345, cfg.graphics_delay);
	assert_false(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);

	assert_failure(exec_commands("set previewoptions=graphicsdelay:145,wtf",
				&lwin, CIT_COMMAND));
	assert_int_equal(12345, cfg.graphics_delay);
	assert_false(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);
	assert_string_equal("Unknown key for 'previewoptions' option: wtf",
			vle_tb_get_data(vle_err));

	assert_success(exec_commands("set previewoptions=hardgraphicsclear", &lwin,
				CIT_COMMAND));
	assert_int_equal(0, cfg.graphics_delay);
	assert_true(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);

	assert_success(exec_commands("set previewoptions=toptreestats", &lwin,
				CIT_COMMAND));
	assert_true(cfg.top_tree_stats);
	assert_int_equal(0, cfg.graphics_delay);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.hard_graphics_clear);

	assert_success(exec_commands("set previewoptions=maxtreedepth:10", &lwin,
				CIT_COMMAND));
	assert_false(cfg.top_tree_stats);
	assert_int_equal(0, cfg.graphics_delay);
	assert_int_equal(10, cfg.max_tree_depth);
	assert_false(cfg.hard_graphics_clear);

	assert_success(exec_commands("set previewoptions=", &lwin, CIT_COMMAND));
	assert_int_equal(0, cfg.graphics_delay);
	assert_false(cfg.hard_graphics_clear);
	assert_int_equal(0, cfg.max_tree_depth);
	assert_false(cfg.top_tree_stats);
}

TEST(autocd)
{
	assert_success(exec_commands("set autocd", &lwin, CIT_COMMAND));
	assert_true(cfg.auto_cd);
	assert_success(exec_commands("set noautocd", &lwin, CIT_COMMAND));
	assert_false(cfg.auto_cd);
}

TEST(iooptions)
{
	assert_success(exec_commands("set iooptions=fastfilecloning", &lwin,
				CIT_COMMAND));
	assert_true(cfg.fast_file_cloning);
	assert_false(cfg.data_sync);

	assert_success(exec_commands("set iooptions=datasync", &lwin, CIT_COMMAND));
	assert_false(cfg.fast_file_cloning);
	assert_true(cfg.data_sync);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
