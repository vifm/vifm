#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ops.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
}

TEST(color_fields)
{
	/* With uninitialized curses these may produce unexpected output, so just
	 * checking for presence and types. */
	GLUA_ENDS(vlua, "number", "print(type(vifm.color.count))");
	GLUA_ENDS(vlua, "boolean", "print(type(vifm.color.gui))");
}

TEST(color_new_bad_cterm_colors)
{
	/* Bad type. */
	BLUA_ENDS(vlua, ": `ctermfg` value is of wrong type: table",
			"vifm.color.new { ctermfg = {} }");
	BLUA_ENDS(vlua, ": `ctermbg` value is of wrong type: table",
			"vifm.color.new { ctermbg = {} }");

	/* Bad string value. */
	GLUA_EQ(vlua, "nil\tBad value of 'ctermfg' field: 'bad'",
			"print(vifm.color.new { ctermfg = 'bad' })");
	GLUA_EQ(vlua, "nil\tBad value of 'ctermbg' field: 'bad'",
			"print(vifm.color.new { ctermbg = 'bad' })");

	/* Bad numerical value. */
	GLUA_EQ(vlua, "nil\tBad value of 'ctermfg' field: '-5'",
			"print(vifm.color.new { ctermfg = -5 })");
	GLUA_EQ(vlua, "nil\tBad value of 'ctermbg' field: '-5'",
			"print(vifm.color.new { ctermbg = -5 })");
}

TEST(color_new_bad_gui_colors)
{
	/* Bad type. */
	BLUA_ENDS(vlua, ": `guifg` value must be a string",
			"vifm.color.new { guifg = {} }");
	BLUA_ENDS(vlua, ": `guibg` value must be a string",
			"vifm.color.new { guibg = {} }");

	/* Bad string value. */
	GLUA_EQ(vlua, "nil\tBad value of 'guifg' field: 'bad'",
			"print(vifm.color.new { guifg = 'bad' })");
	GLUA_EQ(vlua, "nil\tBad value of 'guibg' field: 'bad'",
			"print(vifm.color.new { guibg = 'bad' })");
}

TEST(color_new_bad_attrs)
{
	/* Bad field type. */
	BLUA_ENDS(vlua, ": `cterm` value must be a table",
			"vifm.color.new { cterm = 10 }");
	BLUA_ENDS(vlua, ": `gui` value must be a table",
			"vifm.color.new { gui = 10 }");

	/* Bad element type. */
	BLUA_ENDS(vlua, ": Bad value of 'cterm' field",
			"assert(vifm.color.new { cterm = { 10 } })");
	BLUA_ENDS(vlua, ": Bad value of 'gui' field",
			"assert(vifm.color.new { gui = { 10 } })");

	/* Bad string value. */
	BLUA_ENDS(vlua, ": Bad value of 'cterm' field",
			"assert(vifm.color.new { cterm = { 'bad' } })");
	BLUA_ENDS(vlua, ": Bad value of 'gui' field",
			"assert(vifm.color.new { gui = { 'bad' } })");
}

TEST(color_new_good)
{
	GLUA_STARTS(vlua, "VifmColor: ", "print(vifm.color.new {})");

	/* cterm colors */
	GLUA_STARTS(vlua, "VifmColor: ",
			"print(vifm.color.new {"
			"  ctermfg = 'white',"
			"  ctermbg = 'black',"
			"})");
	/* Testing numerical values other than -1 requires mocking COLORS. */
	GLUA_STARTS(vlua, "VifmColor: ",
			"print(vifm.color.new {"
			"  ctermfg = -1,"
			"  ctermbg = -1,"
			"})");

	/* gui colors */
	GLUA_STARTS(vlua, "VifmColor: ",
			"print(vifm.color.new {"
			"  guifg = '#000000',"
			"  guibg = '#FFffFF',"
			"})");

	/* attributes */
	GLUA_STARTS(vlua, "VifmColor: ",
			"print(vifm.color.new {"
			"  cterm = { 'combine', 'none', 'bold' },"
			"  gui = { },"
			"})");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
