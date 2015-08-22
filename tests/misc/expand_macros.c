#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/macros.h"
#include "../../src/registers.h"

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

SETUP()
{
	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	dynarray_free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	dynarray_free(rwin.dir_entry);
}

TEST(b_both_have_selection)
{
	char *expanded;

	expanded = expand_macros("/%b ", "", NULL, 1);
	assert_string_equal(
			"/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros("%b", "", NULL, 1);
	assert_string_equal(
			"lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(f_both_have_selection)
{
	char *expanded;

	lwin.dir_entry[2].selected = 0;
	lwin.selected_files--;

	expanded = expand_macros("/%f ", "", NULL, 0);
	assert_string_equal("/lfi\\ le0 ", expanded);
	free(expanded);

	expanded = expand_macros("%f", "", NULL, 0);
	assert_string_equal("lfi\\ le0", expanded);
	free(expanded);
}

TEST(b_only_lwin_has_selection)
{
	char *expanded;

	clean_selected_files(&lwin);

	expanded = expand_macros("/%b ", "", NULL, 1);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros("%b", "", NULL, 1);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5",
			expanded);
	free(expanded);
}

TEST(b_only_rwin_has_selection)
{
	char *expanded;

	clean_selected_files(&rwin);

	expanded = expand_macros("/%b ", "", NULL, 1);
	assert_string_equal("/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros("%b", "", NULL, 1);
	assert_string_equal("lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(b_noone_has_selection)
{
	char *expanded;

	clean_selected_files(&lwin);
	clean_selected_files(&rwin);

	expanded = expand_macros("/%b ", "", NULL, 1);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros("%b", "", NULL, 1);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(no_slash_after_dirname)
{
	char *expanded;

	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	expanded = expand_macros("%c", "", NULL, 0);
	assert_string_equal("rdir6", expanded);
	free(expanded);
}

TEST(forward_slashes_on_win_for_non_shell)
{
	char *expanded;

	clean_selected_files(&lwin);
	clean_selected_files(&rwin);

	expanded = expand_macros("/%b ", "", NULL, 0);
	assert_string_equal("/lfile\\\"2 /rwin/rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros("%b", "", NULL, 0);
	assert_string_equal("lfile\\\"2 /rwin/rfile5", expanded);
	free(expanded);
}

TEST(m)
{
	MacroFlags flags;
	char *expanded;

	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	expanded = expand_macros("%M echo log", "", &flags, 0);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_MENU_NAV_OUTPUT, flags);
	free(expanded);
}

TEST(r_well_formed)
{
	const char *p;

	chdir(TEST_DATA_PATH "/existing-files");

	init_registers();

	p = valid_registers;
	while(*p != '\0')
	{
		char line[32];
		char *expanded;
		const char key = *p++;
		snprintf(line, sizeof(line), "%%r%c", key);

		append_to_register(key, "a");
		append_to_register(key, "b");
		append_to_register(key, "c");

		expanded = expand_macros(line, NULL, NULL, 0);
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

	clear_registers();
}

TEST(r_ill_formed)
{
	char key;
	char expected[] = "a b cx";

	chdir(TEST_DATA_PATH "/existing-files");

	init_registers();

	append_to_register(DEFAULT_REG_NAME, "a");
	append_to_register(DEFAULT_REG_NAME, "b");
	append_to_register(DEFAULT_REG_NAME, "c");

	key = '\0';
	do
	{
		char line[32];
		snprintf(line, sizeof(line), "%%r%c", ++key);

		if(!char_is_one_of(valid_registers, key) && key != '%')
		{
			char *const expanded = expand_macros(line, NULL, NULL, 0);
			expected[5] = key;
			assert_string_equal(expected, expanded);
			free(expanded);
		}
	}
	while(key != '\0');

	clear_registers();
}

TEST(with_quotes)
{
	char *expanded;

	expanded = expand_macros("/%\"b ", "", NULL, 1);
	assert_string_equal(
			"/\"lfi le0\" \"lfile\\\"2\" "
			"\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = expand_macros("/%\"f ", "", NULL, 1);
	assert_string_equal("/\"lfi le0\" \"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = expand_macros("/%\"F ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ",
			expanded);
	free(expanded);

	expanded = expand_macros("/%\"c ", "", NULL, 1);
	assert_string_equal("/\"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = expand_macros("/%\"C ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = expand_macros("/%\"d ", "", NULL, 1);
	assert_string_equal("/\"" SL "lwin\" ", expanded);
	free(expanded);

	expanded = expand_macros("/%\"D ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin\" ", expanded);
	free(expanded);

	expanded = expand_macros(" %\"a %\"m %\"M %\"s ", "", NULL, 0);
	assert_string_equal(" a m M s ", expanded);
	free(expanded);

	chdir(TEST_DATA_PATH "/existing-files");
	init_registers();

	append_to_register(DEFAULT_REG_NAME, "a");
	append_to_register(DEFAULT_REG_NAME, "b");
	append_to_register(DEFAULT_REG_NAME, "c");

	expanded = expand_macros("/%\"r ", "", NULL, 1);
	assert_string_equal("/\"a\" \"b\" \"c\" ", expanded);
	free(expanded);

	clear_registers();
}

TEST(single_percent_sign)
{
	char *expanded;

	expanded = expand_macros("%", "", NULL, 0);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(percent_sign_and_double_quote)
{
	char *expanded;

	expanded = expand_macros("%\"", "", NULL, 0);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(empty_line_ok)
{
	char *expanded;

	expanded = expand_macros("", "", NULL, 0);
	assert_string_equal("", expanded);
	free(expanded);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
