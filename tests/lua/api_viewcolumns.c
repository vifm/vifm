#include <stic.h>

#include <string.h> /* memcpy() */

#include "../../src/lua/vlua.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/opt_handlers.h"

#include <test-utils.h>

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
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = nil,"
				                         " handler = nil })"));
	assert_string_ends_with(": `name` key is mandatory", ui_sb_last());

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'NAME',"
				                         " handler = nil })"));
	assert_string_ends_with(": `handler` key is mandatory", ui_sb_last());
}

TEST(bad_name)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = '',"
				                         " handler = handler })"));
	assert_string_ends_with(": View column name can't be empty", ui_sb_last());

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'name',"
				                         " handler = handler })"));
	assert_string_ends_with(
			": View column name must not start with a lower case Latin letter",
			ui_sb_last());

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'A-A',"
				                         " handler = handler })"));
	assert_string_ends_with(
			": View column name must not contain non-Latin characters", ui_sb_last());
}

TEST(column_is_registered)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	assert_int_equal(-1, vlua_viewcolumn_map(vlua, "Test"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Test',"
				                         " handler = handler })"));
	assert_string_equal("true", ui_sb_last());

	int id = vlua_viewcolumn_map(vlua, "Test");
	assert_true(id != -1);

	char *name = vlua_viewcolumn_map_back(vlua, id);
	assert_string_equal("Test", name);
	free(name);

	assert_string_equal(NULL, vlua_viewcolumn_map_back(vlua, -1));
}

TEST(duplicate_name)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Test',"
				                         " handler = handler })"));
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Test',"
				                         " handler = handler })"));
	assert_string_ends_with(": View column with such name already exists: Test",
			ui_sb_last());
}

TEST(columns_are_used)
{
	opt_handlers_setup();
	lwin.columns = columns_create();
	curr_stats.vlua = vlua;

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "function err() func() end"));
	assert_success(vlua_run_string(vlua, "function noval() end"));
	assert_success(vlua_run_string(vlua, "function badval() return {} end"));
	assert_success(vlua_run_string(vlua,
				"function good(info)\n"
				"  return { text = info.entry.name .. info.width }\n"
				"end"));
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Err',"
				                         " handler = err })"));
	assert_string_equal("true", ui_sb_last());
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'NoVal',"
				                         " handler = noval })"));
	assert_string_equal("true", ui_sb_last());
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'BadVal',"
				                         " handler = badval })"));
	assert_string_equal("true", ui_sb_last());
	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Good',"
				                         " handler = good })"));
	assert_string_equal("true", ui_sb_last());

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

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
	      "function handler(info)\n"
	      "  return { text = info.entry.type .. ' -> ' .. info.entry.gettarget(),"
	      "           matchstart = 1,"
				"           matchend = 2 }\n"
	      "end"));
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua,
				"print(vifm.addcolumntype{ name = 'Test',"
				                         " handler = handler,"
				                         " isprimary = true })"));
	assert_string_equal("true", ui_sb_last());

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
