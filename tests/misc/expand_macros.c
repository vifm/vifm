#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/commands.h"
#include "../../src/config.h"
#include "../../src/filelist.h"
#include "../../src/ui.h"

#ifdef _WIN32
#define SL "\\\\"
#else
#define SL "/"
#endif

static void
setup_lwin(void)
{
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 4;
	lwin.list_pos = 2;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfi le0");
	lwin.dir_entry[1].name = strdup("lfile1");
	lwin.dir_entry[2].name = strdup("lfile\"2");
	lwin.dir_entry[3].name = strdup("lfile3");

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 2;
}

static void
setup_rwin(void)
{
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 7;
	rwin.list_pos = 5;
	rwin.dir_entry = calloc(rwin.list_rows, sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[1].name = strdup("rfile1");
	rwin.dir_entry[2].name = strdup("rfile2");
	rwin.dir_entry[3].name = strdup("rfile3");
	rwin.dir_entry[4].name = strdup("rfile4");
	rwin.dir_entry[5].name = strdup("rfile5");
	rwin.dir_entry[6].type = DIRECTORY;
	rwin.dir_entry[6].name = strdup("rdir6/");

	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[3].selected = 1;
	rwin.dir_entry[5].selected = 1;
	rwin.selected_files = 3;
}

static void
setup(void)
{
	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;

	cfg.max_args = 8192;
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	free(rwin.dir_entry);
}

static void
test_b_both_have_selection(void)
{
	int menu, split;
	char *expanded;

	expanded = expand_macros(&lwin, "/%b ", "", &menu, &split);
	assert_string_equal(
			"/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "%b", "", &menu, &split);
	assert_string_equal(
			"lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

static void
test_f_both_have_selection(void)
{
	int menu, split;
	char *expanded;

	lwin.dir_entry[2].selected = 0;
	lwin.selected_files--;

	expanded = expand_macros(&lwin, "/%f ", "", &menu, &split);
	assert_string_equal("/lfi\\ le0 ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "%f", "", &menu, &split);
	assert_string_equal("lfi\\ le0", expanded);
	free(expanded);
}

static void
test_b_only_lwin_has_selection(void)
{
	int menu, split;
	char *expanded;

	clean_selected_files(&lwin);

	expanded = expand_macros(&lwin, "/%b ", "", &menu, &split);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "%b", "", &menu, &split);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5",
			expanded);
	free(expanded);
}

static void
test_b_only_rwin_has_selection(void)
{
	int menu, split;
	char *expanded;

	clean_selected_files(&rwin);

	expanded = expand_macros(&lwin, "/%b ", "", &menu, &split);
	assert_string_equal("/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "%b", "", &menu, &split);
	assert_string_equal("lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

static void
test_b_noone_has_selection(void)
{
	int menu, split;
	char *expanded;

	clean_selected_files(&lwin);
	clean_selected_files(&rwin);

	expanded = expand_macros(&lwin, "/%b ", "", &menu, &split);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "%b", "", &menu, &split);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

static void
test_no_slash_after_dirname(void)
{
	int menu, split;
	char *expanded;

	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	expanded = expand_macros(&rwin, "%c", "", &menu, &split);
	assert_string_equal("rdir6", expanded);
	free(expanded);
}

static void
test_m(void)
{
	int menu, split;
	char *expanded;

	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	expanded = expand_macros(&rwin, "%M echo log", "", &menu, &split);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(menu, 2);
	free(expanded);
}

static void
test_with_quotes(void)
{
	int menu, split;
	char *expanded;

	expanded = expand_macros(&lwin, "/%\"b ", "", &menu, &split);
	assert_string_equal(
			"/\"lfi le0\" \"lfile\\\"2\" "
			"\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"f ", "", &menu, &split);
	assert_string_equal("/\"lfi le0\" \"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"F ", "", &menu, &split);
	assert_string_equal("/\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ",
			expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"c ", "", &menu, &split);
	assert_string_equal("/\"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"C ", "", &menu, &split);
	assert_string_equal("/\"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"d ", "", &menu, &split);
	assert_string_equal("/\"" SL "lwin\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, "/%\"D ", "", &menu, &split);
	assert_string_equal("/\"" SL "rwin\" ", expanded);
	free(expanded);

	expanded = expand_macros(&lwin, " %\"a %\"m %\"M %\"s ", "", &menu, &split);
	assert_string_equal(" a m M s ", expanded);
	free(expanded);
}

void
test_expand_macros(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_b_both_have_selection);
	run_test(test_f_both_have_selection);
	run_test(test_b_only_lwin_has_selection);
	run_test(test_b_only_rwin_has_selection);
	run_test(test_b_noone_has_selection);
	run_test(test_no_slash_after_dirname);
	run_test(test_m);
	run_test(test_with_quotes);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
