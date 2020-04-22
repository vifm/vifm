#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/bmarks.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

static int count_bmarks(void);
static void bmarks_cb(const char p[], const char t[], time_t timestamp,
		void *arg);

static int cb_called;
static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();
	bmarks_clear();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	curr_stats.load_stage = -1;
	curr_stats.save_msg = 0;
}

TEARDOWN()
{
	bmarks_clear();
	vle_keys_reset();
	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(list_of_bmarks_is_filtered)
{
	assert_success(exec_commands("bmark! /a taga", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /b tagb", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks taga", &lwin, CIT_COMMAND));

	assert_int_equal(2, count_bmarks());
	assert_int_equal(1, menu_get_current()->len);
	(void)vle_keys_exec(WK_ESC);
}

TEST(enter_navigates_to_selected_bmark)
{
	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "bmark! '%s/read' taga", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_CR);
	assert_true(paths_are_equal(lwin.curr_dir, test_data));
	(void)vle_keys_exec(WK_ESC);
}

TEST(gf_navigates_to_selected_bmark)
{
	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "bmark! '%s/' taga", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

	lwin.curr_dir[0] = '\0';
	(void)vle_keys_exec(WK_g WK_f);
	assert_true(paths_are_equal(lwin.curr_dir, test_data));
	(void)vle_keys_exec(WK_ESC);
}

TEST(e_opens_selected_bmark)
{
	char buf[PATH_MAX + 1];

	snprintf(buf, sizeof(buf), "bmark! '%s/read' taga", test_data);
	assert_success(exec_commands(buf, &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

#ifndef _WIN32
	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.vi_command, "echo > " SANDBOX_PATH "/out");
#else
	replace_string(&cfg.shell, "cmd");
	update_string(&cfg.vi_command, "echo > " SANDBOX_PATH "/out");
#endif
	stats_update_shell_type(cfg.shell);

	(void)vle_keys_exec(WK_e);
	(void)vle_keys_exec(WK_ESC);

	int nlines;
	char **list = read_file_of_lines(SANDBOX_PATH "/out", &nlines);
	assert_int_equal(1, nlines);
#ifndef _WIN32
	snprintf(buf, sizeof(buf), "+1 %s/read", test_data);
#else
	snprintf(buf, sizeof(buf), "  +1 \"%s/read\"", test_data);
#endif
	assert_string_equal(buf, list[0]);
	free_string_array(list, nlines);

	assert_success(remove(SANDBOX_PATH "/out"));

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type("/bin/sh");
}

TEST(bmark_is_deleted)
{
	assert_success(exec_commands("bmark! /a taga", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /b tagb", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

	assert_int_equal(2, count_bmarks());
	(void)vle_keys_exec(WK_d WK_d);
	assert_int_equal(1, count_bmarks());
	(void)vle_keys_exec(WK_ESC);
}

TEST(unhandled_key_is_ignored)
{
	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "bmark! '%s/read' taga", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_x);
	(void)vle_keys_exec(WK_ESC);
}

TEST(bmgo_navigates_to_single_match)
{
	lwin.curr_dir[0] = '\0';
	char cmd[PATH_MAX + 1];
	snprintf(cmd, sizeof(cmd), "bmark! '%s/read' taga", test_data);
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmgo", &lwin, CIT_COMMAND));
	assert_true(paths_are_equal(lwin.curr_dir, test_data));
}

TEST(sorting_updates_associated_data)
{
	assert_success(exec_commands("bmark! /c tagb", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /b tagb", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmark! /a taga", &lwin, CIT_COMMAND));
	assert_success(exec_commands("bmarks", &lwin, CIT_COMMAND));

	assert_int_equal(3, menu_get_current()->len);
	assert_string_equal("/a", menu_get_current()->data[0]);
	assert_string_equal("/b", menu_get_current()->data[1]);
	assert_string_equal("/c", menu_get_current()->data[2]);
	assert_true(starts_with_lit(menu_get_current()->items[0], "/a"));
	assert_true(starts_with_lit(menu_get_current()->items[1], "/b"));
	assert_true(starts_with_lit(menu_get_current()->items[2], "/c"));
	(void)vle_keys_exec(WK_ESC);
}

static int
count_bmarks(void)
{
	cb_called = 0;
	bmarks_list(&bmarks_cb, NULL);
	return cb_called;
}

static void
bmarks_cb(const char p[], const char t[], time_t timestamp, void *arg)
{
	++cb_called;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
