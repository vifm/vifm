#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matchers.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

static line_stats_t *stats;
static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	stats = get_line_stats();

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	ft_init(NULL);
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

	ft_reset(0);
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

TEST(opening_a_directory_works)
{
	assert_success(exec_commands("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_CR);

	char new_path[PATH_MAX + 1];
	make_abs_path(new_path, sizeof(new_path), TEST_DATA_PATH, "color-schemes",
			cwd);
	assert_true(paths_are_same(lwin.curr_dir, new_path));

	(void)vle_keys_exec(WK_ESC);
}

TEST(no_menu_for_fake_entry)
{
	get_current_entry(&lwin)->name[0] = '\0';
	assert_success(exec_commands("file", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(c_key_is_handled)
{
	assert_success(exec_commands("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));

	assert_wstring_equal(L"!vifm", stats->line);

	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(unknown_key_is_ignored)
{
	assert_success(exec_commands("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_t);
	(void)vle_keys_exec(WK_CR);

	char new_path[PATH_MAX + 1];
	make_abs_path(new_path, sizeof(new_path), TEST_DATA_PATH, "color-schemes",
			cwd);
	assert_true(paths_are_same(lwin.curr_dir, new_path));

	(void)vle_keys_exec(WK_ESC);
}

TEST(pseudo_entry_is_always_present_for_directories)
{
	char *error;
	matchers_t *ms = matchers_alloc("{bla-*/}", 0, 1, "", &error);
	assert_non_null(ms);

	ft_set_programs(ms, "abc-run %c", 0, 0);

	assert_success(exec_commands("filetype bla-dir/", &lwin, CIT_COMMAND));

	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("[present] [Enter directory] " VIFM_PSEUDO_CMD,
			menu_get_current()->items[0]);
	assert_string_equal("[present]                   abc-run %c",
			menu_get_current()->items[1]);

	(void)vle_keys_exec(WK_ESC);
}

TEST(no_menu_if_no_handlers)
{
	assert_failure(exec_commands("filetype bla-file", &lwin, CIT_COMMAND));
}

TEST(filetypes_menu)
{
	char *error;
	matchers_t *ms = matchers_alloc("{a,b,c}", 0, 1, "", &error);
	assert_non_null(ms);

	ft_set_programs(ms, "abc-run %c", 0, 0);

	assert_success(exec_commands("filetype b", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("[present] abc-run %c", menu_get_current()->items[0]);

	(void)vle_keys_exec(WK_ESC);
}

TEST(fileviewers_menu)
{
	char *error;
	matchers_t *ms = matchers_alloc("{a,b,c}", 0, 1, "", &error);
	assert_non_null(ms);

	ft_set_viewers(ms, "abc-view %c");

	assert_success(exec_commands("fileviewer c", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("[present] abc-view %c", menu_get_current()->items[0]);

	(void)vle_keys_exec(WK_ESC);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
