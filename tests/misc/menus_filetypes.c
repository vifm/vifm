#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcpy() */

#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

#include "utils.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	ft_init(NULL);
	ft_reset(0);
}

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);
	populate_dir_list(&lwin, 0);

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
	assert_success(exec_commands("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_CR);

	char new_path[PATH_MAX + 1];
	make_abs_path(new_path, sizeof(new_path), TEST_DATA_PATH, "color-schemes",
			cwd);
	assert_true(paths_are_same(lwin.curr_dir, new_path));

	(void)vle_keys_exec(WK_ESC);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
