#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h> /* remove() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/menus/menus.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"

/* This tests implementation of menus without activating the mode. */

static menu_data_t m;

SETUP()
{
	menus_init_data(&m, &lwin, strdup("test"), strdup("No matches"));
}

TEARDOWN()
{
	menus_reset_data(&m);
}

TEST(can_navigate_to_broken_symlink, IF(not_windows))
{
	char *saved_cwd;
	char buf[PATH_MAX + 1];

	strcpy(lwin.curr_dir, ".");

	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	assert_success(make_symlink("/wrong/path", "broken-link"));

	make_abs_path(buf, sizeof(buf), SANDBOX_PATH , "broken-link:", saved_cwd);
	/* Were trying to open broken link, which will fail, but the parsing part
	 * should succeed. */
	restore_cwd(saved_cwd);
	assert_success(menus_goto_file(&m, &lwin, buf, 1));

	assert_success(remove(SANDBOX_PATH "/broken-link"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
