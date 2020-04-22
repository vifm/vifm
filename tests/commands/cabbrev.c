#include <stic.h>

#include <wctype.h> /* iswspace() */

#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/cmds.h"
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

	init_commands();
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_abbr_reset();
}

TEST(addition)
{
	int no_remap;

	assert_success(exec_commands("cabbrev lhs1 rhs1", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs1", vle_abbr_expand(L"lhs1", &no_remap));

	assert_success(exec_commands("cnoreabbrev lhs2 rhs2", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs2", vle_abbr_expand(L"lhs2", &no_remap));
}

TEST(single_character_abbrev)
{
	int no_remap;

	assert_success(exec_commands("cabbrev x y", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"y", vle_abbr_expand(L"x", &no_remap));

	assert_success(exec_commands("cnoreabbrev a b", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"b", vle_abbr_expand(L"a", &no_remap));
}

TEST(add_with_remap)
{
	int no_remap = -1;

	assert_success(exec_commands("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);
}

TEST(add_with_no_remap)
{
	int no_remap = -1;

	assert_success(exec_commands("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_true(no_remap);
}

TEST(double_cabbrev_same_lhs)
{
	int no_remap = -1;

	assert_success(exec_commands("cabbrev lhs rhs1", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs1", vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);

	assert_success(exec_commands("cabbrev lhs rhs2", &lwin, CIT_COMMAND));
	assert_wstring_equal(L"rhs2", vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);

	assert_success(exec_commands("cnoreabbrev lhs rhs1", &lwin, CIT_COMMAND));
	assert_success(exec_commands("cnoreabbrev lhs rhs2", &lwin, CIT_COMMAND));
}

TEST(unabbrev_by_lhs)
{
	int no_remap;

	assert_success(exec_commands("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(exec_commands("cunabbrev lhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));

	assert_success(exec_commands("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(exec_commands("cunabbrev lhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

TEST(unabbrev_by_rhs)
{
	int no_remap;

	assert_success(exec_commands("cabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(exec_commands("cunabbrev rhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));

	assert_success(exec_commands("cnoreabbrev lhs rhs", &lwin, CIT_COMMAND));
	assert_non_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_success(exec_commands("cunabbrev rhs", &lwin, CIT_COMMAND));
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
