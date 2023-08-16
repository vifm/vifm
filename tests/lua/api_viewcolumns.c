#include <stic.h>

#include <string.h> /* memcpy() */

#include "../../src/lua/vlua.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/opt_handlers.h"

#include <test-utils.h>

#include "asserts.h"

static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);

enum { MAX_WIDTH = 40 };

static vlua_t *vlua;
static char print_buffer[MAX_WIDTH + 1];

SETUP_ONCE()
{
	stub_colmgr();
}

SETUP()
{
	vlua = vlua_init();

	view_setup(&lwin);
	curr_view = &lwin;
}

TEARDOWN()
{
	vlua_finish(vlua);

	view_teardown(&lwin);

	columns_teardown();
}

TEST(bad_args)
{
	BLUA_ENDS(vlua, ": `name` key is mandatory",
			"print(vifm.addcolumntype{ name = nil, handler = nil })");

	BLUA_ENDS(vlua, ": `handler` key is mandatory",
			"print(vifm.addcolumntype{ name = 'NAME', handler = nil })");
}

TEST(bad_name)
{
	GLUA_EQ(vlua, "", "function handler() end");

	BLUA_ENDS(vlua, ": View column name can't be empty",
			"print(vifm.addcolumntype{ name = '', handler = handler })");

	BLUA_ENDS(vlua,
			": View column name must not start with a lower case Latin letter",
			"print(vifm.addcolumntype{ name = 'name', handler = handler })");

	BLUA_ENDS(vlua,
			": View column name must not contain non-Latin characters",
			"print(vifm.addcolumntype{ name = 'A-A', handler = handler })");
}

TEST(column_is_registered)
{
	GLUA_EQ(vlua, "", "function handler() end");

	assert_int_equal(-1, vlua_viewcolumn_map(vlua, "Test"));

	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype{ name = 'Test', handler = handler })");

	int id = vlua_viewcolumn_map(vlua, "Test");
	assert_true(id != -1);

	char *name = vlua_viewcolumn_map_back(vlua, id);
	assert_string_equal("Test", name);
	free(name);

	assert_string_equal(NULL, vlua_viewcolumn_map_back(vlua, -1));
}

TEST(duplicate_name)
{
	GLUA_EQ(vlua, "", "function handler() end");

	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype{ name = 'Test', handler = handler })");
	BLUA_ENDS(vlua, ": View column with such name already exists: Test",
			"print(vifm.addcolumntype{ name = 'Test', handler = handler })");
}

TEST(columns_are_used)
{
	opt_handlers_setup();
	lwin.columns = columns_create();
	curr_stats.vlua = vlua;

	GLUA_EQ(vlua, "", "function err() func() end");
	GLUA_EQ(vlua, "", "function noval() end");
	GLUA_EQ(vlua, "", "function badval() return {} end");
	GLUA_EQ(vlua, "",
			"function good(info)"
			"  return { text = info.entry.name .. info.width }"
			"end");
	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype { name = 'Err', handler = err })");
	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype { name = 'NoVal', handler = noval })");
	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype { name = 'BadVal', handler = badval })");
	GLUA_EQ(vlua, "true",
			"print(vifm.addcolumntype { name = 'Good', handler = good })");

	process_set_args("viewcolumns=10{Err},10{NoVal},10{BadVal},10{Good}", 0, 1);

	dir_entry_t entry = { .name = "name", .origin = "origin" };
	column_data_t cdt = { .view = &lwin, .entry = &entry };

	columns_set_line_print_func(&column_line_print);
	columns_format_line(lwin.columns, &cdt, MAX_WIDTH);
	assert_string_equal("     ERROR   NOVALUE   NOVALUE    name10", print_buffer);

	opt_handlers_teardown();
	curr_stats.vlua = NULL;
}

TEST(symlinks, IF(not_windows))
{
	assert_success(make_symlink("something", SANDBOX_PATH "/symlink"));

	opt_handlers_setup();
	lwin.columns = columns_create();
	curr_stats.vlua = vlua;

	GLUA_EQ(vlua, "",
			"function handler(info)"
			"  return { text = info.entry.type .. ' -> ' .. info.entry.gettarget(),"
			"           matchstart = 1,"
			"           matchend = 2 }"
			"end");

	GLUA_EQ(vlua, "true",
				"print(vifm.addcolumntype { name = 'Test',"
				"                           handler = handler,"
				"                           isprimary = true })");

	process_set_args("viewcolumns=-20{Test}", 0, 1);

	columns_set_line_print_func(&column_line_print);

	dir_entry_t entry = { .name = "name", .origin = "origin", .type = FT_DIR };
	column_data_t cdt = { .view = &lwin, .entry = &entry };
	columns_format_line(lwin.columns, &cdt, MAX_WIDTH);
	assert_string_equal("ERROR                                   ", print_buffer);

	entry.type = FT_LINK;
	columns_format_line(lwin.columns, &cdt, MAX_WIDTH);
	assert_string_equal("ERROR                                   ", print_buffer);

	entry.name = "symlink";
	entry.origin = SANDBOX_PATH;
	columns_format_line(lwin.columns, &cdt, MAX_WIDTH);
	assert_string_equal("link -> something                       ", print_buffer);

	opt_handlers_teardown();
	curr_stats.vlua = NULL;

	remove_file(SANDBOX_PATH "/symlink");
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	memcpy(print_buffer + offset, buf, strlen(buf));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
