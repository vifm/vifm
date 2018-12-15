#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include "utils.h"

SETUP_ONCE()
{
	static char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"color-schemes/", cwd);
}

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	curr_stats.load_stage = -1;
	curr_stats.save_msg = 0;
}

TEARDOWN()
{
	vle_keys_reset();
	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(enter_loads_selected_colorscheme)
{
	assert_success(exec_commands("colorscheme", &lwin, CIT_COMMAND));

	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	(void)vle_keys_exec(WK_G);
	(void)vle_keys_exec(WK_CR);

	assert_string_equal("good", cfg.cs.name);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(0, cfg.cs.color[WIN_COLOR].attr);

	(void)vle_keys_exec(WK_ESC);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
