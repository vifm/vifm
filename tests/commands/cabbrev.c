#include <stic.h>

#include <wctype.h> /* iswspace() */

#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/cmds.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"

SETUP()
{
	int i;
	for(i = 0; i < 255; ++i)
	{
		cfg.word_chars[i] = !iswspace(i);
	}

	lwin.selected_files = 0;
	lwin.list_rows = 0;

	curr_view = &lwin;

	cmds_init();
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_abbr_reset();
}

TEST(addition)
{
	int no_remap;

	assert_success(cmds_dispatch("cabbrev lhs1 rhs1", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs1", vle_abbr_expand(L"lhs1", &no_remap));

	assert_success(cmds_dispatch("cnoreabbrev lhs2 rhs2", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs2", vle_abbr_expand(L"lhs2", &no_remap));
}

TEST(single_character_abbrev)
{
	int no_remap;

	assert_success(cmds_dispatch("cabbrev x y", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"y", vle_abbr_expand(L"x", &no_remap));

	assert_success(cmds_dispatch("cnoreabbrev a b", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"b", vle_abbr_expand(L"a", &no_remap));
}

TEST(add_with_remap)
{
	int no_remap = -1;

	assert_success(cmds_dispatch("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);
}

TEST(add_with_no_remap)
{
	int no_remap = -1;

	assert_success(cmds_dispatch("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_true(no_remap);
}

TEST(double_cabbrev_same_lhs)
{
	int no_remap = -1;

	assert_success(cmds_dispatch("cabbrev lhs rhs1", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs1", vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);

	assert_success(cmds_dispatch("cabbrev lhs rhs2", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs2", vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);

	assert_success(cmds_dispatch("cnoreabbrev lhs rhs1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("cnoreabbrev lhs rhs2", &lwin, CIT_COMMAND));
}

TEST(unabbrev_by_lhs)
{
	int no_remap;

	assert_success(cmds_dispatch("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(cmds_dispatch("cunabbrev lhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));

	assert_success(cmds_dispatch("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(cmds_dispatch("cunabbrev lhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

TEST(unabbrev_by_rhs)
{
	int no_remap;

	assert_success(cmds_dispatch("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(cmds_dispatch("cunabbrev rhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));

	assert_success(cmds_dispatch("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(cmds_dispatch("cunabbrev rhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

TEST(unabbrev_error)
{
	int no_remap;

	ui_sb_msg("");
	assert_failure(cmds_dispatch("cunabbrev rhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_string_equal("No such abbreviation: rhs", ui_sb_last());
}

TEST(cant_list_no_abbrevs)
{
	ui_sb_msg("");
	assert_failure(cmds_dispatch("cabbrev l", &lwin, CIT_COMMAND));
	assert_string_equal("No abbreviations found", ui_sb_last());
}

TEST(list_abbrevs_matches)
{
	const char *expected =
		"Abbreviation -- N -- Expansion/Description\n"
		"lhs1                 1\n"
		"lhs2                 2";

	assert_success(cmds_dispatch("cabbrev lhs1 1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("cabbrev lhs2 2", &lwin, CIT_COMMAND));

	ui_sb_msg("");
	assert_failure(cmds_dispatch("cabbrev lh", &lwin, CIT_COMMAND));
	assert_string_equal(expected, ui_sb_last());
}

TEST(list_abbrevs_no_matches)
{
	assert_success(cmds_dispatch("cabbrev lhs1 1", &lwin, CIT_COMMAND));

	ui_sb_msg("");
	assert_failure(cmds_dispatch("cabbrev r", &lwin, CIT_COMMAND));
	assert_string_equal("No abbreviations found", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
