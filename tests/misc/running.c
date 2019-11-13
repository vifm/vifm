#include <stic.h>

#include <unistd.h> /* chdir() rmdir() symlink() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() snprintf() */
#include <string.h> /* strdup() */

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/running.h"
#include "../../src/status.h"

#include "utils.h"

static int prog_exists(const char name[]);

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
#ifndef _WIN32
	update_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.shell_cmd_flag, "-c");
#else
	update_string(&cfg.shell, "cmd");
	update_string(&cfg.shell_cmd_flag, "/C");
#endif

	update_string(&cfg.vi_command, "echo");
	update_string(&cfg.slow_fs_list, "");
	update_string(&cfg.fuse_home, "");

	stats_update_shell_type(cfg.shell);

	view_setup(&lwin);

	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].name = strdup("b");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;

	curr_view = &lwin;
	other_view = &rwin;

	ft_init(&prog_exists);

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/existing-files",
				TEST_DATA_PATH);
	}
	else
	{
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s/existing-files", cwd,
				TEST_DATA_PATH);
	}
}

TEARDOWN()
{
	update_string(&cfg.shell, NULL);
	update_string(&cfg.shell_cmd_flag, NULL);
	stats_update_shell_type("/bin/sh");

	view_teardown(&lwin);

	ft_reset(0);
	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);
}

TEST(full_path_regexps_are_handled_for_selection)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_programs(ms, "echo %f >> " SANDBOX_PATH "/run", 0, 1);

	open_file(&lwin, FHE_NO_RUN);

	/* Checking for file existence on its removal. */
	assert_success(remove(SANDBOX_PATH "/run"));
}

TEST(full_path_regexps_are_handled_for_selection2)
{
	matchers_t *ms;
	char pattern[PATH_MAX + 16];
	char *error;

	/* Mind that there is no chdir(), this additionally checks that origins are
	 * being used by the code. */

	snprintf(pattern, sizeof(pattern), "//%s/*//", lwin.curr_dir);
	ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
#ifndef _WIN32
	ft_set_programs(ms, "echo > /dev/null %c &", 0, 1);
#else
	ft_set_programs(ms, "echo > NUL %c &", 0, 1);
#endif

	open_file(&lwin, FHE_NO_RUN);

	/* If we don't crash, then everything is fine. */
}

TEST(following_resolves_links_in_origin, IF(not_windows))
{
	assert_success(os_mkdir(SANDBOX_PATH "/A", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/A/B", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/A/C", 0700));
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("../C", SANDBOX_PATH "/A/B/c"));
	assert_success(symlink("A/B", SANDBOX_PATH "/B"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "B", cwd);
	populate_dir_list(&lwin, 0);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("c", lwin.dir_entry[0].name);
	lwin.list_pos = 0;

	char *saved_cwd = save_cwd();
	follow_file(&lwin);
	restore_cwd(saved_cwd);

	assert_true(paths_are_same(lwin.curr_dir, SANDBOX_PATH "/A"));
	assert_string_equal("C", lwin.dir_entry[lwin.list_pos].name);

	assert_success(remove(SANDBOX_PATH "/B"));
	assert_success(remove(SANDBOX_PATH "/A/B/c"));
	assert_success(rmdir(SANDBOX_PATH "/A/C"));
	assert_success(rmdir(SANDBOX_PATH "/A/B"));
	assert_success(rmdir(SANDBOX_PATH "/A"));
}

TEST(following_to_a_broken_symlink_is_possible, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("no-file", SANDBOX_PATH "/bad"));
	assert_success(symlink("bad", SANDBOX_PATH "/to-bad"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	populate_dir_list(&lwin, 0);

	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("to-bad", lwin.dir_entry[1].name);
	lwin.list_pos = 1;

	char *saved_cwd = save_cwd();
	follow_file(&lwin);
	restore_cwd(saved_cwd);

	assert_true(paths_are_same(lwin.curr_dir, SANDBOX_PATH));
	assert_string_equal("bad", lwin.dir_entry[lwin.list_pos].name);

	assert_success(remove(SANDBOX_PATH "/bad"));
	assert_success(remove(SANDBOX_PATH "/to-bad"));
}

static int
prog_exists(const char name[])
{
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
