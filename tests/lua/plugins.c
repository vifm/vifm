#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	create_dir(SANDBOX_PATH "/plugins");
	create_dir(SANDBOX_PATH "/plugins/plug");

	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);

	remove_dir(SANDBOX_PATH "/plugins/plug");
	remove_dir(SANDBOX_PATH "/plugins");
}

TEST(good_plugin_loaded)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");

	ui_sb_msg("");
	assert_success(vlua_load_plugin(vlua, "plug"));
	assert_string_equal("", ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(bad_return_value)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return 123");

	ui_sb_msg("");
	assert_failure(vlua_load_plugin(vlua, "plug"));
	assert_string_equal("Failed to load 'plug' plugin: it didn't return a table",
			ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(syntax_error)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "invalidlua");

	ui_sb_msg("");
	assert_failure(vlua_load_plugin(vlua, "plug"));
	assert_true(starts_with_lit(ui_sb_last(),"Failed to load 'plug' plugin: "));

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(multiple_plugins_loaded)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return {}");

	ui_sb_msg("");
	vlua_load_plugins(vlua);
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "print((plug and '1y' or '1n').."
	                                     "      (plug2 and '2y' or '2n'))"));
	assert_string_equal("1y2y", ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(loading_missing_plugin_fails)
{
	assert_failure(vlua_load_plugin(vlua, "plug"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
