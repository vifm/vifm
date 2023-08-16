#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/plugins.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;
static plugs_t *plugs;
static plug_t plug_dummy;

SETUP()
{
	conf_setup();

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	create_dir(SANDBOX_PATH "/plugins");
	create_dir(SANDBOX_PATH "/plugins/plug");

	vlua = vlua_init();
	plugs = plugs_create(vlua);

	char plug_path[PATH_MAX + 1];
	make_abs_path(plug_path, sizeof(plug_path), SANDBOX_PATH, "plugins/plug",
			NULL);

	update_string(&plug_dummy.name, "plug");
	update_string(&plug_dummy.path, plug_path);
}

TEARDOWN()
{
	conf_teardown();

	plugs_free(plugs);
	vlua_finish(vlua);

	remove_dir(SANDBOX_PATH "/plugins/plug");
	remove_dir(SANDBOX_PATH "/plugins");

	update_string(&plug_dummy.name, NULL);
	update_string(&plug_dummy.path, NULL);
	update_string(&plug_dummy.log, NULL);
	plug_dummy.log_len = 0;
}

TEST(good_plugin_loaded)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");

	ui_sb_msg("");
	assert_success(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_equal("", ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(bad_return_value)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return 123");

	ui_sb_msg("");
	assert_failure(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_equal("Failed to load 'plug' plugin: it didn't return a table",
			ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(syntax_error)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "-+");

	ui_sb_msg("");
	assert_failure(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_starts_with("Failed to load 'plug' plugin: ", ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(runtime_error)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "badcall()");

	ui_sb_msg("");
	assert_failure(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_starts_with("Failed to start 'plug' plugin: ", ui_sb_last());

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(hidden_dir_is_ignored)
{
	remove_dir(SANDBOX_PATH "/plugins/plug");
	create_dir(SANDBOX_PATH "/plugins/.git");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	remove_dir(SANDBOX_PATH "/plugins/.git");
	create_dir(SANDBOX_PATH "/plugins/plug");
}

TEST(multiple_plugins_loaded)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return {}");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "1y2y",
			"print((vifm.plugins.all.plug and '1y' or '1n').."
			"      (vifm.plugins.all.plug2 and '2y' or '2n'))");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(can_load_plugins_only_once)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	assert_false(plugs_loaded(plugs));

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	assert_true(plugs_loaded(plugs));

	const plug_t *plug;
	assert_true(plugs_get(plugs, 0, &plug));
	assert_false(plugs_get(plugs, 1, &plug));

	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());
	assert_true(plugs_loaded(plugs));

	assert_true(plugs_get(plugs, 0, &plug));
	assert_false(plugs_get(plugs, 1, &plug));

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(loading_missing_plugin_fails)
{
	assert_failure(vlua_load_plugin(vlua, &plug_dummy));
}

TEST(plugin_statuses_are_correct)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return");

	load_plugins(plugs, cfg.config_dir);

	const plug_t *plug;
	PluginLoadStatus status;
	assert_true(plugs_get(plugs, 0, &plug));
	status = (ends_with(plug->path, "plug2") ? PLS_FAILURE : PLS_SUCCESS);
	assert_int_equal(status, plug->status);
	assert_true(plugs_get(plugs, 1, &plug));
	status = (ends_with(plug->path, "plug2") ? PLS_FAILURE : PLS_SUCCESS);
	assert_int_equal(status, plug->status);
	assert_false(plugs_get(plugs, 2, &plug));

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(plugins_can_be_blacklisted)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return {}");

	plugs_blacklist(plugs, "plug2");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "1y2n",
			"print((vifm.plugins.all.plug and '1y' or '1n').."
			"      (vifm.plugins.all.plug2 and '2y' or '2n'))");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(plugins_can_be_whitelisted)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return {}");

	plugs_whitelist(plugs, "plug2");
	plugs_blacklist(plugs, "plug2");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "1n2y",
			"print((vifm.plugins.all.plug and '1y' or '1n').."
			"      (vifm.plugins.all.plug2 and '2y' or '2n'))");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(plugin_metadata)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"return { name = vifm.plugin.name }");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "plug", "print(vifm.plugins.all.plug.name)");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(good_plugin_module)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"return vifm.plugin.require('sub')");
	make_file(SANDBOX_PATH "/plugins/plug/sub.lua",
			"return vifm.plugin.require('subsub')");
	make_file(SANDBOX_PATH "/plugins/plug/subsub.lua",
			"return { source = 'subsub' }");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "subsub", "print(vifm.plugins.all.plug.source)");

	remove_file(SANDBOX_PATH "/plugins/plug/subsub.lua");
	remove_file(SANDBOX_PATH "/plugins/plug/sub.lua");
	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(missing_plugin_module)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"return vifm.plugin.require('sub')");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_starts_with("Failed to start 'plug' plugin: ", ui_sb_last());

	GLUA_EQ(vlua, "nil", "print(vifm.plugins.all.plug)");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(plugins_can_add_handler)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"local function handler() end\n"
			"vifm.addhandler({ name='handler', handler=handler })\n"
			"return {}");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	assert_true(vlua_handler_present(vlua, "#plug#handler"));

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(can_not_load_same_plugin_twice, IF(not_windows))
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return {}");
	assert_success(make_symlink("plug", SANDBOX_PATH "/plugins/plug2"));

	load_plugins(plugs, cfg.config_dir);

	const plug_t *plug1, *plug2, *plug3;
	assert_true(plugs_get(plugs, 0, &plug1));
	PluginLoadStatus status1 = plug1->status;
	assert_true(plugs_get(plugs, 1, &plug2));
	PluginLoadStatus status2 = plug2->status;
	assert_false(plugs_get(plugs, 2, &plug3));

	assert_true((status1 == PLS_SUCCESS && status2 == PLS_SKIPPED) ||
	            (status2 == PLS_SUCCESS && status1 == PLS_SKIPPED));

	const plug_t *skipped = (status1 == PLS_SKIPPED ? plug1 : plug2);
	assert_string_starts_with("[vifm][error]: skipped as a duplicate of",
			skipped->log);

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2");
}

TEST(print_outputs_to_plugin_log)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"print('arg1', 'arg2'); return {}");

	ui_sb_msg("");
	assert_success(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_equal("", ui_sb_last());
	assert_string_equal("arg1\targ2", plug_dummy.log);

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(print_without_arguments)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"print('first line'); print(); print('third line'); return {}");

	update_string(&plug_dummy.log, NULL);
	assert_success(vlua_load_plugin(vlua, &plug_dummy));
	assert_string_equal("first line\n\nthird line", plug_dummy.log);

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

TEST(can_not_load_plugins_with_the_same_name)
{
	create_dir(SANDBOX_PATH "/plugins2");
	create_dir(SANDBOX_PATH "/plugins2/plug");

	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"return { src = 'plugins' }");
	make_file(SANDBOX_PATH "/plugins2/plug/init.lua",
			"return { src = 'plugins2' }");

	strlist_t plugins_dirs = { };
	plugins_dirs.nitems = add_to_string_array(&plugins_dirs.items,
			plugins_dirs.nitems, SANDBOX_PATH "/plugins2");
	plugins_dirs.nitems = add_to_string_array(&plugins_dirs.items,
			plugins_dirs.nitems, SANDBOX_PATH "/plugins");

	plugs_load(plugs, plugins_dirs);

	free_string_array(plugins_dirs.items, plugins_dirs.nitems);

	GLUA_EQ(vlua, "plugins2", "print(vifm.plugins.all.plug.src)");

	const plug_t *plug1, *plug2;
	assert_true(plugs_get(plugs, 0, &plug1));
	PluginLoadStatus status1 = plug1->status;
	assert_true(plugs_get(plugs, 1, &plug2));
	PluginLoadStatus status2 = plug2->status;

	assert_int_equal(PLS_SUCCESS, status1);
	assert_int_equal(PLS_SKIPPED, status2);

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins2/plug/init.lua");
	remove_dir(SANDBOX_PATH "/plugins2/plug");
	remove_dir(SANDBOX_PATH "/plugins2");
}

TEST(plugin_dir_does_not_need_to_exist)
{
	load_plugins(plugs, SANDBOX_PATH "/no-such-path");
}

TEST(global_table_is_mocked)
{
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug/init.lua",
			"_G.data = 'plug1'"
			"return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua",
			"function _G.func()"
			"  return data\n"
			"end\n"
			"return {}");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	BLUA_ENDS(vlua, ": attempt to call a nil value (global 'func')",
			"print(func())");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
}

TEST(metatables_are_protected)
{
	make_file(SANDBOX_PATH "/plugins/plug/init.lua", "return { _G = _G }");

	ui_sb_msg("");
	load_plugins(plugs, cfg.config_dir);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "false", "print(getmetatable(vifm.plugins.all.plug._G))");

	remove_file(SANDBOX_PATH "/plugins/plug/init.lua");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
