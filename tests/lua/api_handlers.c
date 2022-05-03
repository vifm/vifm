#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/utils/utils.h"
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
				": attempt to call a nil value (global 'asdf')"));
}

TEST(invalid_statusline_formatter)
{
	char *format = vlua_make_status_line(vlua, "#vifmtest#nohandle", &lwin, 10);
	assert_string_equal("Invalid handler", format);
	free(format);
}

TEST(error_statusline_formatter)
{
	assert_success(vlua_run_string(vlua, "function handle() asdf() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char *format = vlua_make_status_line(vlua, "#vifmtest#handle", &lwin, 10);
	assert_true(ends_with(format,
				": attempt to call a nil value (global 'asdf')"));
	free(format);
}

TEST(bad_statusline_formatter)
{
	assert_success(vlua_run_string(vlua, "function handle() return 'format' end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char *format = vlua_make_status_line(vlua, "#vifmtest#handle", &lwin, 10);
	assert_string_equal("Return value isn't a table.", format);
	free(format);
}

TEST(good_statusline_formatter)
{
	assert_success(vlua_run_string(vlua,
				"function handle(info) return { format = 'width='..info.width } end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char *format = vlua_make_status_line(vlua, "#vifmtest#handle", &lwin, 10);
	assert_string_equal("width=10", format);
	free(format);
}

TEST(handlers_run_in_safe_mode)
{
	assert_success(vlua_run_string(vlua,
				"function handle(info) vifm.opts.global.statusline = 'value' end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char *format = vlua_make_status_line(vlua, "#vifmtest#handle", &lwin, 10);
	assert_true(ends_with(format,
				": Unsafe functions can't be called in this environment!"));
	free(format);
}

TEST(bad_editor_handler)
{
	assert_failure(vlua_edit_one(vlua, "#vifmtest#handle", "path", -1, -1, 0));
}

TEST(error_editor_handler)
{
	assert_success(vlua_run_string(vlua, "function handle() asdf() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	assert_failure(vlua_edit_one(vlua, "#vifmtest#handle", "path", -1, -1, 0));
	assert_true(ends_with(ui_sb_last(),
				": attempt to call a nil value (global 'asdf')"));
}

TEST(error_editor_handler_return)
{
	/* nil return. */

	assert_success(vlua_run_string(vlua, "function handle() end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	assert_failure(vlua_edit_one(vlua, "#vifmtest#handle", "path", -1, -1, 0));

	/* Bad table fields. */

	assert_success(vlua_run_string(vlua, "function handle() return {} end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle2',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	assert_failure(vlua_edit_one(vlua, "#vifmtest#handle2", "path", -1, -1, 0));
}

TEST(good_editor_handler_return)
{
	assert_success(vlua_run_string(vlua,
				"function handle() return { success = true } end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	assert_success(vlua_edit_one(vlua, "#vifmtest#handle", "path", -1, -1, 0));
}

TEST(open_help_input)
{
#ifndef _WIN32
	char vimdoc_dir[PATH_MAX + 1] = PACKAGE_DATA_DIR "/vim-doc";
#else
	char exe_dir[PATH_MAX + 1];
	(void)get_exe_dir(exe_dir, sizeof(exe_dir));

	char vimdoc_dir[PATH_MAX + 1];
	snprintf(vimdoc_dir, sizeof(vimdoc_dir), "%s/vim-doc", exe_dir);
#endif

	assert_success(vlua_run_string(vlua,
				"function handle(info)"
				"  print(info.command, info.action, info.vimdocdir, info.topic)"
				"  return { success = true }"
				"end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char expected[PATH_MAX + 1];
	snprintf(expected, sizeof(expected), "#vifmtest#handle\topen-help\t%s\ttopic",
			vimdoc_dir);

	assert_success(vlua_open_help(vlua, "#vifmtest#handle", "topic"));
	assert_string_equal(expected, ui_sb_last());
}

TEST(edit_one_input)
{
	assert_success(vlua_run_string(vlua,
				"function handle(info)"
				"  print(info.command, info.action, info.path, info.mustwait,"
				"         info.line, info.column)"
				"  return { success = true }"
				"end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	assert_success(vlua_edit_one(vlua, "#vifmtest#handle", "path", 10, 1, 0));
	assert_string_equal("#vifmtest#handle\tedit-one\tpath\tfalse\t10\t1",
			ui_sb_last());
}

TEST(edit_many_input)
{
	assert_success(vlua_run_string(vlua,
				"function handle(info)"
				"  print(info.command, info.action, #info.paths, info.paths[1])"
				"  return { success = true }"
				"end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char path[] = "path";
	char *paths[] = { path };

	assert_success(vlua_edit_many(vlua, "#vifmtest#handle", paths, 1));
	assert_string_equal("#vifmtest#handle\tedit-many\t1\tpath", ui_sb_last());
}

TEST(edit_list_input)
{
	assert_success(vlua_run_string(vlua,
				"function handle(info)"
				"  print(info.command, info.action, #info.entries, info.entries[1],"
				"        info.entries[2], info.current, info.isquickfix)"
				"  return { success = true }"
				"end"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.addhandler{ name = 'handle',"
				                      " handler = handle })"));
	assert_string_equal("true", ui_sb_last());

	char entry0[] = "e0";
	char entry1[] = "e1";
	char *entries[] = { entry0, entry1 };

	assert_success(vlua_edit_list(vlua, "#vifmtest#handle", entries, 2,
				/*current=*/1, /*quickfix=*/1));
	assert_string_equal("#vifmtest#handle\tedit-list\t2\te0\te1\t2\ttrue",
			ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
