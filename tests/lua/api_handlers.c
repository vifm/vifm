#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
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
				"function handler(info) return {info.command, info.path} end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handler })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path");
	assert_int_equal(2, lines.nitems);
	assert_string_equal("#vifmtest#handle", lines.items[0]);
	assert_string_equal("path", lines.items[1]);
	free_string_array(lines.items, lines.nitems);
}

TEST(bad_invocation)
{
	strlist_t lines = vlua_view_file(vlua, "#vifmtest#nosuchhandle", "path");
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);
}

TEST(error_invocation)
{
	assert_success(vlua_run_string(vlua, "function handle() asdf() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path");
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

	strlist_t lines = vlua_view_file(vlua, "#vifmtest#handle", "path");
	assert_int_equal(0, lines.nitems);
	free_string_array(lines.items, lines.nitems);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
