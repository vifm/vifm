#include <stic.h>

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/ui/color_manager.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/background.h"

static char *saved_cwd;

DEFINE_SUITE();

SETUP_ONCE()
{
	fix_environ();

	stub_colmgr();

	/* Remember original path in global SETUP_ONCE instead of SETUP to make sure
	 * nothing will change the path before we try to save it. */
	saved_cwd = save_cwd();

	bg_init();

	tabs_init();
}

SETUP()
{
	strcpy(lwin.curr_dir, "/non-existing-dir");
	strcpy(rwin.curr_dir, "/non-existing-dir");
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
