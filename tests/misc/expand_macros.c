#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h>
#include <string.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/flist_sel.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/status.h"

#ifdef _WIN32
#define SL "\\\\"
#else
#define SL "/"
#endif

static void
setup_lwin(void)
{
	view_setup(&lwin);
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 4;
	lwin.list_pos = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfi le0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("lfile1");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].name = strdup("lfile\"2");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[3].name = strdup("lfile3");
	lwin.dir_entry[3].origin = &lwin.curr_dir[0];

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 2;
}

static void
setup_rwin(void)
{
	view_setup(&rwin);
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 7;
	rwin.list_pos = 5;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	rwin.dir_entry[1].name = strdup("rfile1");
	rwin.dir_entry[1].origin = &rwin.curr_dir[0];
	rwin.dir_entry[2].name = strdup("rfile2");
	rwin.dir_entry[2].origin = &rwin.curr_dir[0];
	rwin.dir_entry[3].name = strdup("rfile3");
	rwin.dir_entry[3].origin = &rwin.curr_dir[0];
	rwin.dir_entry[4].name = strdup("rfile4");
	rwin.dir_entry[4].origin = &rwin.curr_dir[0];
	rwin.dir_entry[5].name = strdup("rfile5");
	rwin.dir_entry[5].origin = &rwin.curr_dir[0];
	rwin.dir_entry[6].name = strdup("rdir6");
	rwin.dir_entry[6].type = FT_DIR;
	rwin.dir_entry[6].origin = &rwin.curr_dir[0];

	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[3].selected = 1;
	rwin.dir_entry[5].selected = 1;
	rwin.selected_files = 3;
}

SETUP_ONCE()
{
	stats_update_shell_type("/bin/sh");
}

SETUP()
{
	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(literal_percent)
{
	char *expanded = ma_expand("echo log %%", "", NULL, MER_OP);
	assert_string_equal("echo log %", expanded);
	free(expanded);
}

TEST(argument_expansion)
{
	char *expanded = ma_expand("echo %a", "this is arg", NULL, MER_OP);
	assert_string_equal("echo this is arg", expanded);
	free(expanded);
}

TEST(b_both_have_selection)
{
	char *expanded;

	expanded = ma_expand("/%b ", "", NULL, MER_SHELL);
	assert_string_equal(
			"/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, MER_SHELL);
	assert_string_equal(
			"lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(f_both_have_selection)
{
	char *expanded;

	lwin.dir_entry[2].selected = 0;
	lwin.selected_files--;

	expanded = ma_expand("/%f ", "", NULL, MER_OP);
	assert_string_equal("/lfi\\ le0 ", expanded);
	free(expanded);

	expanded = ma_expand("%f", "", NULL, MER_OP);
	assert_string_equal("lfi\\ le0", expanded);
	free(expanded);
}

TEST(l_selection)
{
	char *expanded = ma_expand("%l", "", NULL, MER_OP);
	assert_string_equal("lfi\\ le0 lfile\\\"2", expanded);
	free(expanded);
}

TEST(L_no_selection)
{
	curr_view = &rwin;
	other_view = &lwin;

	lwin.dir_entry[0].selected = 0;
	lwin.dir_entry[2].selected = 0;
	lwin.selected_files = 0;

	char *expanded = ma_expand("%L", "", NULL, MER_OP);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(b_only_lwin_has_selection)
{
	char *expanded;

	flist_sel_stash(&lwin);

	expanded = ma_expand("/%b ", "", NULL, MER_SHELL);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, MER_SHELL);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5",
			expanded);
	free(expanded);
}

TEST(b_only_rwin_has_selection)
{
	char *expanded;

	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, MER_SHELL);
	assert_string_equal("/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, MER_SHELL);
	assert_string_equal("lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(b_noone_has_selection)
{
	char *expanded;

	flist_sel_stash(&lwin);
	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, MER_SHELL);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, MER_SHELL);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(no_slash_after_dirname)
{
	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	char *expanded = ma_expand("%c", "", NULL, MER_OP);
	assert_string_equal("rdir6", expanded);
	free(expanded);
}

TEST(forward_slashes_on_win_for_non_shell)
{
	char *expanded;

	flist_sel_stash(&lwin);
	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, MER_OP);
	assert_string_equal("/lfile\\\"2 /rwin/rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, MER_OP);
	assert_string_equal("lfile\\\"2 /rwin/rfile5", expanded);
	free(expanded);
}

TEST(good_flag_macros)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand("%i echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_IGNORE, flags);
	free(expanded);

	expanded = ma_expand("%Iu echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_CUSTOMVIEW_IOUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%IU echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_VERYCUSTOMVIEW_IOUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%m echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_MENU_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%M echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_MENU_NAV_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%n echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_NO_TERM_MUX, flags);
	free(expanded);

	expanded = ma_expand("%q echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_PREVIEW_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%s echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_SPLIT, flags);
	free(expanded);

	expanded = ma_expand("%S echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_STATUSBAR_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%u echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_CUSTOMVIEW_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%U echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_VERYCUSTOMVIEW_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("%v echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_SPLIT_VERT, flags);
	free(expanded);

	expanded = ma_expand("%N echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_KEEP_SESSION, flags);
	free(expanded);

	expanded = ma_expand("%Pl echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_PIPE_FILE_LIST, flags);
	free(expanded);

	expanded = ma_expand("%Pz echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_PIPE_FILE_LIST_Z, flags);
	free(expanded);

	expanded = ma_expand("%pu echo log", "", &flags, MER_OP);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_NO_CACHE, flags);
	free(expanded);
}

TEST(bad_flag_macros)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand("%IX echo log", "", &flags, MER_OP);
	assert_string_equal("X echo log", expanded);
	assert_int_equal(MF_NONE, flags);
	free(expanded);

	expanded = ma_expand("%PX echo log", "", &flags, MER_OP);
	assert_string_equal("X echo log", expanded);
	assert_int_equal(MF_NONE, flags);
	free(expanded);
}

TEST(flags_from_different_sets)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand("echo%Pl%S", "", &flags, MER_OP);
	assert_string_equal("echo", expanded);
	assert_int_equal(MF_PIPE_FILE_LIST | MF_STATUSBAR_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("echo%q%Pz", "", &flags, MER_OP);
	assert_string_equal("echo", expanded);
	assert_int_equal(MF_PIPE_FILE_LIST_Z | MF_PREVIEW_OUTPUT, flags);
	free(expanded);

	expanded = ma_expand("echo%m%pu%Pz", "", &flags, MER_OP);
	assert_string_equal("echo", expanded);
	assert_int_equal(MF_PIPE_FILE_LIST_Z | MF_MENU_OUTPUT | MF_NO_CACHE, flags);
	assert_true(ma_flags_present(flags, MF_PIPE_FILE_LIST_Z));
	assert_true(ma_flags_present(flags, MF_MENU_OUTPUT));
	assert_true(ma_flags_present(flags, MF_NO_CACHE));
	free(expanded);
}

TEST(r_well_formed)
{
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	regs_init();

	const char *p = valid_registers;
	while(*p != '\0')
	{
		char line[32];
		char *expanded;
		const char key = *p++;
		snprintf(line, sizeof(line), "%%r%c", key);

		regs_append(key, "a");
		regs_append(key, "b");
		regs_append(key, "c");

		expanded = ma_expand(line, NULL, NULL, MER_OP);
		if(key == '_')
		{
			assert_string_equal("", expanded);
		}
		else
		{
			assert_string_equal("a b c", expanded);
		}
		free(expanded);
	}

	regs_reset();
}

TEST(r_ill_formed)
{
	char expected[] = "a b cx";

	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	regs_init();

	regs_append(DEFAULT_REG_NAME, "a");
	regs_append(DEFAULT_REG_NAME, "b");
	regs_append(DEFAULT_REG_NAME, "c");

	unsigned char key = '\0';
	do
	{
		char line[32];
		snprintf(line, sizeof(line), "%%r%c", ++key);

		if(!char_is_one_of(valid_registers, key) && key != '%')
		{
			char *const expanded = ma_expand(line, NULL, NULL, MER_OP);
			expected[5] = key;
			assert_string_equal(expected, expanded);
			free(expanded);
		}
	}
	while(key != '\0');

	regs_reset();
}

TEST(with_quotes)
{
	char *expanded;

	expanded = ma_expand("/%\"b ", "", NULL, MER_SHELL);
	assert_string_equal(
			"/\"lfi le0\" \"lfile\\\"2\" "
			"\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"f ", "", NULL, MER_SHELL);
	assert_string_equal("/\"lfi le0\" \"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"F ", "", NULL, MER_SHELL);
	assert_string_equal("/\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ",
			expanded);
	free(expanded);

	expanded = ma_expand("/%\"c ", "", NULL, MER_SHELL);
	assert_string_equal("/\"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"C ", "", NULL, MER_SHELL);
	assert_string_equal("/\"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"d ", "", NULL, MER_SHELL);
	assert_string_equal("/\"" SL "lwin\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"D ", "", NULL, MER_SHELL);
	assert_string_equal("/\"" SL "rwin\" ", expanded);
	free(expanded);

	expanded = ma_expand(" %\"a %\"m %\"M %\"s ", "", NULL, MER_OP);
	assert_string_equal(" a m M s ", expanded);
	free(expanded);

	assert_success(chdir(TEST_DATA_PATH "/existing-files"));
	regs_init();

	regs_append(DEFAULT_REG_NAME, "a");
	regs_append(DEFAULT_REG_NAME, "b");
	regs_append(DEFAULT_REG_NAME, "c");

	expanded = ma_expand("/%\"r ", "", NULL, MER_SHELL);
	assert_string_equal("/\"a\" \"b\" \"c\" ", expanded);
	free(expanded);

	regs_reset();
}

TEST(single_percent_sign)
{
	char *expanded = ma_expand("%", "", NULL, MER_OP);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(percent_sign_and_double_quote)
{
	char *expanded = ma_expand("%\"", "", NULL, MER_OP);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(empty_line_ok)
{
	char *expanded = ma_expand("", "", NULL, MER_OP);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(singly_expanded_is_not_escaped)
{
	lwin.list_pos = 0;
	char *expanded = ma_expand_single("%c");
	assert_string_equal("lfi le0", expanded);
	free(expanded);
}

TEST(singly_expanded_is_not_quoted)
{
	lwin.list_pos = 0;
	char *expanded = ma_expand_single("%\"c");
	assert_string_equal("lfi le0", expanded);
	free(expanded);
}

TEST(singly_expanded_single_macros)
{
	lwin.list_pos = 0;
	char *expanded = ma_expand_single("%d");
	assert_string_equal("/lwin", expanded);
	free(expanded);
}

TEST(singly_expanded_multiple_macros_single_file)
{
	char *expanded;

	lwin.dir_entry[2].selected = 0;
	lwin.selected_files = 1;
	expanded = ma_expand_single("%f");
	assert_string_equal("lfi le0", expanded);
	free(expanded);

	rwin.dir_entry[3].selected = 0;
	rwin.dir_entry[5].selected = 0;
	rwin.selected_files = 1;
	expanded = ma_expand_single("%F");
	assert_string_equal("/rwin/rfile1", expanded);
	free(expanded);
}

TEST(singly_not_expanded_multiple_macros_multiple_files)
{
	char *expanded;

	lwin.list_pos = 0;
	expanded = ma_expand_single("%f");
	assert_string_equal("", expanded);
	free(expanded);

	expanded = ma_expand_single("%F");
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(marking_is_not_disturbed)
{
	int indexes[] = { 1, 3 };
	mark_files_at(&lwin, 2, indexes);

	assert_true(lwin.pending_marking);
	assert_true(lwin.pending_marking);
	assert_false(lwin.dir_entry[0].marked);
	assert_true(lwin.dir_entry[1].marked);
	assert_false(lwin.dir_entry[2].marked);
	assert_true(lwin.dir_entry[3].marked);

	char *expanded = ma_expand("%%", "", NULL, MER_OP);
	assert_string_equal("%", expanded);
	free(expanded);

	assert_true(lwin.pending_marking);
	assert_false(lwin.dir_entry[0].marked);
	assert_true(lwin.dir_entry[1].marked);
	assert_false(lwin.dir_entry[2].marked);
	assert_true(lwin.dir_entry[3].marked);
}

TEST(singly_no_crash_on_wrong_register_name)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", NULL);
	create_file(SANDBOX_PATH "/spaces in the middle");

	regs_init();

	assert_success(regs_append('r', "spaces in the middle"));
	free(ma_expand_single("%r "));

	regs_reset();

	remove_file(SANDBOX_PATH "/spaces in the middle");
}

TEST(singly_expanded_single_file_register)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", NULL);
	create_file(SANDBOX_PATH "/spaces in the middle");

	regs_init();

	assert_success(regs_append('r', "spaces in the middle"));
	char *expanded = ma_expand_single("%rr");
	assert_string_equal("spaces in the middle", expanded);
	free(expanded);

	regs_reset();

	remove_file(SANDBOX_PATH "/spaces in the middle");
}

TEST(singly_not_expanded_multiple_files_register)
{
	char *expanded;

	regs_init();

	regs_append('r', "a");
	regs_append('r', "b");
	expanded = ma_expand_single("%rr");
	assert_string_equal("", expanded);
	free(expanded);

	regs_reset();
}

TEST(dollar_and_backtick_are_escaped_in_dquotes)
{
	char *expanded;

	lwin.list_pos = 3;
	assert_success(replace_string(&lwin.dir_entry[lwin.list_pos].name, "a$`b"));

	curr_stats.shell_type = ST_NORMAL;
	expanded = ma_expand("%\"c", "", NULL, MER_OP);
	assert_string_equal("\"a\\$\\`b\"", expanded);
	free(expanded);

	curr_stats.shell_type = ST_CMD;
	expanded = ma_expand("%\"c", "", NULL, MER_OP);
	assert_string_equal("\"a$`b\"", expanded);
	free(expanded);

	/* Restore normal value or some further tests can get broken. */
	curr_stats.shell_type = ST_NORMAL;
}

TEST(newline_is_escaped_with_quotes)
{
	lwin.list_pos = 3;
	assert_success(replace_string(&lwin.dir_entry[lwin.list_pos].name, "a\nb"));

	char *expanded = ma_expand("%c", "", NULL, MER_OP);
	assert_string_equal("a\"\n\"b", expanded);
	free(expanded);
}

TEST(bad_preview_macro)
{
	char *expanded = ma_expand("draw %pz", "", NULL, MER_OP);
	assert_string_equal("draw z", expanded);
	free(expanded);
}

TEST(preview_macros)
{
	lwin.window_cols = 15;
	lwin.window_rows = 10;

	char *expanded = ma_expand("draw %pw %ph", "", NULL, MER_OP);
	assert_string_equal("draw 15 10", expanded);
	free(expanded);
}

TEST(preview_macros_use_hint)
{
	const preview_area_t parea = {
		.source = &lwin,
		.view = &rwin,
		.x = 1,
		.y = 2,
		.w = 3,
		.h = 4,
	};

	rwin.window_cols = 15;
	rwin.window_rows = 10;

	curr_stats.preview_hint = &parea;

	char *expanded = ma_expand("draw %pw %ph %px %py", "", NULL, MER_OP);
	assert_string_equal("draw 3 4 0 1", expanded);
	free(expanded);

	curr_stats.preview_hint = NULL;
}

TEST(preview_clear_cmd_gets_cut_off)
{
	lwin.window_cols = 20;
	lwin.window_rows = 30;

	char *expanded = ma_expand("draw %pw %ph %pc clear", "", NULL, MER_OP);
	assert_string_equal("draw 20 30 ", expanded);
	free(expanded);
}

TEST(preview_clear_cmd_is_extracted)
{
	assert_string_equal("clear", ma_get_clear_cmd("draw %pw %ph %pc clear"));
}

TEST(preview_clear_cmd_is_optional)
{
	assert_string_equal(NULL, ma_get_clear_cmd("draw %pw %ph"));
}

TEST(preview_direct_is_expanded_to_nothing)
{
	char *expanded = ma_expand("draw %pd arg", "", NULL, MER_OP);
	assert_string_equal("draw  arg", expanded);
	free(expanded);
}

TEST(flags_to_str)
{
	assert_string_equal("", ma_flags_to_str(MF_NONE));

	assert_string_equal("%m", ma_flags_to_str(MF_MENU_OUTPUT));
	assert_string_equal("%M", ma_flags_to_str(MF_MENU_NAV_OUTPUT));
	assert_string_equal("%S", ma_flags_to_str(MF_STATUSBAR_OUTPUT));
	assert_string_equal("%q", ma_flags_to_str(MF_PREVIEW_OUTPUT));
	assert_string_equal("%u", ma_flags_to_str(MF_CUSTOMVIEW_OUTPUT));
	assert_string_equal("%U", ma_flags_to_str(MF_VERYCUSTOMVIEW_OUTPUT));
	assert_string_equal("%Iu", ma_flags_to_str(MF_CUSTOMVIEW_IOUTPUT));
	assert_string_equal("%IU", ma_flags_to_str(MF_VERYCUSTOMVIEW_IOUTPUT));

	assert_string_equal("%s", ma_flags_to_str(MF_SPLIT));
	assert_string_equal("%v", ma_flags_to_str(MF_SPLIT_VERT));
	assert_string_equal("%i", ma_flags_to_str(MF_IGNORE));
	assert_string_equal("%n", ma_flags_to_str(MF_NO_TERM_MUX));

	assert_string_equal("%N", ma_flags_to_str(MF_KEEP_SESSION));

	assert_string_equal("%Pl", ma_flags_to_str(MF_PIPE_FILE_LIST));
	assert_string_equal("%Pz", ma_flags_to_str(MF_PIPE_FILE_LIST_Z));

	assert_string_equal("%pu", ma_flags_to_str(MF_NO_CACHE));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
