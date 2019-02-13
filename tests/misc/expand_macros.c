#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/flist_sel.h"
#include "../../src/macros.h"
#include "../../src/registers.h"

#include "utils.h"

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

	expanded = ma_expand("/%b ", "", NULL, 1);
	assert_string_equal(
			"/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, 1);
	assert_string_equal(
			"lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(f_both_have_selection)
{
	char *expanded;

	lwin.dir_entry[2].selected = 0;
	lwin.selected_files--;

	expanded = ma_expand("/%f ", "", NULL, 0);
	assert_string_equal("/lfi\\ le0 ", expanded);
	free(expanded);

	expanded = ma_expand("%f", "", NULL, 0);
	assert_string_equal("lfi\\ le0", expanded);
	free(expanded);
}

TEST(b_only_lwin_has_selection)
{
	char *expanded;

	flist_sel_stash(&lwin);

	expanded = ma_expand("/%b ", "", NULL, 1);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, 1);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5",
			expanded);
	free(expanded);
}

TEST(b_only_rwin_has_selection)
{
	char *expanded;

	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, 1);
	assert_string_equal("/lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, 1);
	assert_string_equal("lfi\\ le0 lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(b_noone_has_selection)
{
	char *expanded;

	flist_sel_stash(&lwin);
	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, 1);
	assert_string_equal("/lfile\\\"2 " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, 1);
	assert_string_equal("lfile\\\"2 " SL "rwin" SL "rfile5", expanded);
	free(expanded);
}

TEST(no_slash_after_dirname)
{
	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	char *expanded = ma_expand("%c", "", NULL, 0);
	assert_string_equal("rdir6", expanded);
	free(expanded);
}

TEST(forward_slashes_on_win_for_non_shell)
{
	char *expanded;

	flist_sel_stash(&lwin);
	flist_sel_stash(&rwin);

	expanded = ma_expand("/%b ", "", NULL, 0);
	assert_string_equal("/lfile\\\"2 /rwin/rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand("%b", "", NULL, 0);
	assert_string_equal("lfile\\\"2 /rwin/rfile5", expanded);
	free(expanded);
}

TEST(m)
{
	MacroFlags flags;

	rwin.list_pos = 6;
	curr_view = &rwin;
	other_view = &lwin;
	char *expanded = ma_expand("%M echo log", "", &flags, 0);
	assert_string_equal(" echo log", expanded);
	assert_int_equal(MF_MENU_NAV_OUTPUT, flags);
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

		expanded = ma_expand(line, NULL, NULL, 0);
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
			char *const expanded = ma_expand(line, NULL, NULL, 0);
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

	expanded = ma_expand("/%\"b ", "", NULL, 1);
	assert_string_equal(
			"/\"lfi le0\" \"lfile\\\"2\" "
			"\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"f ", "", NULL, 1);
	assert_string_equal("/\"lfi le0\" \"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"F ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin" SL "rfile1\" \"" SL "rwin" SL "rfile3\" \"" SL "rwin" SL "rfile5\" ",
			expanded);
	free(expanded);

	expanded = ma_expand("/%\"c ", "", NULL, 1);
	assert_string_equal("/\"lfile\\\"2\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"C ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin" SL "rfile5\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"d ", "", NULL, 1);
	assert_string_equal("/\"" SL "lwin\" ", expanded);
	free(expanded);

	expanded = ma_expand("/%\"D ", "", NULL, 1);
	assert_string_equal("/\"" SL "rwin\" ", expanded);
	free(expanded);

	expanded = ma_expand(" %\"a %\"m %\"M %\"s ", "", NULL, 0);
	assert_string_equal(" a m M s ", expanded);
	free(expanded);

	assert_success(chdir(TEST_DATA_PATH "/existing-files"));
	regs_init();

	regs_append(DEFAULT_REG_NAME, "a");
	regs_append(DEFAULT_REG_NAME, "b");
	regs_append(DEFAULT_REG_NAME, "c");

	expanded = ma_expand("/%\"r ", "", NULL, 1);
	assert_string_equal("/\"a\" \"b\" \"c\" ", expanded);
	free(expanded);

	regs_reset();
}

TEST(single_percent_sign)
{
	char *expanded = ma_expand("%", "", NULL, 0);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(percent_sign_and_double_quote)
{
	char *expanded = ma_expand("%\"", "", NULL, 0);
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(empty_line_ok)
{
	char *expanded = ma_expand("", "", NULL, 0);
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
	lwin.dir_entry[2].selected = 0;
	lwin.selected_files = 1;
	char *expanded = ma_expand_single("%f");
	assert_string_equal("lfi le0", expanded);
	free(expanded);
}

TEST(singly_not_expanded_multiple_macros_multiple_files)
{
	lwin.list_pos = 0;
	char *expanded = ma_expand_single("%f");
	assert_string_equal("", expanded);
	free(expanded);
}

TEST(singly_no_crash_on_wrong_register_name)
{
	assert_success(chdir(TEST_DATA_PATH "/spaces-in-names"));
	regs_init();

	assert_success(regs_append('r', "spaces in the middle"));
	free(ma_expand_single("%r "));

	regs_reset();
}

TEST(singly_expanded_single_file_register)
{
	char *expanded;

	assert_success(chdir(TEST_DATA_PATH "/spaces-in-names"));
	regs_init();

	assert_success(regs_append('r', "spaces in the middle"));
	expanded = ma_expand_single("%rr");
	assert_string_equal("spaces in the middle", expanded);
	free(expanded);

	regs_reset();
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
	expanded = ma_expand("%\"c", "", NULL, 0);
	assert_string_equal("\"a\\$\\`b\"", expanded);
	free(expanded);

	curr_stats.shell_type = ST_CMD;
	expanded = ma_expand("%\"c", "", NULL, 0);
	assert_string_equal("\"a$`b\"", expanded);
	free(expanded);

	/* Restore normal value or some further tests can get broken. */
	curr_stats.shell_type = ST_NORMAL;
}

TEST(newline_is_escaped_with_quotes)
{
	lwin.list_pos = 3;
	assert_success(replace_string(&lwin.dir_entry[lwin.list_pos].name, "a\nb"));

	char *expanded = ma_expand("%c", "", NULL, 0);
	assert_string_equal("a\"\n\"b", expanded);
	free(expanded);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
