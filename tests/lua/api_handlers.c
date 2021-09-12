#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/cmd_completion.h"
#include "../../src/status.h"

#include <test-utils.h>

static vlua_t *vlua;

static const preview_area_t parea = {
	.source = &lwin,
	.view = &lwin,
	.x = 1,
	.y = 2,
	.w = 3,
	.h = 4,
};

static dir_entry_t entry = {
	.name = "binary-data",
	.origin = TEST_DATA_PATH "/test-data/read",
	.name_dec_num = -1,
};

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
}

TEST(handler_check)
{
	assert_false(vlua_handler_cmd(vlua, "normal command"));
	assert_false(vlua_handler_cmd(vlua, "#something"));
	assert_true(vlua_handler_cmd(vlua, "#something#"));
	assert_true(vlua_handler_cmd(vlua, "#something#more"));
	assert_true(vlua_handler_cmd(vlua, "#something#more here"));
}

TEST(bad_args)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = nil,"
				                      " handler = nil })"));
	assert_true(ends_with(ui_sb_last(), ": `name` key is mandatory"));

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'NAME',"
				                      " handler = nil })"));
	assert_true(ends_with(ui_sb_last(), ": `handler` key is mandatory"));
}

TEST(bad_name)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = '',"
				                      " handler = handler })"));
	assert_true(ends_with(ui_sb_last(), ": Handler's name can't be empty"));

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'name with white\tspace',"
				                      " handler = handler })"));
	assert_true(ends_with(ui_sb_last(),
				": Handler's name can't contain whitespace"));
}

TEST(registered)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handler })"));
	assert_string_equal("true", ui_sb_last());

	assert_true(vlua_handler_present(vlua, "#vifmtest#handle"));

	curr_stats.vlua = vlua;
	assert_true(external_command_exists("#vifmtest#handle"));
	curr_stats.vlua = NULL;
}

TEST(duplicate_rejected)
{
	assert_success(vlua_run_string(vlua, "function handler() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handler })"));
	assert_string_equal("true", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handler })"));
	assert_string_equal("false", ui_sb_last());
}

TEST(invoked)
{
	assert_success(vlua_run_string(vlua,
				"function handler(info)"
				"  return { lines = {info.command, info.path} }\n"
				"end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handler })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path", &parea);
	assert_int_equal(2, lines.nitems);
	assert_string_equal("#vifmtest#handle", lines.items[0]);
	assert_string_equal("path", lines.items[1]);
	free_string_array(lines.items, lines.nitems);
}

TEST(bad_invocation)
{
	strlist_t lines =
		vlua_view_file(vlua, "#vifmtest#nosuchhandle", "path", &parea);
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);

	vlua_open_file(vlua, "#vifmtest#nosuchhandle", &entry);
}

TEST(error_invocation)
{
	assert_success(vlua_run_string(vlua, "function handle() asdf() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path", &parea);
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);
}

TEST(wrong_return)
{
	assert_success(vlua_run_string(vlua, "function handle() return true end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path", &parea);
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);
}

TEST(missing_field)
{
	assert_success(vlua_run_string(vlua, "function handle() return {} end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path", &parea);
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);
}

TEST(error_open_invocation)
{
	assert_success(vlua_run_string(vlua, "function handle() asdf() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	ui_sb_msg("");
	vlua_open_file(vlua, "#vifmtest#handle", &entry);
	assert_true(ends_with(ui_sb_last(),
				": global 'asdf' is not callable (a nil value)"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
