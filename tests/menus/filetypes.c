#include <stic.h>

#include <stddef.h> /* NULL */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

static int fake_prog_exists(const char name[]);
static void setup_file_test(void);

static line_stats_t *stats;
static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	stats = get_line_stats();

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	ft_init(&fake_prog_exists);
}

SETUP()
{
	conf_setup();
	modes_init();
	cmds_init();

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
	assert_success(cmds_dispatch("file", &lwin, CIT_COMMAND));

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
	assert_success(cmds_dispatch("file", &lwin, CIT_COMMAND));
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(c_key_is_handled)
{
	assert_success(cmds_dispatch("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));

	assert_wstring_equal(L"!vifm", stats->line);

	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(unknown_key_is_ignored)
{
	assert_success(cmds_dispatch("file", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_t);
	(void)vle_keys_exec(WK_CR);

	char new_path[PATH_MAX + 1];
	make_abs_path(new_path, sizeof(new_path), TEST_DATA_PATH, "color-schemes",
			cwd);
	assert_true(paths_are_same(lwin.curr_dir, new_path));

	(void)vle_keys_exec(WK_ESC);
}

TEST(file_can_show_missing_entries)
{
	setup_file_test();

	assert_success(cmds_dispatch1("file", &lwin, CIT_COMMAND));

	assert_true(menu_get_current()->len >= 3);
	assert_string_equal("          no-cat", menu_get_current()->items[0]);
	assert_string_equal("[present] cat", menu_get_current()->items[1]);
	assert_string_equal("          no-echo", menu_get_current()->items[2]);
	if(menu_get_current()->len > 3)
	{
		assert_int_equal(4, menu_get_current()->len);
		/* Separator. */
		assert_string_equal("", menu_get_current()->items[3]);
	}

	(void)vle_keys_exec(WK_ESC);
}

TEST(file_can_hide_missing_entries)
{
	cfg.hide_missing_progs = 1;

	setup_file_test();

	assert_success(cmds_dispatch1("file", &lwin, CIT_COMMAND));

	assert_true(menu_get_current()->len >= 1);
	assert_string_equal("[present] cat", menu_get_current()->items[0]);
	if(menu_get_current()->len > 1)
	{
		assert_int_equal(2, menu_get_current()->len);
		/* Separator. */
		assert_string_equal("", menu_get_current()->items[1]);
	}

	(void)vle_keys_exec(WK_ESC);

	cfg.hide_missing_progs = 0;
}

TEST(pseudo_entry_is_always_present_for_directories)
{
	assoc_programs("{bla-*/}", "abc-run %c", 0, 0);

	assert_success(cmds_dispatch("filetype bla-dir/", &lwin, CIT_COMMAND));

	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("[present] {Enter directory} " VIFM_PSEUDO_CMD,
			menu_get_current()->items[0]);
	assert_string_equal("[present]                   abc-run %c",
			menu_get_current()->items[1]);

	(void)vle_keys_exec(WK_ESC);
}

TEST(no_menu_if_no_handlers)
{
	assert_failure(cmds_dispatch("filetype bla-file", &lwin, CIT_COMMAND));
}

TEST(filetypes_menu)
{
	assoc_programs("{a,b,c}", "abc-run %c", 0, 0);

	assert_success(cmds_dispatch("filetype b", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("[present] abc-run %c", menu_get_current()->items[0]);

	(void)vle_keys_exec(WK_ESC);
}

TEST(fileviewers_menu)
{
	assoc_viewers("{a,b,c}", "abc-view %c");

	assert_success(cmds_dispatch("fileviewer c", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("[present] abc-view %c", menu_get_current()->items[0]);

	(void)vle_keys_exec(WK_ESC);
}

static int
fake_prog_exists(const char name[])
{
	return !starts_with_lit(name, "no-");
}

static void
setup_file_test(void)
{
	dir_entry_t *entry = get_current_entry(&lwin);

	update_string(&entry->name, "test-file");
	entry->type = FT_REG;

	assert_success(cmds_dispatch("filetype test-file no-cat", &lwin,
				CIT_COMMAND));
	assert_success(cmds_dispatch("filetype test-file cat", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("filetype test-file no-echo", &lwin,
				CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
