#include <stic.h>

#include <unistd.h> /* chdir() symlink() */

#include <stdlib.h> /* remove() */
#include <string.h> /* strcpy() */

#include "../../src/menus/menus.h"
#include "../../src/utils/fs.h"

static int not_windows(void);

TEST(can_navigate_to_broken_symlink, IF(not_windows))
{
	char *saved_cwd;

	strcpy(lwin.curr_dir, ".");

	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink("/wrong/path", "broken-link"));
#endif

	/* Were trying to open broken link, which will fail, but the parsing part
	 * should succeed. */
	restore_cwd(saved_cwd);
	assert_success(goto_selected_file(&lwin, SANDBOX_PATH "/broken-link:", 1));

	assert_success(remove(SANDBOX_PATH "/broken-link"));
}

static int
not_windows(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
