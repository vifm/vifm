#include <stic.h>

#include "../../src/engine/abbrevs.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/bracket_notation.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP_ONCE()
{
	init_bracket_notation();
}

SETUP()
{
	vlua = vlua_init();
	curr_stats.vlua = vlua;

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	vlua_finish(vlua);
	curr_stats.vlua = NULL;

	vle_abbr_reset();
}

TEST(abbrevs_add_errors)
{
	BLUA_ENDS(vlua, ": `lhs` key is mandatory",
			"vifm.abbrevs.add {"
			"  handler = handler,"
			"}");

	BLUA_ENDS(vlua, ": `handler` key is mandatory",
			"vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"}");

	BLUA_ENDS(vlua, ": `lhs` can't be empty or longer than 31",
			"vifm.abbrevs.add {"
			"  lhs = '',"
			"  handler = handler,"
			"}");
}

TEST(abbrevs_bad_handler)
{
	int no_remap;

	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  handler = function() adsf() end,"
			"})");

	ui_sb_msg("");
	assert_null(vle_abbr_expand(L"lhs", &no_remap));
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());
}

TEST(abbrevs_bad_handler_return)
{
	int no_remap;

	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  handler = function() return nil end,"
			"})");

	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

TEST(abbrevs_bad_handler_rhs_return)
{
	int no_remap;

	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  handler = function() return { rhs = {} } end,"
			"})");

	assert_null(vle_abbr_expand(L"lhs", &no_remap));
}

TEST(abbrevs_add)
{
	int no_remap;

	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  description = 'descr',"
			"  handler = function(info)"
			"    print(info.lhs)"
			"    return { rhs = 'rhs' }"
			"  end,"
			"})");

	ui_sb_msg("");
	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));
	assert_true(no_remap);
	assert_string_equal("lhs", ui_sb_last());
}

TEST(abbrevs_add_duplicate_fails)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  handler = function(info) end"
			"})");

	GLUA_EQ(vlua, "false",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  handler = function(info) end"
			"})");
}

TEST(abbrevs_noremap)
{
	int no_remap;

	GLUA_EQ(vlua, "true",
			"print(vifm.abbrevs.add {"
			"  lhs = 'lhs',"
			"  description = 'descr',"
			"  noremap = false,"
			"  handler = function(info)"
			"    return { rhs = 'rhs' }"
			"  end,"
			"})");

	assert_wstring_equal(L"rhs", vle_abbr_expand(L"lhs", &no_remap));
	assert_false(no_remap);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
