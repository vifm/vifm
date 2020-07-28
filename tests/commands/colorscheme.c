#include <stic.h>

#include <stdio.h> /* free() snprintf() */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/color_scheme.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/cmd_core.h"

static char *saved_cwd;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	saved_cwd = save_cwd();
	cs_load_defaults();

	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"color-schemes/", saved_cwd);
}

TEARDOWN()
{
	vle_cmds_reset();

	restore_cwd(saved_cwd);
	cs_load_defaults();
}

TEST(current_colorscheme_is_printed)
{
	strcpy(cfg.cs.name, "test-scheme");

	ui_sb_msg("");
	assert_failure(exec_commands("colorscheme?", &lwin, CIT_COMMAND));
	assert_string_equal("test-scheme", ui_sb_last());
}

TEST(unknown_colorscheme_is_not_loaded)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	assert_failure(exec_commands("colorscheme bad-cs-name", &lwin, CIT_COMMAND));

	assert_string_equal("test-scheme", cfg.cs.name);
	assert_int_equal(1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(2, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(3, cfg.cs.color[WIN_COLOR].attr);

	cs_load_primary("bad-cs-name");

	assert_string_equal("test-scheme", cfg.cs.name);
	assert_int_equal(1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(2, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(3, cfg.cs.color[WIN_COLOR].attr);
}

TEST(good_colorscheme_is_loaded)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	assert_success(exec_commands("colorscheme good", &lwin, CIT_COMMAND));

	assert_string_equal("good", cfg.cs.name);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(0, cfg.cs.color[WIN_COLOR].attr);
}

TEST(unknown_colorscheme_is_not_associated)
{
	assert_failure(exec_commands("colorscheme bad-name /", &lwin, CIT_COMMAND));
}

TEST(colorscheme_is_not_associated_to_a_file)
{
	char path[PATH_MAX + 1];
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/very-long-line",
			saved_cwd);

	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "colorscheme good %s", path);

	assert_failure(exec_commands(cmd, &lwin, CIT_COMMAND));
}

TEST(colorscheme_is_not_associated_to_relpath_on_startup)
{
	strcpy(lwin.curr_dir, saved_cwd);

	assert_success(exec_commands("colorscheme good .", &lwin, CIT_COMMAND));
	assert_false(cs_load_local(1, "."));

	assert_success(exec_commands("colorscheme name1 name2", &lwin, CIT_COMMAND));
	assert_false(cs_load_local(1, "name2"));
}

TEST(colorscheme_is_associated_to_tildepath_on_startup)
{
	make_abs_path(cfg.home_dir, sizeof(cfg.home_dir), SANDBOX_PATH, "/",
			saved_cwd);
	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), SANDBOX_PATH, "colors",
			saved_cwd);

	create_dir(SANDBOX_PATH "/colors");
	create_file(SANDBOX_PATH "/colors/cs.vifm");

	assert_success(exec_commands("colorscheme cs ~/colors", &lwin,
				CIT_COMMAND));
	char *path = expand_tilde("~/colors");
	assert_true(cs_load_local(1, path));
	free(path);

	remove_file(SANDBOX_PATH "/colors/cs.vifm");
	remove_dir(SANDBOX_PATH "/colors");
}

TEST(colorscheme_is_restored_on_bad_name)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	assert_success(exec_commands("colorscheme bad-color", &lwin, CIT_COMMAND));

	assert_string_equal("test-scheme", cfg.cs.name);
	assert_int_equal(1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(2, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(3, cfg.cs.color[WIN_COLOR].attr);
}

TEST(colorscheme_is_restored_on_sourcing_error)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"scripts/", saved_cwd);
	assert_success(exec_commands("colorscheme wrong-cmd-name", &lwin,
				CIT_COMMAND));

	assert_string_equal("test-scheme", cfg.cs.name);
	assert_int_equal(1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(2, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(3, cfg.cs.color[WIN_COLOR].attr);
}

TEST(colorscheme_is_restored_on_multiple_loading_failures)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	assert_success(exec_commands("colorscheme bad-cs-name bad-cmd bad-color",
				&lwin, CIT_COMMAND));

	assert_string_equal("test-scheme", cfg.cs.name);
	assert_int_equal(1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(2, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(3, cfg.cs.color[WIN_COLOR].attr);
}

TEST(first_usable_colorscheme_is_loaded)
{
	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	assert_success(exec_commands("colorscheme bad-color good", &lwin,
				CIT_COMMAND));

	assert_string_equal("good", cfg.cs.name);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(0, cfg.cs.color[WIN_COLOR].attr);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
